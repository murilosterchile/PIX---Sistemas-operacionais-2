#include "client_processor.h"
#include <iostream>
#include <thread>
#include <chrono>

ClientProcessor::ClientProcessor(const std::string& server_ip, uint16_t server_port)
    : server_ip(server_ip), server_port(server_port) {
    std::cout << "[Processor] Mock processor initialized for server at "
              << server_ip << ":" << server_port << std::endl;
}

void ClientProcessor::request(const std::string& ip, int value) {
    // Simulate network delay and processing time
    std::cout << "[Processor] Received request to transfer " << value << " to " << ip << ". Processing..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Create a fake response message
    std::string response_message = "[CONFIRMADO] Transferência de R$" + std::to_string(value) + " para " + ip + " concluída com sucesso.";

    // Lock the queue and add the response. The lock_guard ensures the mutex is
    // released automatically when the function exits.
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        response_queue.push(response_message);
    }
     std::cout << "[Processor] Request processed. Response is ready." << std::endl;
}

std::optional<std::string> ClientProcessor::getResponse() {
    std::lock_guard<std::mutex> lock(queue_mutex);

    if (response_queue.empty()) {
        return std::nullopt; // No response available
    }

    // Get the response from the front of the queue
    std::string response = response_queue.front();
    response_queue.pop();

    return response;
}

