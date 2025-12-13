#include "replication.h"
#include "../common/utils.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <shared_mutex>

ReplicationService::ReplicationService(uint16_t port, ServerData* data)
    : repl_port(port), server_data(data), running(false) {
    
    socket_fd = createUdpSocket();
    
    // Bind na porta de replicação
    sockaddr_in repl_addr;
    memset(&repl_addr, 0, sizeof(repl_addr));
    repl_addr.sin_family = AF_INET;
    repl_addr.sin_addr.s_addr = INADDR_ANY;
    repl_addr.sin_port = htons(repl_port);
    
    if (bind(socket_fd, (struct sockaddr*)&repl_addr, sizeof(repl_addr)) < 0) {
        close(socket_fd);
        throw std::runtime_error("Erro no bind do socket de replicação");
    }
}

ReplicationService::~ReplicationService() {
    stop();
    if (socket_fd >= 0) {
        close(socket_fd);
    }
}

void ReplicationService::start() {
    running = true;
    listener_thread = std::thread(&ReplicationService::listenForUpdates, this);
    std::cout << "ReplicationService iniciado na porta " << repl_port << std::endl;
    
    // Se sou backup, peço os dados atuais imediatamente
    if (server_data->config->status == BACKUP) {
        requestSync();
    }
}

void ReplicationService::stop() {
    running = false;
    if (listener_thread.joinable()) {
        listener_thread.join();
    }
}

void ReplicationService::propagateState() {
    // Só propaga se for primário
    if (server_data->config->status != PRIMARY) {
        return;
    }
    
    std::cout << "[REPLICATION] Propagando estado para backups..." << std::endl;
    
    // Envia para cada backup
    for (const auto& peer : server_data->config->peers) {
        if (peer.is_alive) {
            sendStateToBackup(peer);
        }
    }
    
    // Agora propaga todos os clientes
    propagateAllClients();
}

void ReplicationService::sendStateToBackup(const PeerInfo& peer) {
    // REMOVIDO LOCK: O mutex já está trancado pela thread de processamento
    
    // faz o pacote de update
    packet_t update_packet;
    init_packet(&update_packet, STATE_UPDATE, 0);
    
    // preenche dados
    update_packet.payload.state.num_transactions = server_data->num_transactions;
    update_packet.payload.state.total_transferred = server_data->total_transferred;
    update_packet.payload.state.total_balance = server_data->total_balance;
    update_packet.payload.state.num_clients = server_data->clients.size();
    
    // converte para network order
    packet_host_to_net(&update_packet);
    
    // Enviar para o backup
    sockaddr_in peer_addr;
    memset(&peer_addr, 0, sizeof(peer_addr));
    peer_addr.sin_family = AF_INET;
    inet_aton(peer.ip.c_str(), &peer_addr.sin_addr);
    peer_addr.sin_port = htons(peer.repl_port);
    
    ssize_t sent = sendto(socket_fd, &update_packet, sizeof(update_packet), 0,
                         (struct sockaddr*)&peer_addr, sizeof(peer_addr));
    
    if (sent > 0) {
        std::cout << "[REPLICATION] Estado enviado para servidor " << peer.server_id 
                  << " (tx:" << server_data->num_transactions << ")" << std::endl;
    }
}

void ReplicationService::listenForUpdates() {
    char buffer[UDP_BUFFER_SIZE];
    sockaddr_in sender_addr;
    socklen_t sender_len = sizeof(sender_addr);
    
    std::cout << "[REPLICATION] Aguardando updates de estado..." << std::endl;
    
    while (running) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(socket_fd, &readfds);
        
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int activity = select(socket_fd + 1, &readfds, NULL, NULL, &timeout);
        
        if (activity < 0) continue;
        if (activity == 0) continue;
        
        ssize_t recv_len = recvfrom(socket_fd, buffer, sizeof(buffer), 0,
                                   (struct sockaddr*)&sender_addr, &sender_len);
        
        if (recv_len == PACKET_SIZE) {
            packet_t packet;
            memcpy(&packet, buffer, sizeof(packet_t));
            packet_net_to_host(&packet);
            
            PacketType type = static_cast<PacketType>(packet.type);
            
            if (type == STATE_UPDATE) {
                if (server_data->config->status == BACKUP) {
                    std::cout << "[REPLICATION] Recebido update de " 
                              << ipToString(sender_addr.sin_addr.s_addr) << std::endl;
                    handleStateUpdate(packet);
                }
            }
            else if (type == CLIENT_DATA) {
                if (server_data->config->status == BACKUP) {
                    handleClientData(packet);
                }
            }
            else if (type == SYNC_REQ) {
                handleSyncRequest(sender_addr);
            }
        }
    }
}

void ReplicationService::handleStateUpdate(const packet_t& packet) {
    if (server_data->config->status != BACKUP) {
        return;
    }
    
    std::unique_lock<std::shared_mutex> lock(server_data->rw_mutex);
    
    server_data->num_transactions = packet.payload.state.num_transactions;
    server_data->total_transferred = packet.payload.state.total_transferred;
    server_data->total_balance = packet.payload.state.total_balance;
    
    std::cout << "[REPLICATION] Estado aplicado: tx=" << server_data->num_transactions
              << " total_transferred=" << server_data->total_transferred
              << " total_balance=" << server_data->total_balance << std::endl;
    
    // marca como sincronizado quando recebe primeiro STATE_UPDATE
    if (!server_data->is_synchronized) {
        server_data->is_synchronized = true;
        std::cout << "[REPLICATION] Servidor sincronizado e pronto!" << std::endl;
    }
    
    server_data->has_update = true;
    
    lock.unlock();
    server_data->data_updated.notify_all();
}

