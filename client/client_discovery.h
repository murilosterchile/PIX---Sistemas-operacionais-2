#ifndef CLIENT_DISCOVERY_H
#define CLIENT_DISCOVERY_H

#include <string>
#include <sys/socket.h>
#include "../common/protocol.h"

class ClientDiscovery {
private:
    int socket_fd;
    uint16_t port;
    
public:
    ClientDiscovery(uint16_t port);
    ~ClientDiscovery();
    
    std::string discoverServer();
    
private:
    void sendDiscoveryBroadcast();
    bool waitForResponse(std::string& server_ip);
};

#endif