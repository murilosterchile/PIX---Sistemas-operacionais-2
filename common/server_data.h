#ifndef SERVER_DATA_H
#define SERVER_DATA_H

#include <unordered_map>
#include <shared_mutex>
#include <condition_variable>
#include <cstdint>
#include <string>

struct ClientInfo {
    uint32_t address;     // IP do cliente (network order)
    uint32_t last_req;    // Último ID de req processado
    uint32_t balance;     // Saldo atual em reais
    
    ClientInfo() : address(0), last_req(0), balance(100) {}
    ClientInfo(uint32_t addr) : address(addr), last_req(0), balance(100) {}
};

struct ServerData {
    // Dados dos clientes
    std::unordered_map<uint32_t, ClientInfo> clients;
    
    uint32_t num_transactions;
    uint32_t total_transferred;
    uint32_t total_balance;
    
    std::shared_mutex rw_mutex;
    std::condition_variable_any data_updated;
    bool has_update;
    
    ServerData() : num_transactions(0), total_transferred(0), 
                   total_balance(0), has_update(false) {}
};

#endif
