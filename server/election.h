#ifndef ELECTION_H
#define ELECTION_H

#include <thread>
#include <atomic>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include "../common/protocol.h"
#include "../common/server_data.h"

class ElectionService {
private:
    int socket_fd;
    uint16_t election_port;
    ServerData* server_data;
    std::thread listener_thread;
    std::thread heartbeat_thread;
    std::atomic<bool> running;
    std::atomic<bool> election_in_progress;
    std::atomic<bool> is_sending_heartbeat;
    
    // Controle de heartbeat
    std::chrono::steady_clock::time_point last_heartbeat;
    static constexpr int HEARTBEAT_INTERVAL_MS = 1000;  // 1 segundo
    static constexpr int HEARTBEAT_TIMEOUT_MS = 3000;   // 3 segundos
    
public:
    ElectionService(uint16_t port, ServerData* data);
    ~ElectionService();
    
    void start();
    void stop();
    
    // inicia o processo de eleição
    void startElection();
    
private:
    // Thread que escuta mensagens de eleição
    void listenForElectionMessages();
    
    // Thread que monitora heartbeat, para backups
    void monitorHeartbeat();
    
    // Thread que envia heartbeat, para o primário
    void sendHeartbeatLoop();
    
    // processamento de mensagens recebidas
    void handleElectionMessage(const packet_t& packet, const sockaddr_in& sender);
    void handleOkMessage(const packet_t& packet, const sockaddr_in& sender);
    void handleCoordinatorMessage(const packet_t& packet, const sockaddr_in& sender);
    void handleHeartbeat(const packet_t& packet, const sockaddr_in& sender);
    void handleHeartbeatAck(const packet_t& packet, const sockaddr_in& sender);
    
    // sender de mensagens
    void sendElectionToHigher();
    void sendOkMessage(const sockaddr_in& dest);
    void announceCoordinator();
    void sendHeartbeat();
    
    // vira o novo primário
    void becomeCoordinator();
};

#endif