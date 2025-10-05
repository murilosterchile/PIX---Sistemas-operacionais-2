#include "client_processor.h"
#include <condition_variable>
#include <iostream>
#include <thread>
#include <chrono>
#include "../common/debug.h"

ClientProcessor::ClientProcessor(const std::string& server_ip, uint16_t server_port)
    : server_ip(server_ip), server_port(server_port) {
    D_PRINT("processor initialized for server:port " << server_ip << ":" << server_port);
}

void ClientProcessor::request(const std::string& ip, int value) {
    D_PRINT("received request to transfer " << value << " to " << ip );
    response_data_t response_message;

    //make request and save response message
    D_PRINT("placeholder for request");
    // Example of an approved response
    response_data_t approved_response = {
        true,                      // approved
        "192.168.1.1",             // server_ip
        101,                       // id_req
        "10.0.0.5",                // dest_ip
        75,                        // value
        925                        // new_balance
    };

    // Example of a denied response
    response_data_t denied_response = {
        false,                     // approved
        "192.168.1.1",             // server_ip
        102,                       // id_req
        "10.0.0.8",                // dest_ip
        5000,                      // value (e.g., insufficient funds)
        230                        // new_balance (unchanged)
    };
    //add response to the queue so interface can consume
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        //response_queue.push(response_message);
        response_queue.push(approved_response);
    }
    cv.notify_one();
}

response_data_t ClientProcessor::getResponse() {
    std::unique_lock<std::mutex> lock(queue_mutex);

    cv.wait(lock, [this]{ return !response_queue.empty(); });

    // Get the response from the front of the queue
    response_data_t response = response_queue.front();
    response_queue.pop();

    return response;
}

