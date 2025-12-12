#ifndef SERVER_CONFIG_H
#define SERVER_CONFIG_H

#include <cstdint>
#include <string>
#include <vector>

// Status do servidor
enum ServerStatus {
    PRIMARY,    // Servidor primário
    BACKUP      // Servidor backup
};

// Informações de um peer
struct PeerInfo {
    uint32_t server_id;      // ID único do servidor
    std::string ip;          // IP do servidor
    uint16_t port;           // Porta principal
    uint16_t repl_port;      // Porta de replicação
    bool is_alive;           // Se está respondendo heartbeat
    
    PeerInfo(uint32_t id, const std::string& ip_addr, uint16_t p, uint16_t rp)
        : server_id(id), ip(ip_addr), port(p), repl_port(rp), is_alive(true) {}
};

// Configuração do servidor
struct ServerConfig {
    uint32_t my_id;              // ID deste servidor
    ServerStatus status;         // PRIMARY ou BACKUP
    uint16_t main_port;          // Porta principal (descoberta/processamento)
    uint16_t repl_port;          // Porta de replicação
    std::vector<PeerInfo> peers; // Lista de outros servidores
    
    ServerConfig(uint32_t id, uint16_t port)
        : my_id(id), status(BACKUP), main_port(port), repl_port(port + 100) {}
    
    // Retorna o servidor com maior id
    uint32_t getMaxPeerId() const {
        uint32_t max_id = my_id;
        for (const auto& peer : peers) {
            if (peer.server_id > max_id) {
                max_id = peer.server_id;
            }
        }
        return max_id;
    }
    
    // Verifica um servidor tem o maior id
    bool hasHighestId() const {
        return my_id == getMaxPeerId();
    }
};

#endif