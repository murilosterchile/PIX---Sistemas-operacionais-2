
#include <iostream>
#include <memory>
#include <csignal>
#include "discovery.h"
#include "../common/utils.h"

std::unique_ptr<DiscoveryService> discovery_service;
ServerData server_data;

void signalHandler(int signum) {
    std::cout << "\nEncerrando servidor..." << std::endl;
    if (discovery_service) {
        discovery_service->stop();
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Uso: " << argv << " <porta>" << std::endl;
        return 1;
    }
    
    uint16_t port = static_cast<uint16_t>(std::atoi(argv[1]));
    if (!isValidPort(port)) {
        std::cerr << "Porta inválida: " << port << std::endl;
        return 1;
    }
    
    // Configurar handler de sinal
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    try {
        // Status inicial
        std::cout << getCurrentTimestamp()
                  << " num_transactions 0 total_transferred 0 total_balance 0"
                  << std::endl;
        
        // Iniciar serviço de descoberta
        discovery_service = std::make_unique<DiscoveryService>(port, &server_data);
        discovery_service->start();
        
        std::cout << "Servidor iniciado na porta " << port << std::endl;
        std::cout << "Pressione Ctrl+C para encerrar" << std::endl;
        
        // Loop principal
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Erro: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}