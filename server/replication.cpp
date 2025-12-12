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
    
    // Enviar para cada backup
    for (const auto& peer : server_data->config->peers) {
        if (peer.is_alive) {
            sendStateToBackup(peer);
        }
    }
}

void ReplicationService::sendStateToBackup(const PeerInfo& peer) {
    // esse lock tava dando erro
    // std::shared_lock<std::shared_mutex> lock(server_data->rw_mutex);
    
    // faz o pacote de update
    packet_t update_packet;
    init_packet(&update_packet, STATE_UPDATE, 0);
    
    // preenche dados
    // Acesso seguro pois handleRequestThread mantém o lock de escrita
    update_packet.payload.state.num_transactions = server_data->num_transactions;
    update_packet.payload.state.total_transferred = server_data->total_transferred;
    update_packet.payload.state.total_balance = server_data->total_balance;
    update_packet.payload.state.num_clients = server_data->clients.size();
    
    // ja que não tem o lock, não precisa do unlock
    // lock.unlock();
    
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
        // Timeout para verificar running periodicamente
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(socket_fd, &readfds);
        
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int activity = select(socket_fd + 1, &readfds, NULL, NULL, &timeout);
        
        if (activity < 0) continue;
        if (activity == 0) continue; // timeout
        
        ssize_t recv_len = recvfrom(socket_fd, buffer, sizeof(buffer), 0,
                                   (struct sockaddr*)&sender_addr, &sender_len);
        
        if (recv_len == PACKET_SIZE) {
            packet_t packet;
            memcpy(&packet, buffer, sizeof(packet_t));
            packet_net_to_host(&packet);
            
            if (static_cast<PacketType>(packet.type) == STATE_UPDATE) {
                std::cout << "[REPLICATION] Recebido update de " 
                          << ipToString(sender_addr.sin_addr.s_addr) << std::endl;
                handleStateUpdate(packet);
            }
        }
    }
}

void ReplicationService::handleStateUpdate(const packet_t& packet) {
    // somente aceita updates se for backup
    if (server_data->config->status != BACKUP) {
        return;
    }
    
    std::unique_lock<std::shared_mutex> lock(server_data->rw_mutex);
    
    // aplica o estado recebido
    server_data->num_transactions = packet.payload.state.num_transactions;
    server_data->total_transferred = packet.payload.state.total_transferred;
    server_data->total_balance = packet.payload.state.total_balance;
    
    std::cout << "[REPLICATION] Estado aplicado: tx=" << server_data->num_transactions
              << " total_transferred=" << server_data->total_transferred
              << " total_balance=" << server_data->total_balance << std::endl;
    
    // manda um sinal de atualização para interface
    server_data->has_update = true;
    
    lock.unlock();
    server_data->data_updated.notify_all();
}