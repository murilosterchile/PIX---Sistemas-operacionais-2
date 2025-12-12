#include "election.h"
#include "../common/utils.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>

ElectionService::ElectionService(uint16_t port, ServerData* data)
    : election_port(port + 200), server_data(data), running(false), 
      election_in_progress(false), is_sending_heartbeat(false) {
    
    socket_fd = createUdpSocket();
    
    // Bind na porta de eleição
    sockaddr_in election_addr;
    memset(&election_addr, 0, sizeof(election_addr));
    election_addr.sin_family = AF_INET;
    election_addr.sin_addr.s_addr = INADDR_ANY;
    election_addr.sin_port = htons(election_port);
    
    if (bind(socket_fd, (struct sockaddr*)&election_addr, sizeof(election_addr)) < 0) {
        close(socket_fd);
        throw std::runtime_error("Erro no bind do socket de eleição");
    }
    
    last_heartbeat = std::chrono::steady_clock::now();
}

ElectionService::~ElectionService() {
    stop();
    if (socket_fd >= 0) {
        close(socket_fd);
    }
}

void ElectionService::start() {
    running = true;
    listener_thread = std::thread(&ElectionService::listenForElectionMessages, this);
    
    // Se for primário, envia heartbeat
    if (server_data->config->status == PRIMARY) {
        is_sending_heartbeat = true;
        heartbeat_thread = std::thread(&ElectionService::sendHeartbeatLoop, this);
    } else {
        // Se for backup, monitora heartbeat
        is_sending_heartbeat = false;
        heartbeat_thread = std::thread(&ElectionService::monitorHeartbeat, this);
    }
    
    std::cout << "[ELECTION] ElectionService iniciado na porta " << election_port << std::endl;
}

void ElectionService::stop() {
    running = false;
    if (listener_thread.joinable()) {
        listener_thread.join();
    }
    if (heartbeat_thread.joinable()) {
        heartbeat_thread.join();
    }
}

void ElectionService::listenForElectionMessages() {
    char buffer[UDP_BUFFER_SIZE];
    sockaddr_in sender_addr;
    socklen_t sender_len = sizeof(sender_addr);
    
    while (running) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(socket_fd, &readfds);
        
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int activity = select(socket_fd + 1, &readfds, NULL, NULL, &timeout);
        if (activity <= 0) continue;
        
        ssize_t recv_len = recvfrom(socket_fd, buffer, sizeof(buffer), 0,
                                   (struct sockaddr*)&sender_addr, &sender_len);
        
        if (recv_len == PACKET_SIZE) {
            packet_t packet;
            memcpy(&packet, buffer, sizeof(packet_t));
            packet_net_to_host(&packet);
            
            PacketType type = static_cast<PacketType>(packet.type);
            
            switch (type) {
                case ELECTION:
                    handleElectionMessage(packet, sender_addr);
                    break;
                case OK:
                    handleOkMessage(packet, sender_addr);
                    break;
                case COORDINATOR:
                    handleCoordinatorMessage(packet, sender_addr);
                    break;
                case HEARTBEAT:
                    handleHeartbeat(packet, sender_addr);
                    break;
                case HEARTBEAT_ACK:
                    handleHeartbeatAck(packet, sender_addr);
                    break;
                default:
                    break;
            }
        }
    }
}