void ReplicationService::requestSync() {
    packet_t req_packet;
    init_packet(&req_packet, SYNC_REQ, server_data->config->my_id);
    packet_host_to_net(&req_packet);
    
    std::cout << "[REPLICATION] Solicitando sincronização inicial aos peers..." << std::endl;
    
    for (const auto& peer : server_data->config->peers) {
        sockaddr_in peer_addr;
        memset(&peer_addr, 0, sizeof(peer_addr));
        peer_addr.sin_family = AF_INET;
        inet_aton(peer.ip.c_str(), &peer_addr.sin_addr);
        peer_addr.sin_port = htons(peer.repl_port);
        
        sendto(socket_fd, &req_packet, sizeof(req_packet), 0,
               (struct sockaddr*)&peer_addr, sizeof(peer_addr));
    }
}

void ReplicationService::handleSyncRequest(const sockaddr_in& sender) {
    if (server_data->config->status != PRIMARY) {
        return;
    }

    std::cout << "[REPLICATION] Recebido pedido de SYNC de " 
              << ipToString(sender.sin_addr.s_addr) << ". Enviando estado completo..." << std::endl;

    // 1. Envia dados agregados
    packet_t update_packet;
    init_packet(&update_packet, STATE_UPDATE, 0);
    
    {
        std::shared_lock<std::shared_mutex> lock(server_data->rw_mutex);
        update_packet.payload.state.num_transactions = server_data->num_transactions;
        update_packet.payload.state.total_transferred = server_data->total_transferred;
        update_packet.payload.state.total_balance = server_data->total_balance;
        update_packet.payload.state.num_clients = server_data->clients.size();
        
        packet_host_to_net(&update_packet);
        
        sendto(socket_fd, &update_packet, sizeof(update_packet), 0,
               (struct sockaddr*)&sender, sizeof(sender));
        
        // 2. Envia cada cliente individualmente
        std::cout << "[REPLICATION] Enviando " << server_data->clients.size() 
                  << " clientes..." << std::endl;
        
        for (const auto& client_pair : server_data->clients) {
            packet_t client_packet;
            init_packet(&client_packet, CLIENT_DATA, 0);
            
            client_packet.payload.cli_data.client_addr = client_pair.first;
            client_packet.payload.cli_data.balance = client_pair.second.balance;
            client_packet.payload.cli_data.last_req = client_pair.second.last_req;
            
            packet_host_to_net(&client_packet);
            
            sendto(socket_fd, &client_packet, sizeof(client_packet), 0,
                   (struct sockaddr*)&sender, sizeof(sender));
            
            // Pequeno delay para não sobrecarregar a rede
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
    
    std::cout << "[REPLICATION] Sincronização completa enviada para " 
              << ipToString(sender.sin_addr.s_addr) << std::endl;
}

void ReplicationService::propagateClientData(uint32_t client_ip, const ClientInfo& client) {
    if (server_data->config->status != PRIMARY) {
        return;
    }
    
    for (const auto& peer : server_data->config->peers) {
        if (peer.is_alive) {
            sendClientToBackup(peer, client_ip, client);
        }
    }
}

void ReplicationService::propagateAllClients() {
    if (server_data->config->status != PRIMARY) {
        return;
    }
    
    std::shared_lock<std::shared_mutex> lock(server_data->rw_mutex);
    
    std::cout << "[REPLICATION] Propagando " << server_data->clients.size() 
              << " clientes para backups..." << std::endl;
    
    for (const auto& client_pair : server_data->clients) {
        uint32_t client_ip = client_pair.first;
        const ClientInfo& client = client_pair.second;
        
        for (const auto& peer : server_data->config->peers) {
            if (peer.is_alive) {
                sendClientToBackup(peer, client_ip, client);
            }
        }
    }
}

void ReplicationService::sendClientToBackup(const PeerInfo& peer, uint32_t client_ip, const ClientInfo& client) {
    packet_t client_packet;
    init_packet(&client_packet, CLIENT_DATA, 0);
    
    client_packet.payload.cli_data.client_addr = client_ip;
    client_packet.payload.cli_data.balance = client.balance;
    client_packet.payload.cli_data.last_req = client.last_req;
    
    packet_host_to_net(&client_packet);
    
    sockaddr_in peer_addr;
    memset(&peer_addr, 0, sizeof(peer_addr));
    peer_addr.sin_family = AF_INET;
    inet_aton(peer.ip.c_str(), &peer_addr.sin_addr);
    peer_addr.sin_port = htons(peer.repl_port);
    
    sendto(socket_fd, &client_packet, sizeof(client_packet), 0,
           (struct sockaddr*)&peer_addr, sizeof(peer_addr));
}

void ReplicationService::handleClientData(const packet_t& packet) {
    if (server_data->config->status != BACKUP) {
        return;
    }
    
    std::unique_lock<std::shared_mutex> lock(server_data->rw_mutex);
    
    uint32_t client_ip = packet.payload.cli_data.client_addr;
    
    // Atualiza ou cria cliente
    auto it = server_data->clients.find(client_ip);
    if (it == server_data->clients.end()) {
        // Novo cliente
        server_data->clients[client_ip] = ClientInfo(client_ip);
    }
    
    server_data->clients[client_ip].balance = packet.payload.cli_data.balance;
    server_data->clients[client_ip].last_req = packet.payload.cli_data.last_req;
    
    std::cout << "[REPLICATION] Cliente " << ipToString(client_ip) 
              << " atualizado: balance=" << packet.payload.cli_data.balance
              << " last_req=" << packet.payload.cli_data.last_req << std::endl;
}