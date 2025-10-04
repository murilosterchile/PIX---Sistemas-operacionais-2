#include "client_interface.hpp"
#include "../common/debug.h"
#include "../common/utils.h"
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
    running = false;
    if (input_thread.joinable()) input_thread.join();
    if (output_thread.joinable()) output_thread.join();
}

void ClientInterface::input_loop(){
    std::string input;
    while(running){
        if(!std::getline(std::cin, input)){
            running = false;
            break;
        }

        std::stringstream ss;
        std::string ip;
        int value;

        if(ss >> ip >> value){
            if(isValidIpAddress(ip)){
                D_PRINT(ip << " " << value);
                //processor.request(ip, value);
            }
            else{
                std::cerr << "Error: " << ip << " is not a valid ip adress" << std:endl;
            }
        }
        else{
            std::cerr << "Error: could not parse line: " << input << std::endl;
        }
    }
}

void ClientInterface::output_loop();
