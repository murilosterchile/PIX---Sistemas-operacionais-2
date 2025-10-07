#include "client_discovery.h"
#include "../common/utils.h"
#include "../common/debug.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <vector>

ClientDiscovery::ClientDiscovery(uint16_t port) : port(port) {
    socket_fd = createUdpSocket();
    configureBroadcast(socket_fd);
    setSocketTimeout(socket_fd, 1000); // 1 segundo timeout para debugging
}

ClientDiscovery::~ClientDiscovery() {
    if (socket_fd >= 0) {
        close(socket_fd);
    }
}

std::string ClientDiscovery::discoverServer() {
    const int MAX_ATTEMPTS = 3;
    
    // Mostrar informações de rede para debugging
    std::cout << "=== Informações de Rede ===" << std::endl;
    struct ifaddrs *ifaddrs_ptr;
    if (getifaddrs(&ifaddrs_ptr) == 0) {
        for (struct ifaddrs *ifa = ifaddrs_ptr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
                struct sockaddr_in *addr_in = (struct sockaddr_in *)ifa->ifa_addr;
                std::cout << "Interface: " << ifa->ifa_name 
                          << " IP: " << inet_ntoa(addr_in->sin_addr) << std::endl;
            }
        }
        freeifaddrs(ifaddrs_ptr);
    }
    std::cout << "========================" << std::endl;
    
    for (int attempt = 0; attempt < MAX_ATTEMPTS; attempt++) {
        std::cout << "Procurando servidor... (tentativa " 
                  << (attempt + 1) << "/" << MAX_ATTEMPTS << ")" << std::endl;
        
        sendDiscoveryBroadcast();
        
        std::string server_ip;
        if (waitForResponse(server_ip)) {
            return server_ip;
        }
        
        std::cout << "Tentativa " << (attempt + 1) << " falhou" << std::endl;
    }
    
    throw std::runtime_error("Servidor não encontrado após " + 
                            std::to_string(MAX_ATTEMPTS) + " tentativas");
}

void ClientDiscovery::sendDiscoveryBroadcast() {
    packet_t discovery_packet;
    init_packet(&discovery_packet, DESCOBERTA, 0);
    
    std::cout << "Criando pacote de descoberta..." << std::endl;
    std::cout << "Tipo: " << static_cast<uint16_t>(DESCOBERTA) << std::endl;
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

bool ClientDiscovery::waitForResponse(std::string& server_ip) {
    char buffer[PACKET_SIZE];
    sockaddr_in server_addr;
    socklen_t server_len = sizeof(server_addr);
    
    D_PRINT("Aguardando resposta...");
    
    ssize_t recv_len = recvfrom(socket_fd, buffer, sizeof(buffer), 0, (struct sockaddr*)&server_addr, &server_len);
    
    if (recv_len > 0) {
        D_PRINT("Recebida resposta de " << ipToString(server_addr.sin_addr.s_addr) << " (tamanho: " << recv_len << ")" );
                  
        if (recv_len == PACKET_SIZE) {
            packet_t response;
            memcpy(&response, buffer, sizeof(response));
            packet_net_to_host(&response);
            
            D_PRINT("Tipo de resposta: " << response.type);
            D_PRINT("Esperado: " << static_cast<uint16_t>(DESCOBERTA_ACK));
            
            if (static_cast<PacketType>(response.type) == DESCOBERTA_ACK) {
                if (response.payload.disc_ack.accepted == 1) {
                    server_ip = ipToString(server_addr.sin_addr.s_addr);
                    std::cout << "Servidor aceito: " << server_ip << std::endl;
                    return true;
                } else {
                    std::cout << "Servidor rejeitou conexão" << std::endl;
                }
            } else {
                std::cout << "Tipo de resposta inválido" << std::endl;
            }
        } else {
            std::cout << "Tamanho de resposta inválido" << std::endl;
        }
    } else {
        std::cout << "Timeout na resposta" << std::endl;
    }
    
    return false;
}
