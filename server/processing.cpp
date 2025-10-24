#include "processing.h"
#include "../common/utils.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <shared_mutex>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <memory>

ProcessingService::ProcessingService(uint16_t port, ServerData* data)
    : port(port), server_data(data), running(false) {
    socket_fd = createUdpSocket();
    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port + 1);
    if (bind(socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(socket_fd);
        throw std::runtime_error("Erro no bind do socket");
    }
}

ProcessingService::~ProcessingService() {
    stop();
    if (socket_fd >= 0) close(socket_fd);
}

void ProcessingService::start() {
    running = true;
    listener_thread = std::thread(&ProcessingService::listenForRequests, this);
}

void ProcessingService::stop() {
    running = false;
    if (listener_thread.joinable()) listener_thread.join();
}

void ProcessingService::listenForRequests() {
    char buffer[PACKET_SIZE];
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    while (running) {
        ssize_t recv_len = recvfrom(socket_fd, buffer, sizeof(buffer), 0,
                                   (struct sockaddr*)&client_addr, &client_len);
        if (recv_len == PACKET_SIZE) {
            packet_t packet;
            memcpy(&packet, buffer, sizeof(packet_t));
            packet_net_to_host(&packet);
            if (static_cast<PacketType>(packet.type) == REQUISICAO) {
                std::thread request_thread(&ProcessingService::handleRequestThread, this, packet, client_addr);
                request_thread.detach();
            }
        }
    }
}

void ProcessingService::handleRequestThread(packet_t packet, sockaddr_in client_addr) {
    uint32_t client_ip = client_addr.sin_addr.s_addr;

    // Lock crítico de escrita
    std::unique_lock<std::shared_mutex> write_lock(server_data->rw_mutex);

    // cliente existe?
    auto client_it = server_data->clients.find(client_ip);
    if (client_it == server_data->clients.end()) {
        sendAck(client_addr, packet.seqn, 0, false);
        return;
    }
    ClientInfo& client = client_it->second;

    // verifica duplicata 
    if (packet.seqn <= client.last_req) {
        displayTransaction(packet, client_ip, true);
        sendAck(client_addr, packet.seqn, client.balance, true);
        return;
    }

    // verifica ordem (ignora pacotes fora de ordem antigos)
    if (packet.seqn > client.last_req + 1) {
        sendAck(client_addr, client.last_req, client.balance, false);
        return;
    }

    // verifica se cliente destino existe
    auto dest_it = server_data->clients.find(packet.payload.req.dest_addr);
    if (dest_it == server_data->clients.end()) {
        std::cout << "Tentativa de enviar para cliente inexistente ou servidor." << std::endl;
        client.last_req = packet.seqn; // marca como tratado
        sendAck(client_addr, packet.seqn, client.balance, false);
        return;
    }

    ClientInfo& dest_client = dest_it->second;

    // verifica saldo
    if (client.balance < packet.payload.req.value) {
        client.last_req = packet.seqn; // marca como tratado mesmo que falhe
        sendAck(client_addr, packet.seqn, client.balance, false);
        return;
    }

    // executa transferência (permite enviar 0 e enviar para si mesmo)
    client.balance -= packet.payload.req.value;
    dest_client.balance += packet.payload.req.value;
    client.last_req = packet.seqn;
    server_data->num_transactions++;
    server_data->total_transferred += packet.payload.req.value;

    displayTransaction(packet, client_ip, false);

    server_data->has_update = true;
    server_data->data_updated.notify_all();

    sendAck(client_addr, packet.seqn, client.balance, true);
}

void ProcessingService::sendAck(const sockaddr_in& addr, uint32_t seqn, uint32_t balance, bool success) {
    packet_t ack_packet;
    init_packet(&ack_packet, REQUISICAO_ACK, seqn);
    ack_packet.payload.req_ack.seqn = seqn;
    ack_packet.payload.req_ack.new_balance = balance; 
    ack_packet.payload.req_ack.success = success ? 1 : 0;
    packet_host_to_net(&ack_packet);

    sendto(socket_fd, &ack_packet, sizeof(ack_packet), 0, (struct sockaddr*)&addr, sizeof(addr));
}

void ProcessingService::displayTransaction(const packet_t& packet, uint32_t client_ip, bool duplicate) {
    std::cout << getCurrentTimestamp() << " client " << ipToString(client_ip);
    if (duplicate) std::cout << " DUP!!";
    std::cout << " id req " << packet.seqn
              << " dest " << ipToString(packet.payload.req.dest_addr)
              << " value " << packet.payload.req.value << std::endl
              << "num transactions " << server_data->num_transactions
              << " total transferred " << server_data->total_transferred
              << " total balance " << server_data->total_balance << std::endl;
}
