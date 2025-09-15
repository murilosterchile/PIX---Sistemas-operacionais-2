#include <iostream>
#include <memory>
#include "client_discovery.h"
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
        std::cout << "=== CLIENTE PIX - Etapa 5 Debug ===" << std::endl;
        std::cout << "Porta: " << port << std::endl;
        std::cout << "Timestamp: " << getCurrentTimestamp() << std::endl;
        std::cout << "====================================" << std::endl;
        
        // Descobrir servidor
        ClientDiscovery discovery(port);
        std::string server_ip = discovery.discoverServer();
        
        std::cout << getCurrentTimestamp() 
                  << " server addr " << server_ip << std::endl;
        
        std::cout << "✅ Descoberta concluída com sucesso!" << std::endl;
        std::cout << "Servidor encontrado: " << server_ip << std::endl;
        std::cout << "Pressione Enter para encerrar..." << std::endl;
        std::cin.get();
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Erro: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}