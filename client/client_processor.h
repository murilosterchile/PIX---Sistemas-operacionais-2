#ifndef CLIENT_PROCESSOR_H
#define CLIENT_PROCESSOR_H

#include <string>
#include <cstdint>
#include <queue>
#include <mutex>
#include <optional>

class ClientProcessor {
public:
    ClientProcessor(const std::string& server_ip, uint16_t server_port);
    void request(const std::string& ip, int value);
    std::optional<std::string> getResponse();

private:
    std::string server_ip;
    uint16_t server_port;
    std::queue<std::string> response_queue;
    std::mutex queue_mutex;
};

#endif // CLIENT_PROCESSOR_H
