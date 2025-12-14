#ifndef PROTOCOL_H
#define PROTOCOL_H

/**
 * @file protocol.h
 * @brief Definições do protocolo de comunicação UDP
 */

#include <cstdint>
#include <cstring>
#include <arpa/inet.h>

// ============================================================================
// CONSTANTES DO PROTOCOLO
// ============================================================================

#define DEFAULT_PORT 4000
#define INITIAL_BALANCE 100
#define DEFAULT_TIMEOUT_MS 10
#define MAX_RETRIES 3
#define UDP_BUFFER_SIZE 1024

// ============================================================================
// TIPOS DE MENSAGEM
// ============================================================================

enum PacketType : uint16_t {
    DESCOBERTA     = 1,
    REQUISICAO     = 2,
    DESCOBERTA_ACK = 3,
    REQUISICAO_ACK = 4,
    STATE_UPDATE   = 5,
    ELECTION       = 6,
    OK             = 7,
    COORDINATOR    = 8,
    HEARTBEAT      = 9,
    HEARTBEAT_ACK  = 10,
    SYNC_REQ       = 11,
    CLIENT_DATA    = 12,
    LEADER_CHANGE  = 13,
    
    // NOVOS TIPOS PARA AUTODESCOBERTA DE SERVIDORES
    SERVER_HELLO   = 20, 
    SERVER_WELCOME = 21  
};

// ============================================================================
// ESTRUTURAS DE DADOS (DEVEM VIR ANTES DO packet_t)
// ============================================================================

struct requisicao {
    uint32_t dest_addr;
    uint32_t value;
} __attribute__((packed));

struct requisicao_ack {
    uint32_t seqn;
    uint32_t new_balance;
    uint8_t  success;
    uint8_t  padding[3];
} __attribute__((packed));

struct descoberta {
    uint8_t reserved[4];
} __attribute__((packed));

struct descoberta_ack {
    uint32_t server_addr;
    uint16_t server_port;
    uint8_t  accepted;
    uint8_t  padding;
} __attribute__((packed));

struct state_update {
    uint32_t num_transactions;
    uint32_t total_transferred;
    uint32_t total_balance;
    uint32_t num_clients;
} __attribute__((packed));

struct simple_message {
    uint8_t reserved[4];
} __attribute__((packed));

struct client_data {
    uint32_t client_addr;
    uint32_t balance;
    uint32_t last_req;
} __attribute__((packed));

struct leader_change {
    uint32_t new_leader_id;
    uint32_t new_leader_ip;
    uint16_t new_leader_port;
    uint8_t  padding[2];
} __attribute__((packed));

// NOVA ESTRUTURA PARA SERVIDORES (Necessária para o union abaixo)
struct server_info {
    uint32_t id;
    uint16_t port;
    uint16_t repl_port;
    uint8_t  padding[4];
} __attribute__((packed));

// ============================================================================
// ESTRUTURA PRINCIPAL DO PROTOCOLO
// ============================================================================

typedef struct packet {
    uint16_t type;
    uint32_t seqn;
    
    union {
        struct requisicao req;
        struct requisicao_ack req_ack;
        struct descoberta disc;
        struct descoberta_ack disc_ack;
        struct state_update state;
        struct simple_message simple;
        struct client_data cli_data;
        struct leader_change leader;
        
        // NOVO CAMPO (Agora vai funcionar porque server_info foi definido acima)
        struct server_info server;
    } payload;
    
} __attribute__((packed)) packet_t;

#define PACKET_SIZE sizeof(packet_t)

// ============================================================================
// FUNÇÕES DE CONVERSÃO
// ============================================================================

inline void init_packet(packet_t* packet, PacketType type, uint32_t seqn) {
    if (packet) {
        memset(packet, 0, sizeof(packet_t));
        packet->type = static_cast<uint16_t>(type);
        packet->seqn = seqn;
    }
}

