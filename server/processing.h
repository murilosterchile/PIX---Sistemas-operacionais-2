#ifndef PROCESSING_H
#define PROCESSING_H

#include <thread>
#include <atomic>
#include <sys/socket.h>
#include <netinet/in.h>
#include "../common/protocol.h"
#include "../common/server_data.h"

class ProcessingService {
private:
    int socket_fd;
    uint16_t port;
    ServerData* server_data;
    std::thread listener_thread;
    std::atomic<bool> running;

public:
    ProcessingService(uint16_t port, ServerData* data);
    ~ProcessingService();

    void start();
    void stop();

    void listenForRequests(); // Thread de escuta principal
    void handleRequestThread(packet_t packet, sockaddr_in client_addr);

private:
    void sendAck(const sockaddr_in& addr, uint32_t seqn, uint32_t balance, bool success);
    void displayTransaction(const packet_t& packet, uint32_t client_ip, bool duplicate = false);
};
#endif
