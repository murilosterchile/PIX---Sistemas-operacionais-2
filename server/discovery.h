#ifndef DISCOVERY_H
#define DISCOVERY_H

#include <thread>
#include <atomic>
#include <sys/socket.h>
#include <netinet/in.h>
#include "../common/protocol.h"
#include "../common/server_data.h"

class DiscoveryService {
private:
    int socket_fd;
    uint16_t port;
    ServerData* server_data;
    std::thread listener_thread;
    std::atomic<bool> running;
    
public:
    DiscoveryService(uint16_t port, ServerData* data);
    ~DiscoveryService();
    
    void start();
    void stop();
    
private:
    void listenForDiscovery();
    void handleDiscoveryRequest(const sockaddr_in& client_addr);
    void sendDiscoveryResponse(const sockaddr_in& client_addr);
};

#endif
