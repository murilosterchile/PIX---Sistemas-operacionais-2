#include "discovery.h"
#include "../common/utils.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <shared_mutex>
#include <arpa/inet.h>

DiscoveryService::DiscoveryService(uint16_t port, ServerData* data) 
    : port(port), server_data(data), running(false) {
    
    socket_fd = createUdpSocket();
    configureBroadcast(socket_fd);
    
    // SEM TIMEOUT AQUI (como solicitado, removemos a correção do deadlock)
    
    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (bind(socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(socket_fd);
        throw std::runtime_error("Erro no bind do socket de descoberta");
    }
}

DiscoveryService::~DiscoveryService() {
    stop();
    if (socket_fd >= 0) {
        close(socket_fd);
    }
}

void DiscoveryService::start() {
    running = true;
    listener_thread = std::thread(&DiscoveryService::listenForDiscovery, this);
    std::cout << "DiscoveryService iniciado na porta " << port << std::endl;
}

void DiscoveryService::stop() {
    running = false;
    if (listener_thread.joinable()) {
        listener_thread.join();
    }
}

void DiscoveryService::discoverPeers() {
    std::cout << "[DISCOVERY] Varrendo portas 4000-4050 para encontrar peers..." << std::endl;
    
    packet_t hello;
    init_packet(&hello, SERVER_HELLO, 0);
    hello.payload.server.id = server_data->config->my_id;
    hello.payload.server.port = server_data->config->main_port;
    hello.payload.server.repl_port = server_data->config->repl_port;
    packet_host_to_net(&hello);
    
    for (uint16_t p = 4000; p <= 4050; p += 10) {
        if (p == port) continue;
        
        sockaddr_in dest;
        memset(&dest, 0, sizeof(dest));
        dest.sin_family = AF_INET;
        dest.sin_port = htons(p);
        inet_aton("127.0.0.1", &dest.sin_addr);
        
        sendto(socket_fd, &hello, sizeof(hello), 0, (struct sockaddr*)&dest, sizeof(dest));
    }
}

void DiscoveryService::listenForDiscovery() {
    char buffer[PACKET_SIZE];
    sockaddr_in sender;
    socklen_t len = sizeof(sender);
    
    while (running) {
        // Bloqueante (sem timeout)
        ssize_t recv_len = recvfrom(socket_fd, buffer, sizeof(buffer), 0, (struct sockaddr*)&sender, &len);
            
        if (recv_len == PACKET_SIZE) {
            packet_t packet;
            memcpy(&packet, buffer, sizeof(packet_t));
            packet_net_to_host(&packet);
            
            PacketType type = static_cast<PacketType>(packet.type);
            
            if (type == DESCOBERTA && server_data->config->status == PRIMARY) {
                handleDiscoveryRequest(sender);
            }
            else if (type == SERVER_HELLO || type == SERVER_WELCOME) {
                handleServerMessage(packet, sender);
            }
        }
    }
}

void DiscoveryService::handleServerMessage(const packet_t& packet, const sockaddr_in& sender_addr) {
    uint32_t pid = packet.payload.server.id;
    if (pid == server_data->config->my_id) return;

    std::string ip = ipToString(sender_addr.sin_addr.s_addr);
    uint16_t p_port = packet.payload.server.port;
    uint16_t r_port = packet.payload.server.repl_port;

    addPeerIfNew(pid, ip, p_port, r_port);
    
    if (static_cast<PacketType>(packet.type) == SERVER_HELLO) {
        packet_t welcome;
        init_packet(&welcome, SERVER_WELCOME, 0);
        welcome.payload.server.id = server_data->config->my_id;
        welcome.payload.server.port = server_data->config->main_port;
        welcome.payload.server.repl_port = server_data->config->repl_port;
        packet_host_to_net(&welcome);
        
        sockaddr_in dest = sender_addr;
        dest.sin_port = htons(p_port); 
        sendto(socket_fd, &welcome, sizeof(welcome), 0, (struct sockaddr*)&dest, sizeof(dest));
    }
}

void DiscoveryService::addPeerIfNew(uint32_t id, const std::string& ip, uint16_t port, uint16_t repl_port) {
    std::unique_lock<std::shared_mutex> lock(server_data->rw_mutex);
    for (const auto& peer : server_data->config->peers) {
        if (peer.server_id == id) return;
    }
    server_data->config->peers.emplace_back(id, ip, port, repl_port);
    std::cout << "[DISCOVERY] Peer adicionado: ID=" << id << " Porta=" << port << std::endl;
}

void DiscoveryService::handleDiscoveryRequest(const sockaddr_in& client_addr) {
    uint32_t client_ip = client_addr.sin_addr.s_addr;
    std::cout << "Registrando cliente " << ipToString(client_ip) << std::endl;
    
    std::unique_lock<std::shared_mutex> lock(server_data->rw_mutex);
    auto it = server_data->clients.find(client_ip);
    if (it == server_data->clients.end()) {
        server_data->clients[client_ip] = ClientInfo(client_ip);
        server_data->total_balance += 100;
        server_data->has_update = true;
    }
    lock.unlock();
    
    if (server_data->has_update) server_data->data_updated.notify_all();
    sendDiscoveryResponse(client_addr);
}

void DiscoveryService::sendDiscoveryResponse(const sockaddr_in& client_addr) {
    packet_t response;
    init_packet(&response, DESCOBERTA_ACK, 0);
    response.payload.disc_ack.server_addr = htonl(INADDR_ANY);
    response.payload.disc_ack.server_port = htons(port);
    response.payload.disc_ack.accepted = 1;
    packet_host_to_net(&response);
    sendto(socket_fd, &response, sizeof(response), 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
}