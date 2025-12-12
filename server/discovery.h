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
    std::chrono::steady_clock::time_point last_heartbeat_time;
    bool running_election;
    
public:
    DiscoveryService(uint16_t port, ServerData* data);
    ~DiscoveryService();
    
    void start();
    void stop();
    
private:
    void listenForDiscovery();
    void handleDiscoveryRequest(const sockaddr_in& client_addr);
    void sendDiscoveryResponse(const sockaddr_in& client_addr);
    void sendElectionResponse(const sockaddr_in& client_addr);
    void startElection();
};

#endif
