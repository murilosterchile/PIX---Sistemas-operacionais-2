#include "election.h"
#include "../common/utils.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>

ElectionService::ElectionService(uint16_t port, ServerData* data)
    : election_port(port + 200), server_data(data), running(false), 
      election_in_progress(false), is_sending_heartbeat(false) {
    
    socket_fd = createUdpSocket();
    notification_sock = createUdpSocket();  // Socket separado para notificações
    
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
    if (notification_sock >= 0) {
        close(notification_sock);
    }
}

void ElectionService::start() {
    running = true;
    listener_thread = std::thread(&ElectionService::listenForElectionMessages, this);
    
    // Se for primário, seta a flag para o loop saber o que fazer
    if (server_data->config->status == PRIMARY) {
        is_sending_heartbeat = true;
    } else {
        is_sending_heartbeat = false;
    }

    // CORREÇÃO: Sempre inicia pela monitorHeartbeat. 
    // Ela saberá chamar a sendHeartbeatLoop se is_sending_heartbeat for true.
    heartbeat_thread = std::thread(&ElectionService::monitorHeartbeat, this);
    
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
            
            // CORREÇÃO: Removido o 'break'. 
            // Se sendHeartbeatLoop retornar (pq deixou de ser lider),
            // atualizamos o last_heartbeat e continuamos monitorando.
            last_heartbeat = std::chrono::steady_clock::now();
            continue; 
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Só monitora se for backup
        if (server_data->config->status != BACKUP) {
            // ... (código existente mantido)
            last_heartbeat = std::chrono::steady_clock::now();
            continue;
        }
        
        // ... (código de verificação de timeout existente mantido) ...
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_heartbeat).count();
        
        if (elapsed > HEARTBEAT_TIMEOUT_MS && !election_in_progress) {
             // ... (código existente mantido)
             std::cout << "[ELECTION] Primário não responde (timeout: " << elapsed 
                      << "ms)! Iniciando eleição..." << std::endl;
            startElection();
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
        }else{
	    is_sending_heartbeat = false;
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
    
    // notifica clientes sobre a mudança de líder
    notifyClientsOfLeaderChange();
    
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
    // CORREÇÃO: Verificar conflito de liderança
    // O seqn do pacote de heartbeat carrega o ID do remetente
    uint32_t sender_id = packet.seqn; 
    
    // Se eu sou PRIMARY, mas recebo heartbeat de um ID maior, devo renunciar
    if (server_data->config->status == PRIMARY && sender_id > server_data->config->my_id) {
        std::cout << "[ELECTION] Conflito detectado! Recebido Heartbeat de ID maior (" 
                  << sender_id << "). Renunciando liderança..." << std::endl;
        
        server_data->config->status = BACKUP;
        is_sending_heartbeat = false;
        // A thread sendHeartbeatLoop vai encerrar e voltará para monitorHeartbeat
    }

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

void ElectionService::notifyClientsOfLeaderChange() {
    std::cout << "[ELECTION] Notificando clientes sobre novo líder..." << std::endl;
    
    std::shared_lock<std::shared_mutex> lock(server_data->rw_mutex);
    
    // Descobrir o IP real da máquina
    uint32_t my_ip = 0;
    struct ifaddrs *ifaddrs_ptr;
    
    if (getifaddrs(&ifaddrs_ptr) == 0) {
        for (struct ifaddrs *ifa = ifaddrs_ptr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
                struct sockaddr_in *addr_in = (struct sockaddr_in *)ifa->ifa_addr;
                uint32_t ip = addr_in->sin_addr.s_addr;
                
                // Ignorar localhost (127.0.0.1)
                if (ip != htonl(INADDR_LOOPBACK)) {
                    my_ip = ip;
                    std::cout << "[ELECTION] IP do servidor: " << ipToString(ip) << std::endl;
                    break;  // Pega o primeiro IP não-localhost
                }
            }
        }
        freeifaddrs(ifaddrs_ptr);
    }
    
    // Se não encontrou, usa o localhost como fallback
    if (my_ip == 0) {
        inet_aton("127.0.0.1", (struct in_addr*)&my_ip);
        std::cout << "[ELECTION] Usando localhost como fallback" << std::endl;
    }
    
    // Pegar informações do novo líder (eu mesmo)
    packet_t leader_packet;
    init_packet(&leader_packet, LEADER_CHANGE, server_data->config->my_id);
    
    leader_packet.payload.leader.new_leader_id = server_data->config->my_id;
    leader_packet.payload.leader.new_leader_ip = my_ip;
    leader_packet.payload.leader.new_leader_port = server_data->config->main_port;
    
    packet_host_to_net(&leader_packet);
    
    // enviar unicast para cada cliente conectado na porta de notificações
    int notified = 0;
    for (const auto& client_pair : server_data->clients) {
        uint32_t client_ip = client_pair.first;
        
        sockaddr_in client_notify_addr;
        memset(&client_notify_addr, 0, sizeof(client_notify_addr));
        client_notify_addr.sin_family = AF_INET;
        client_notify_addr.sin_addr.s_addr = client_ip;
        client_notify_addr.sin_port = htons(40010);  // Porta fixa global para notificações
        
        ssize_t sent = sendto(notification_sock, &leader_packet, sizeof(leader_packet), 0,
                             (struct sockaddr*)&client_notify_addr, sizeof(client_notify_addr));
        
        if (sent > 0) {
            notified++;
            std::cout << "[ELECTION] Notificação enviada para cliente " 
                      << ipToString(client_ip) << ":40010"
                      << " (bytes: " << sent << ")" << std::endl;
        } else {
            perror("[ELECTION] ERRO ao enviar");
            std::cout << "[ELECTION] FALHA ao enviar para " 
                      << ipToString(client_ip) << " (errno: " << errno << ")" << std::endl;
        }
    
    std::cout << "[ELECTION] Notificação enviada para " << notified << " clientes" << std::endl;
}

}
