#include "client_processor.h"
#include <cerrno>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <chrono>
#include "../common/protocol.h"
#include "../common/utils.h"
#include "../common/debug.h"


ClientProcessor::ClientProcessor(const std::string& server_ip, uint16_t server_port)
    : server_ip(server_ip), server_port(server_port), current_id(1) {
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sockfd<0){
        perror("Failed to create socket for processor");
        throw std::runtime_error("Failed to create socket for processor");
    }
    D_PRINT("processor initialized for server:port " << server_ip << ":" << server_port);
}

void ClientProcessor::request(const std::string& ip, int value) {
    D_PRINT("received request to transfer " << value << " to " << ip );

    //set socket timeout
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 1000*DEFAULT_TIMEOUT_MS;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("Error setting socket timeout");
    }

    //keep sending request until it is answered
    bool answered = false;
    while(!answered){
        //make request and save response message
        char req_packet[PACKET_SIZE];
        sockaddr_in server_addr;
        socklen_t server_len = sizeof(server_addr);
        sendto(sockfd, &req_packet, sizeof(req_packet), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));

        //try to receive response for DEFAULT_TIMEOUT_MS amount of time
        char ack_packet[PACKET_SIZE];
        ssize_t n = recvfrom(sockfd, ack_packet, sizeof(ack_packet), 0, (struct sockaddr*)&server_addr, &server_len);

        if (n > 0) {
            D_PRINT("Recebida req ack do servidor");
            if (n == PACKET_SIZE) {
                packet_t response;
                memcpy(&response, ack_packet, sizeof(response));
                packet_net_to_host(&response);
                
                D_PRINT("Tipo de resposta: " << response.type);
                D_PRINT("Esperado: " << static_cast<uint16_t>(REQUISICAO_ACK));
                
                if (static_cast<PacketType>(response.type) == REQUISICAO_ACK) {
                    uint32_t seqn = response.payload.req_ack.seqn;

                    //if the ack is for the right request
                    if(seqn == current_id){
                        D_PRINT("right ack received");

                        //format data from ack into the queue format
                        response_data_t response_message;
                        response_message.success = response.payload.req_ack.success; //success;
                        response_message.server_ip = server_ip;//server_ip;
                        response_message.id_req = seqn; //id_req;
                        response_message.dest_ip = ip; //dest_ip;
                        response_message.value = value; //value;
                        response_message.new_balance = response.payload.req_ack.new_balance; //new_balance;

                        //send info to interface queue
                        {
                            std::unique_lock<std::mutex> lock(queue_mutex);
                            response_queue.push(response_message);
                        }
                        cv.notify_one();

                        answered = true;
                        ++current_id;
                    } else {
                        std::cout << "Old ID received" << std::endl;
                    }
                } else {
                    std::cout << "Tipo de resposta inválido" << std::endl;
                }
            } else {
                std::cout << "Tamanho de resposta inválido" << std::endl;
            }
        //if couldn receive data from socket
        } else {
            //timeout reached
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                D_PRINT("timeout");
            //another error
            }else{
                D_PRINT("another error");
                break;
            }
        }
    }
}

response_data_t ClientProcessor::getResponse() {
    std::unique_lock<std::mutex> lock(queue_mutex);

    cv.wait(lock, [this]{ return !response_queue.empty(); });

    // Get the response from the front of the queue
    response_data_t response = response_queue.front();
    response_queue.pop();

    return response;
}