void ElectionService::monitorHeartbeat() {
    while (running) {
        // Se virou primário, começar a enviar heartbeat
        if (is_sending_heartbeat) {
            sendHeartbeatLoop();
            break; // Sair do loop de monitoramento
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Só monitora se for backup
        if (server_data->config->status != BACKUP) {
            last_heartbeat = std::chrono::steady_clock::now();
            continue;
        }
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_heartbeat).count();
        
        if (elapsed > HEARTBEAT_TIMEOUT_MS && !election_in_progress) {
            std::cout << "[ELECTION] Primário não responde (timeout: " << elapsed 
                      << "ms)! Iniciando eleição..." << std::endl;
            startElection();
            // Aguardar um pouco antes de verificar novamente
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
}

void ElectionService::sendHeartbeatLoop() {
    while (running && is_sending_heartbeat) {
        std::this_thread::sleep_for(std::chrono::milliseconds(HEARTBEAT_INTERVAL_MS));
        
        // Só envia se for primário
        if (server_data->config->status == PRIMARY) {
            sendHeartbeat();
        }
    }
}

void ElectionService::startElection() {
    if (election_in_progress) {
        return; // Já está em processo de eleição
    }
    
    election_in_progress = true;
    std::cout << "[ELECTION] Servidor " << server_data->config->my_id 
              << " iniciando eleição" << std::endl;
    
    bool has_higher = false;
    for (const auto& peer : server_data->config->peers) {
        if (peer.server_id > server_data->config->my_id) {
            has_higher = true;
            break;
        }
    }
    
    if (!has_higher) {
        // "não tem ninguém com ID maior, sou o coordenador"
        becomeCoordinator();
        return;
    }
    
    sendElectionToHigher();
    
    // Aguardar respostas, com timeout de 2s
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    
    // 'Se ainda em progresso, ninguém respondeu, eu sou o coordenador'
    if (election_in_progress) {
        becomeCoordinator();
    }
}

void ElectionService::sendElectionToHigher() {
    packet_t election_packet;
    init_packet(&election_packet, ELECTION, server_data->config->my_id);
    packet_host_to_net(&election_packet);
    
    for (const auto& peer : server_data->config->peers) {
        if (peer.server_id > server_data->config->my_id) {
            sockaddr_in peer_addr;
            memset(&peer_addr, 0, sizeof(peer_addr));
            peer_addr.sin_family = AF_INET;
            inet_aton(peer.ip.c_str(), &peer_addr.sin_addr);
            peer_addr.sin_port = htons(peer.port + 200); // Porta de eleição do peer
            
            sendto(socket_fd, &election_packet, sizeof(election_packet), 0,
                   (struct sockaddr*)&peer_addr, sizeof(peer_addr));
            
            std::cout << "[ELECTION] Enviado ELECTION para servidor " << peer.server_id << std::endl;
        }
    }
}

void ElectionService::handleElectionMessage(const packet_t& packet, const sockaddr_in& sender) {
    std::cout << "[ELECTION] Recebido ELECTION de " << ipToString(sender.sin_addr.s_addr) << std::endl;
    
    // responde com OK
    sendOkMessage(sender);
    
    // 'se não estou em eleição, iniciar minha própria'
    if (!election_in_progress) {
        startElection();
    }
}

void ElectionService::sendOkMessage(const sockaddr_in& dest) {
    packet_t ok_packet;
    init_packet(&ok_packet, OK, server_data->config->my_id);
    packet_host_to_net(&ok_packet);
    
    sendto(socket_fd, &ok_packet, sizeof(ok_packet), 0,
           (struct sockaddr*)&dest, sizeof(dest));
    
    std::cout << "[ELECTION] Enviado OK" << std::endl;
}

void ElectionService::handleOkMessage(const packet_t& packet, const sockaddr_in& sender) {
    std::cout << "[ELECTION] Recebido OK de " << ipToString(sender.sin_addr.s_addr) 
              << " - Cancelando eleição (há servidor com ID maior)" << std::endl;
    election_in_progress = false;  // Alguém maior assumiu
}

void ElectionService::becomeCoordinator() {
    std::cout << "[ELECTION] Servidor " << server_data->config->my_id 
              << " se tornando COORDENADOR!" << std::endl;
    
    server_data->config->status = PRIMARY;
    election_in_progress = false;
    is_sending_heartbeat = true;
    
    announceCoordinator();
    
    std::cout << "[ELECTION] Agora enviando heartbeat como primário" << std::endl;
}

void ElectionService::announceCoordinator() {
    packet_t coord_packet;
    init_packet(&coord_packet, COORDINATOR, server_data->config->my_id);
    packet_host_to_net(&coord_packet);
    
    for (const auto& peer : server_data->config->peers) {
        sockaddr_in peer_addr;
        memset(&peer_addr, 0, sizeof(peer_addr));
        peer_addr.sin_family = AF_INET;
        inet_aton(peer.ip.c_str(), &peer_addr.sin_addr);
        peer_addr.sin_port = htons(peer.port + 200); // Porta de eleição do peer
        
        sendto(socket_fd, &coord_packet, sizeof(coord_packet), 0,
               (struct sockaddr*)&peer_addr, sizeof(peer_addr));
    }
    
    std::cout << "[ELECTION] Anunciado como COORDINATOR para todos" << std::endl;
}

void ElectionService::handleCoordinatorMessage(const packet_t& packet, const sockaddr_in& sender) {
    std::cout << "[ELECTION] Novo coordenador: servidor " << packet.seqn 
              << " (" << ipToString(sender.sin_addr.s_addr) << ")" << std::endl;
    
    server_data->config->status = BACKUP;
    election_in_progress = false;
    last_heartbeat = std::chrono::steady_clock::now();
}

void ElectionService::sendHeartbeat() {
    packet_t hb_packet;
    init_packet(&hb_packet, HEARTBEAT, server_data->config->my_id);
    packet_host_to_net(&hb_packet);
    
    for (const auto& peer : server_data->config->peers) {
        sockaddr_in peer_addr;
        memset(&peer_addr, 0, sizeof(peer_addr));
        peer_addr.sin_family = AF_INET;
        inet_aton(peer.ip.c_str(), &peer_addr.sin_addr);
        peer_addr.sin_port = htons(peer.port + 200); // Porta de eleição do peer
        
        sendto(socket_fd, &hb_packet, sizeof(hb_packet), 0,
               (struct sockaddr*)&peer_addr, sizeof(peer_addr));
    }
    
    // Log a cada 10 heartbeats
    static int hb_count = 0;
    if (++hb_count % 10 == 0) {
        std::cout << "[ELECTION] Heartbeat enviado para " << server_data->config->peers.size() 
                  << " peers" << std::endl;
    }
}

void ElectionService::handleHeartbeat(const packet_t& packet, const sockaddr_in& sender) {
    last_heartbeat = std::chrono::steady_clock::now();
    
    // Responder com ACK
    packet_t ack_packet;
    init_packet(&ack_packet, HEARTBEAT_ACK, server_data->config->my_id);
    packet_host_to_net(&ack_packet);
    
    sendto(socket_fd, &ack_packet, sizeof(ack_packet), 0,
           (struct sockaddr*)&sender, sizeof(sender));
}

void ElectionService::handleHeartbeatAck(const packet_t& packet, const sockaddr_in& sender) {
    // Primário recebe acks, apenas para confirmar que backup está vivo
}