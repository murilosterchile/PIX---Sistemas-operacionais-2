#include <iostream>
#include <memory>
#include "client_discovery.h"
#include "client_interface.h"
#include "client_processor.h"
#include "../common/utils.h"
#include  "../common/debug.h"

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

        //roda a interface (thread de input e de output) que vai se comunicar com o modulo de processamento que manda requisicoes para o servidor
        interface.start();
        //depois que a o usuario encerra o processo da interface ela da join nas duas threads e corrige a variavel running para false
        interface.stop();
    } catch (const std::exception& e) {
        std::cerr << "Erro: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
