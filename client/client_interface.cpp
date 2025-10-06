#include "client_interface.h"
#include "../common/debug.h"
#include "../common/utils.h"
#include "client_processor.h"
#include <sstream>
#include <string>

ClientInterface::ClientInterface(ClientProcessor& processor)
    : processor(processor), running(false){}

ClientInterface::~ClientInterface(){
    stop();
}

void ClientInterface::start(){
    running = true;
    input_thread = std::thread(&ClientInterface::input_loop, this);
    output_thread = std::thread(&ClientInterface::output_loop, this);
}

void ClientInterface::stop(){
    if (!running) return;
    if (input_thread.joinable()) input_thread.join();
    if (output_thread.joinable()) output_thread.join();
    running = false;
}

void ClientInterface::input_loop(){
    std::string input;
    while(running){
        //checka se CTRL+D
        if(!std::getline(std::cin, input)){
            running = false;
            break;
        }

        std::stringstream ss(input);
        std::string ip;
        int value;

        //checka se a linha enviada esta no formato IP VALUE
        if(ss >> ip >> value){
            //checka se o formato do IP esta correto
            if(isValidIpAddress(ip)){
                D_PRINT(ip << " " << value);
                processor.request(ip, value);
            }
            else{
                std::cerr << "Error: " << ip << " is not a valid ip adress" << std::endl;
            }
        }
        else{
            std::cerr << "Error: could not parse line: " << input << std::endl;
        }
    }
}

void ClientInterface::output_loop(){
//e.g.: 2024-10-01 18:37:01 server 10.1.1.20 id req 1 dest 10.1.1.3 value 10 new balance 90
    response_data_t  response;
    while(running){
    //check response queue and print to terminal every time a new response is added
        response = processor.getResponse();
        if(response.approved){
            D_PRINT("response for approved transaction");
            std::cout << getCurrentTimestamp() << " server " << response.server_ip << " id req " << response.id_req << " dest " << response.dest_ip << " value " << response.value << " new balance " << response.new_balance << std::endl;
        } else {
            D_PRINT("response for denied transaction");
            std::cout << getCurrentTimestamp() << " request denied: server " << response.server_ip << " id req " << response.id_req << " dest " << response.dest_ip << " value " << response.value << " balance " << response.new_balance << std::endl;
        }

    }
};
