#ifndef CLIENT_PROCESSOR_H
#define CLIENT_PROCESSOR_H

#include <condition_variable>
#include <string>
#include <cstdint>
#include <queue>
#include <mutex>
#include <optional>

typedef struct response_data{
    bool approved;
    std::string server_ip;
    int id_req;
    std::string dest_ip;
    int value;
    int new_balance;
} response_data_t;

class ClientProcessor {
public:
    ClientProcessor(const std::string& server_ip, uint16_t server_port);
    void request(const std::string& ip, int value);
    response_data_t getResponse();

private:
    std::string server_ip;
    uint16_t server_port;
    std::queue<response_data_t> response_queue;
    std::mutex queue_mutex;
    std::condition_variable cv;
};

#endif // CLIENT_PROCESSOR_H
