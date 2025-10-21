#include "interface.h"
#include "../common/utils.h"
#include <iostream>
#include <shared_mutex>

Interface::Interface(ServerData* data) 
    : server_data(data), running(false) {
}

Interface::~Interface() {
    stop();
}

void Interface::start() {
    running = true;
    reader_thread = std::thread(&Interface::monitorUpdates, this);
}

void Interface::stop() {
    running = false;
    server_data->data_updated.notify_all(); // Acordar thread para finalizar
    if (reader_thread.joinable()) {
        reader_thread.join();
    }
}

void Interface::monitorUpdates() {
    while (running) {
        // leitor, aguarda notificação de atualização
        std::shared_lock<std::shared_mutex> lock(server_data->rw_mutex);
        
        server_data->data_updated.wait(lock, [this] { 
            return server_data->has_update || !running; 
        });
        
        if (!running) break;

        if (server_data->has_update) {
            // exibe estatisticas atualizadas
            std::cout << getCurrentTimestamp()
                      << " num_transactions " << server_data->num_transactions
                      << " total_transferred " << server_data->total_transferred
                      << " total_balance " << server_data->total_balance
                      << std::endl;
            
            server_data->has_update = false;
        }
    }
}