inline void packet_net_to_host(packet_t* packet) {
    if (packet) {
        packet->type = ntohs(packet->type);
        packet->seqn = ntohl(packet->seqn);
        
        switch (static_cast<PacketType>(packet->type)) {
            case REQUISICAO:
                packet->payload.req.dest_addr = ntohl(packet->payload.req.dest_addr);
                packet->payload.req.value = ntohl(packet->payload.req.value);
                break;
            case REQUISICAO_ACK:
                packet->payload.req_ack.seqn = ntohl(packet->payload.req_ack.seqn);
                packet->payload.req_ack.new_balance = ntohl(packet->payload.req_ack.new_balance);
                break;
            case DESCOBERTA_ACK:
                packet->payload.disc_ack.server_addr = ntohl(packet->payload.disc_ack.server_addr);
                packet->payload.disc_ack.server_port = ntohs(packet->payload.disc_ack.server_port);
                break;
            case STATE_UPDATE:
                packet->payload.state.num_transactions = ntohl(packet->payload.state.num_transactions);
                packet->payload.state.total_transferred = ntohl(packet->payload.state.total_transferred);
                packet->payload.state.total_balance = ntohl(packet->payload.state.total_balance);
                packet->payload.state.num_clients = ntohl(packet->payload.state.num_clients);
                break;
            case CLIENT_DATA:
                packet->payload.cli_data.client_addr = ntohl(packet->payload.cli_data.client_addr);
                packet->payload.cli_data.balance = ntohl(packet->payload.cli_data.balance);
                packet->payload.cli_data.last_req = ntohl(packet->payload.cli_data.last_req);
                break;
            case LEADER_CHANGE:
                packet->payload.leader.new_leader_id = ntohl(packet->payload.leader.new_leader_id);
                packet->payload.leader.new_leader_ip = ntohl(packet->payload.leader.new_leader_ip);
                packet->payload.leader.new_leader_port = ntohs(packet->payload.leader.new_leader_port);
                break;
            // NOVOS CASES PARA SERVIDOR
            case SERVER_HELLO:
            case SERVER_WELCOME:
                packet->payload.server.id = ntohl(packet->payload.server.id);
                packet->payload.server.port = ntohs(packet->payload.server.port);
                packet->payload.server.repl_port = ntohs(packet->payload.server.repl_port);
                break;
            default: break;
        }
    }
}

inline void packet_host_to_net(packet_t* packet) {
    if (packet) {
        PacketType original_type = static_cast<PacketType>(packet->type);
        
        switch (original_type) {
            case REQUISICAO:
                packet->payload.req.dest_addr = htonl(packet->payload.req.dest_addr);
                packet->payload.req.value = htonl(packet->payload.req.value);
                break;
            case REQUISICAO_ACK:
                packet->payload.req_ack.seqn = htonl(packet->payload.req_ack.seqn);
                packet->payload.req_ack.new_balance = htonl(packet->payload.req_ack.new_balance);
                break;
            case DESCOBERTA_ACK:
                packet->payload.disc_ack.server_addr = htonl(packet->payload.disc_ack.server_addr);
                packet->payload.disc_ack.server_port = htons(packet->payload.disc_ack.server_port);
                break;
            case STATE_UPDATE:
                packet->payload.state.num_transactions = htonl(packet->payload.state.num_transactions);
                packet->payload.state.total_transferred = htonl(packet->payload.state.total_transferred);
                packet->payload.state.total_balance = htonl(packet->payload.state.total_balance);
                packet->payload.state.num_clients = htonl(packet->payload.state.num_clients);
                break;
            case CLIENT_DATA:
                packet->payload.cli_data.client_addr = htonl(packet->payload.cli_data.client_addr);
                packet->payload.cli_data.balance = htonl(packet->payload.cli_data.balance);
                packet->payload.cli_data.last_req = htonl(packet->payload.cli_data.last_req);
                break;
            case LEADER_CHANGE:
                packet->payload.leader.new_leader_id = htonl(packet->payload.leader.new_leader_id);
                packet->payload.leader.new_leader_ip = htonl(packet->payload.leader.new_leader_ip);
                packet->payload.leader.new_leader_port = htons(packet->payload.leader.new_leader_port);
                break;
            // NOVOS CASES PARA SERVIDOR
            case SERVER_HELLO:
            case SERVER_WELCOME:
                packet->payload.server.id = htonl(packet->payload.server.id);
                packet->payload.server.port = htons(packet->payload.server.port);
                packet->payload.server.repl_port = htons(packet->payload.server.repl_port);
                break;
            default: break;
        }
        
        packet->type = htons(static_cast<uint16_t>(original_type));
        packet->seqn = htonl(packet->seqn);
    }
}

#endif