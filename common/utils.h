#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <cstdint>
#include <chrono>

// Conversões de IP
std::string ipToString(uint32_t ip_addr);
uint32_t stringToIp(const std::string& ip_str);

// Timestamp
std::string getCurrentTimestamp();

// Validações
bool isValidIpAddress(const std::string& ip);
bool isValidPort(uint16_t port);
bool isValidTransferValue(uint32_t value);

// Network utilities
int createUdpSocket();
int configureBroadcast(int socket_fd);
void setSocketTimeout(int socket_fd, int timeout_ms);

#endif
