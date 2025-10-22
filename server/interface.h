#ifndef INTERFACE_H
#define INTERFACE_H

#include <thread>
#include <atomic>
#include "../common/server_data.h"

class Interface {
private:
    ServerData* server_data;
    std::thread reader_thread;
    std::atomic<bool> running;
    
public:
    Interface(ServerData* data);
    ~Interface();
    
    void start();
    void stop();
    
private:
    void monitorUpdates();
};

#endif