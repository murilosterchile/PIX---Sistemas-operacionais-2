#ifndef PROTOCOL_H
#define PROTOCOL_H

/**
 * @file protocol.h
 * @brief Definições do protocolo de comunicação UDP para sistema de transferência de valores
 */

#include <cstdint>
#include <cstring>
#include <arpa/inet.h>

// ============================================================================
// CONSTANTES DO PROTOCOLO
// ============================================================================

/** @brief Porta padrão para comunicação UDP */
#define DEFAULT_PORT 4000

/** @brief Saldo inicial de cada cliente (em reais) */
#define INITIAL_BALANCE 100

/** @brief Timeout padrão para retransmissão (em milissegundos) */
#define DEFAULT_TIMEOUT_MS 10

/** @brief Número máximo de tentativas de retransmissão */
#define MAX_RETRIES 3

/** @brief Tamanho máximo do buffer de recepção UDP */
#define UDP_BUFFER_SIZE 1024

// ============================================================================
// TIPOS DE MENSAGEM
// ============================================================================

/**
 * @brief Tipos de pacotes suportados pelo protocolo
 */
enum PacketType : uint16_t {
    DESCOBERTA     = 1,  /**< Cliente solicita descoberta do servidor (broadcast) */
    REQUISICAO     = 2,  /**< Cliente envia requisição de transação (unicast) */
    DESCOBERTA_ACK = 3,  /**< Servidor responde à descoberta (unicast) */
    REQUISICAO_ACK = 4   /**< Servidor responde à requisição (unicast) */
};

// ============================================================================
// ESTRUTURAS DE DADOS
// ============================================================================

/**
 * @brief Estrutura da requisição de transação
 */
struct requisicao {
    uint32_t dest_addr;  /**< Endereço IP do cliente destino (network order) */
    uint32_t value;      /**< Valor da transferência (em reais) */
} __attribute__((packed));

/**
 * @brief Estrutura da resposta de requisição (ACK)
 */
struct requisicao_ack {
    uint32_t seqn;        /**< Número de sequência confirmado */
    uint32_t new_balance; /**< Novo saldo do cliente origem */
    uint8_t  success;     /**< 1=sucesso, 0=falha */
    uint8_t  padding[3];  /**< Padding para alinhamento */
} __attribute__((packed));

/**
 * @brief Estrutura vazia para mensagem de descoberta
 */
struct descoberta {
    uint8_t reserved[4];  /**< Bytes reservados (devem ser zero) */
} __attribute__((packed));

/**
 * @brief Estrutura da resposta de descoberta (ACK)
 */
struct descoberta_ack {
    uint32_t server_addr; /**< Endereço IP do servidor (network order) */
    uint16_t server_port; /**< Porta do servidor (network order) */
    uint8_t  accepted;    /**< 1=cliente aceito, 0=rejeitado */
    uint8_t  padding;     /**< Padding para alinhamento */
} __attribute__((packed));

// ============================================================================
// ESTRUTURA PRINCIPAL DO PROTOCOLO
// ============================================================================

/**
 * @brief Estrutura principal de todos os pacotes do protocolo
 */
typedef struct packet {
    uint16_t type;        /**< Tipo do pacote (PacketType) */
    uint32_t seqn;        /**< Número de sequência da mensagem */
    
    union {
        struct requisicao req;          /**< Dados de requisição de transação */
        struct requisicao_ack req_ack;  /**< Dados de confirmação de requisição */
        struct descoberta disc;         /**< Dados de descoberta (vazio) */
        struct descoberta_ack disc_ack; /**< Dados de confirmação de descoberta */
    } payload;
    
} __attribute__((packed)) packet_t;

// ============================================================================
// CONSTANTES DERIVADAS
// ============================================================================

/** @brief Tamanho total de um pacote do protocolo */
#define PACKET_SIZE sizeof(packet_t)

// ============================================================================
// FUNÇÕES DE CONVERSÃO CORRIGIDAS
// ============================================================================

/**
 * @brief Inicializa um pacote com valores padrão
 */
inline void init_packet(packet_t* packet, PacketType type, uint32_t seqn) {
    if (packet) {
        memset(packet, 0, sizeof(packet_t));
        packet->type = static_cast<uint16_t>(type);  // NÃO converter aqui
        packet->seqn = seqn;  // NÃO converter aqui
    }
}

/**
 * @brief Converte um pacote recebido de network para host order
 */
inline void packet_net_to_host(packet_t* packet) {
    if (packet) {
        // Converter header
        packet->type = ntohs(packet->type);
        packet->seqn = ntohl(packet->seqn);
        
        // Converter payload baseado no tipo
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
                
            case DESCOBERTA:
                // Nenhuma conversão necessária
                break;
                
            default:
                // Tipo inválido - não converter
                break;
        }
    }
}

/**
 * @brief Converte um pacote de host para network order antes do envio
 */
inline void packet_host_to_net(packet_t* packet) {
    if (packet) {
        PacketType original_type = static_cast<PacketType>(packet->type);
        
        // Converter campos específicos por tipo ANTES de converter o header
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
                
            case DESCOBERTA:
                // Nenhuma conversão necessária
                break;
        }
        
        // Converter header por último
        packet->type = htons(static_cast<uint16_t>(original_type));
        packet->seqn = htonl(packet->seqn);
    }
}

#endif 