#ifndef REPLICATION_H
#define REPLICATION_H

#include <thread>
#include <atomic>
#include <sys/socket.h>
#include <netinet/in.h>
#include "../common/protocol.h"
#include "../common/server_data.h"

class ReplicationService {
private:
    int socket_fd;
    uint16_t repl_port;
    ServerData* server_data;
    std::thread listener_thread;
    std::atomic<bool> running;
    
public:
    ReplicationService(uint16_t port, ServerData* data);
    ~ReplicationService();
    
    void start();
    void stop();
    
    // é chamado pelo primário após cada transação
    void propagateState();
    
    // propaga dados de um cliente específico para os backups
    void propagateClientData(uint32_t client_ip, const ClientInfo& client);
    
    // propaga todos os clientes para os backups
    void propagateAllClients();

    // método para pedir sincronização ao iniciar
    void requestSync();
    
private:
    // Thread que escuta updates, quando for um backup
    void listenForUpdates();
    
    // processa o update recebido, quando for um backup
    void handleStateUpdate(const packet_t& packet);
    
    // processa dados de cliente individual recebido
    void handleClientData(const packet_t& packet);
    
    // envia o estado para um backup específico
    void sendStateToBackup(const PeerInfo& peer);
    
    // envia dados de um cliente para um backup específico
    void sendClientToBackup(const PeerInfo& peer, uint32_t client_ip, const ClientInfo& client);

    // processa o pedido de sync vindo de outro servidor
    void handleSyncRequest(const sockaddr_in& sender);
};

#endif