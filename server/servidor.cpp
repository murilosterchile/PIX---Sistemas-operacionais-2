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

// SIGNAL HANDLER ORIGINAL (CAUSA DEADLOCK SE USAR JOIN INTERNO)
void signalHandler(int) {
    std::cout << "\nEncerrando servidor..." << std::endl;
    if (interface_service) interface_service->stop();
    if (processing_service) processing_service->stop();
    if (discovery_service) discovery_service->stop();
    if (replication_service) replication_service->stop();
    if (election_service) election_service->stop();
    exit(0);
}

int main(int argc, char* argv[]) {
    // Nova assinatura simplificada
    if (argc < 3) {
        std::cerr << "Uso: " << argv[0] << " <porta> <server_id>" << std::endl;
        return 1;
    }

    uint16_t port = static_cast<uint16_t>(std::atoi(argv[1]));
    if (!isValidPort(port)) return 1;
    
    uint32_t server_id = static_cast<uint32_t>(std::atoi(argv[2]));
    if (server_id == 0) return 1;
    
    server_config = new ServerConfig(server_id, port);
    server_data = new ServerData(server_config);
    
    std::cout << "Servidor ID: " << server_id << " na porta " << port << std::endl;
    
    server_config->status = BACKUP;
    std::cout << "[CONFIG] Iniciando como BACKUP..." << std::endl;
    
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    try {
        // Inicia serviços
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
        
        // --- AUTODESCOBERTA ATIVA ---
        discovery_service->discoverPeers();
        
        std::cout << "[STARTUP] Aguardando descoberta de peers..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // Lógica de Sincronização
        {
            std::shared_lock<std::shared_mutex> lock(server_data->rw_mutex);
            if (server_config->peers.empty()) {
                std::cout << "[STARTUP] Sem peers - assumindo isolado." << std::endl;
                server_data->is_synchronized = true;
            } else {
                std::cout << "[STARTUP] Peers encontrados: " << server_config->peers.size() << std::endl;
                replication_service->requestSync();
            }
        }
        
        // Aguarda sync se necessário
        if (!server_data->is_synchronized) {
            std::cout << "[STARTUP] Aguardando sync..." << std::endl;
            auto start = std::chrono::steady_clock::now();
            while (!server_data->is_synchronized) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
                if (elapsed > 3000) {
                    server_data->is_synchronized = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        
        // Eleição
        {
            std::shared_lock<std::shared_mutex> lock(server_data->rw_mutex);
            if (server_config->hasHighestId()) {
                std::cout << "[STARTUP] Maior ID. Iniciando eleição..." << std::endl;
                election_service->startElection();
            }
        }
        
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Erro: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}