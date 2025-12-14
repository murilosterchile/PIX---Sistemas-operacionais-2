#include "discovery.h"
#include "../common/utils.h"
#include <cstdint>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <shared_mutex>

DiscoveryService::DiscoveryService(uint16_t port, ServerData* data) 
    : port(port), server_data(data), running(false) {
    
    socket_fd = createUdpSocket();
    configureBroadcast(socket_fd);
    
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

void DiscoveryService::listenForDiscovery() {
    char buffer[PACKET_SIZE];
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    std::cout << "Aguardando mensagens de descoberta..." << std::endl;
    
    while (running) {
	ssize_t recv_len = recvfrom(socket_fd, buffer, sizeof(buffer), 0,
                        (struct sockaddr*)&client_addr, &client_len);
            
            if (recv_len > 0) {
                std::cout << "Recebida mensagem de " << ipToString(client_addr.sin_addr.s_addr) 
                          << " (tamanho: " << recv_len << ")" << std::endl;
                          
                if (recv_len == PACKET_SIZE) {
                    packet_t packet;
                    memcpy(&packet, buffer, sizeof(packet_t));
                    packet_net_to_host(&packet);
                    
                    std::cout << "Tipo de pacote: " << packet.type << std::endl;
                    
                    if (static_cast<PacketType>(packet.type) == DESCOBERTA && server_data->config->status == PRIMARY) {
                        std::cout << "Processando descoberta de " << ipToString(client_addr.sin_addr.s_addr) << std::endl;
                        handleDiscoveryRequest(client_addr);
                    }
                    if (static_cast<PacketType>(packet.type) == DESCOBERTA_SERVIDOR || static_cast<PacketType>(packet.type) == DESCOBERTA_SERVIDOR_ACK ) {
                        std::cout << "Processando descoberta de " << ipToString(client_addr.sin_addr.s_addr) << std::endl;
                        handleServerDiscoveryRequest(client_addr, packet.payload.server.new_server_port, packet.payload.server.new_server_id);
                    }
		    if(static_cast<PacketType>(packet.type) == DESCOBERTA_SERVIDOR){
    			sendServerDiscoveryResponse(client_addr);
		    }
                } else {
                    std::cout << "Pacote com tamanho incorreto: " << recv_len << " (esperado: " << PACKET_SIZE << ")" << std::endl;
                }
	    }
    }
}

void DiscoveryService::handleDiscoveryRequest(const sockaddr_in& client_addr) {
    uint32_t client_ip = client_addr.sin_addr.s_addr;
    
    std::cout << "Registrando cliente " << ipToString(client_ip) << std::endl;
    
    // ESCRITOR - Adicionar cliente se não existe
    std::unique_lock<std::shared_mutex> lock(server_data->rw_mutex);
    
    auto it = server_data->clients.find(client_ip);
    if (it == server_data->clients.end()) {
        // Novo cliente
        server_data->clients[client_ip] = ClientInfo(client_ip);
        server_data->total_balance += 100;  // Saldo inicial
        
        std::cout << getCurrentTimestamp() 
                  << " new client " << ipToString(client_ip)
                  << " balance 100" << std::endl;
        
        // Marcar que houve atualização
        server_data->has_update = true;
    }else{
        server_data->clients[client_ip].last_req = 0;
    }
    
    lock.unlock();
    
    // Notificar interface apos liberar o lock
    if (server_data->has_update) {
        server_data->data_updated.notify_all();
    }

    sendDiscoveryResponse(client_addr);
}

void DiscoveryService::sendDiscoveryResponse(const sockaddr_in& client_addr) {
    packet_t response;
    init_packet(&response, DESCOBERTA_ACK, 0);

    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    
    response.payload.disc_ack.server_addr = htonl(INADDR_ANY);
    response.payload.disc_ack.server_port = htons(port);
    response.payload.disc_ack.accepted = 1;
    
    packet_host_to_net(&response);
    
    ssize_t sent = sendto(socket_fd, &response, sizeof(response), 0,
                         (struct sockaddr*)&client_addr, sizeof(client_addr));
    
    std::cout << "Enviada resposta para " << ipToString(client_addr.sin_addr.s_addr) 
              << " (bytes enviados: " << sent << ")" << std::endl;
}

void DiscoveryService::sendServerBroadcast() {
    packet_t discovery_packet;
    init_packet(&discovery_packet, DESCOBERTA_SERVIDOR, 0);
    
    discovery_packet.payload.server.new_server_port = server_data->config->main_port;
    discovery_packet.payload.server.new_server_id = server_data->config->my_id;

    std::cout << "Criando pacote de descoberta de servidor..." << std::endl;
    std::cout << "Tipo: " << static_cast<uint16_t>(DESCOBERTA_SERVIDOR) << std::endl;
    std::cout << "Tamanho do pacote: " << sizeof(discovery_packet) << std::endl;
    
    packet_host_to_net(&discovery_packet);
    
    // Tentar múltiplos endereços de broadcast
    std::vector<std::string> broadcast_addrs = {
        "255.255.255.255",  // Broadcast global
        "127.255.255.255",  // Localhost broadcast  
        "192.168.255.255",  // Broadcast típico de rede local
        "10.255.255.255"    // Outro broadcast comum
    };
    
    for (const auto& broadcast_addr : broadcast_addrs) {
        sockaddr_in broadcast_sockaddr;
        memset(&broadcast_sockaddr, 0, sizeof(broadcast_sockaddr));
        broadcast_sockaddr.sin_family = AF_INET;
        broadcast_sockaddr.sin_port = htons(port);
        
        if (inet_aton(broadcast_addr.c_str(), &broadcast_sockaddr.sin_addr)) {
            ssize_t sent = sendto(socket_fd, &discovery_packet, sizeof(discovery_packet), 0,
                                 (struct sockaddr*)&broadcast_sockaddr, sizeof(broadcast_sockaddr));
            
            std::cout << "Enviado broadcast para " << broadcast_addr 
                      << " (bytes enviados: " << sent << ")" << std::endl;
        }
    }
    
    // ADICIONALMENTE - tentar localhost direto (para teste)
    sockaddr_in localhost_addr;
    memset(&localhost_addr, 0, sizeof(localhost_addr));
    localhost_addr.sin_family = AF_INET;
    localhost_addr.sin_port = htons(port);
    inet_aton("127.0.0.1", &localhost_addr.sin_addr);
    
    ssize_t sent = sendto(socket_fd, &discovery_packet, sizeof(discovery_packet), 0,
                         (struct sockaddr*)&localhost_addr, sizeof(localhost_addr));
    
    std::cout << "Enviado direto para localhost (bytes enviados: " << sent << ")" << std::endl;
}

void DiscoveryService::handleServerDiscoveryRequest(const sockaddr_in& client_addr, uint16_t peer_port, uint16_t peer_id) {
    uint32_t client_ip = client_addr.sin_addr.s_addr;
    
    std::cout << "Registrando peer " << ipToString(client_ip) << std::endl;
    
    // ESCRITOR - Adicionar cliente se não existe
    std::unique_lock<std::shared_mutex> lock(server_data->rw_mutex);

    bool peer_exists = false;

    for (const auto& peer : server_data->config->peers) {
        if (peer.ip == ipToString(client_ip)) {  // Assuming 'peer.ip' holds the client's IP.
            peer_exists = true;
            break;
        }
    }

    if (!peer_exists) {
        // Novo cliente
        server_data->config->peers.emplace_back(peer_id, ipToString(client_ip), peer_port, peer_port + 100);
        
        std::cout << getCurrentTimestamp() 
                  << " new peer " << ipToString(client_ip)
                  << std::endl;
        
        // Marcar que houve atualização
        server_data->has_update = true;
    }
    
    lock.unlock();
    
    // Notificar interface apos liberar o lock
    if (server_data->has_update) {
        server_data->data_updated.notify_all();
    }
}

void DiscoveryService::sendServerDiscoveryResponse(const sockaddr_in& client_addr){
    packet_t response;
    init_packet(&response, DESCOBERTA_SERVIDOR_ACK, 0);

    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    
    response.payload.server.new_server_port = server_data->config->main_port;
    response.payload.server.new_server_id = server_data->config->my_id;
    
    packet_host_to_net(&response);
    
    ssize_t sent = sendto(socket_fd, &response, sizeof(response), 0,
                         (struct sockaddr*)&client_addr, sizeof(client_addr));
    
    std::cout << "Enviada resposta para " << ipToString(client_addr.sin_addr.s_addr) 
              << " (bytes enviados: " << sent << ")" << std::endl;
}
