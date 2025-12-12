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
    
private:
    // Thread que escuta updates, quando for um backup
    void listenForUpdates();
    
    // processa o update recebido, quando for um backup
    void handleStateUpdate(const packet_t& packet);
    
    // envia o estado para um backup específico
    void sendStateToBackup(const PeerInfo& peer);
};

#endif