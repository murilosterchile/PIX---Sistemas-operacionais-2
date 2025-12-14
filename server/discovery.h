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
    
    // Método para autodescoberta
    void discoverPeers();
    
private:
    void listenForDiscovery();
    void handleDiscoveryRequest(const sockaddr_in& client_addr);
    void sendDiscoveryResponse(const sockaddr_in& client_addr);
    
    // Novos handlers
    void handleServerMessage(const packet_t& packet, const sockaddr_in& sender_addr);
    void addPeerIfNew(uint32_t id, const std::string& ip, uint16_t port, uint16_t repl_port);
};

#endif