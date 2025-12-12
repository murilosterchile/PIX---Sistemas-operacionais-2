#include <iostream>
#include <memory>
#include <csignal>
#include <thread>
#include <chrono>
#include "discovery.h"
#include "processing.h"
#include "interface.h"
#include "../common/utils.h"
#include "../common/server_config.h"
#include "replication.h"
#include "election.h"

std::unique_ptr<DiscoveryService> discovery_service;
std::unique_ptr<ProcessingService> processing_service;
std::unique_ptr<Interface> interface_service;
ServerConfig* server_config = nullptr;
ServerData* server_data = nullptr;
std::unique_ptr<ReplicationService> replication_service;
std::unique_ptr<ElectionService> election_service;

void signalHandler(int) {
    std::cout << "\nEncerrando servidor..." << std::endl;
    
    if (interface_service) {
        interface_service->stop();
    }
    if (processing_service) {
        processing_service->stop();
    }
    if (discovery_service) {
        discovery_service->stop();
    }
    if (replication_service) {
        replication_service->stop();
    }
    if (election_service) {
        election_service->stop();
    }
    
    exit(0);
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Uso: " << argv[0] << " <porta> <server_id> [peer_id:peer_ip:peer_port ...]" << std::endl;
        return 1;
    }

    uint16_t port = static_cast<uint16_t>(std::atoi(argv[1]));
    if (!isValidPort(port)) {
        std::cerr << "Porta inválida: " << port << std::endl;
        return 1;
    }
    
    uint32_t server_id = static_cast<uint32_t>(std::atoi(argv[2]));
    if (server_id == 0) {
        std::cerr << "ID do servidor inválido, deve ser > 0" << std::endl;
        return 1;
    }
    
    // cria a configuração
    server_config = new ServerConfig(server_id, port);
    server_data = new ServerData(server_config);
    
    std::cout << "Servidor ID: " << server_id << " na porta " << port << std::endl;

    // configuramos os peers a partir dos argumentos
    for (int i = 3; i < argc; i++) {
        std::string peer_str(argv[i]);
        size_t first_colon = peer_str.find(':');
        size_t second_colon = peer_str.find(':', first_colon + 1);
        
        if (first_colon != std::string::npos && second_colon != std::string::npos) {
            uint32_t peer_id = std::stoul(peer_str.substr(0, first_colon));
            std::string peer_ip = peer_str.substr(first_colon + 1, second_colon - first_colon - 1);
            uint16_t peer_port = std::stoul(peer_str.substr(second_colon + 1));
            
            server_config->peers.emplace_back(peer_id, peer_ip, peer_port, peer_port + 100);
            std::cout << "[CONFIG] Peer adicionado: ID=" << peer_id 
                      << " IP=" << peer_ip << " Porta=" << peer_port << std::endl;
        }
    }
    
    // --- LÓGICA DE STATUS INICIAL (MODIFICADA) ---
    // NUNCA inicie como PRIMARY automaticamente. 
    // Inicie sempre como BACKUP para proteger os dados.
    server_config->status = BACKUP;
    std::cout << "[CONFIG] Iniciando como BACKUP para sincronização..." << std::endl;
    
    // configurando handler de sinal
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    try {
        // Status inicial
        std::cout << getCurrentTimestamp()
                  << " num_transactions 0 total_transferred 0 total_balance 0"
                  << std::endl;
        
        // inicia serviços
        discovery_service = std::make_unique<DiscoveryService>(port, server_data);
        discovery_service->start();
        
        replication_service = std::make_unique<ReplicationService>(server_config->repl_port, server_data);
        replication_service->start();
        
        processing_service = std::make_unique<ProcessingService>(port, server_data, replication_service.get());
        processing_service->start();
        
        interface_service = std::make_unique<Interface>(server_data);
        interface_service->start();

        election_service = std::make_unique<ElectionService>(port, server_data);
        election_service->start();
        
        std::cout << "Servidor iniciado na porta " << port << std::endl;
        std::cout << "Pressione Ctrl+C para encerrar" << std::endl;
        
        // --- LÓGICA DE STARTUP SEGURO ---
        
        // 1. Tenta pegar os dados mais recentes de quem estiver vivo
        // (Isso envia um SYNC_REQ para os peers)
        replication_service->requestSync();
        
        // 2. Dá um tempo para os dados chegarem (1.5 segundos)
        std::cout << "Aguardando sincronização de dados..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        
        // 3. Agora que (provavelmente) estamos sincronizados, verificamos a liderança
        if (server_config->hasHighestId()) {
            std::cout << "Sou o servidor com maior ID. Iniciando eleição para assumir..." << std::endl;
            // Se eu tenho o maior ID, inicio uma eleição para me tornar o líder oficial
            election_service->startElection();
        } else {
            std::cout << "Não sou o maior ID. Permanecendo como Backup." << std::endl;
        }
        
        // Loop principal
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Erro: " << e.what() << std::endl;
        return 1;
    }
    
    delete server_data;
    delete server_config;

    return 0;
}