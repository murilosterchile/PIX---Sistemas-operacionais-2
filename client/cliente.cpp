#include <iostream>
#include <memory>
#include "client_discovery.h"
#include "client_interface.h"
#include "client_processor.h"
#include "../common/utils.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Uso: " << argv[0] << " <porta>" << std::endl;
        return 1;
    }
    
    uint16_t port = static_cast<uint16_t>(std::atoi(argv[1]));
    if (!isValidPort(port)) {
        std::cerr << "Porta inválida: " << port << std::endl;
        return 1;
    }
    
    try {
        // Descobrir servidor
        ClientDiscovery discovery(port);
        std::string server_ip = discovery.discoverServer();
        
        std::cout << getCurrentTimestamp() << " server_addr " << server_ip << std::endl;

        ClientProcessor processor(server_ip, port);
        ClientInterface interface(processor);

        interface.start();
        interface.stop();
    } catch (const std::exception& e) {
        std::cerr << "Erro: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
