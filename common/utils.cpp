#include "utils.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sstream>
#include <iomanip>
#include <regex>

std::string ipToString(uint32_t ip_addr) {
    struct in_addr addr;
    addr.s_addr = ip_addr;
    return std::string(inet_ntoa(addr));
}

uint32_t stringToIp(const std::string& ip_str) {
    struct in_addr addr;
    if (inet_aton(ip_str.c_str(), &addr) != 0) {
        return addr.s_addr;
    }
    return 0;
}

std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

bool isValidIpAddress(const std::string& ip) {
    std::regex ip_regex("^([0-9]{1,3})\\.([0-9]{1,3})\\.([0-9]{1,3})\\.([0-9]{1,3})$");
    return std::regex_match(ip, ip_regex);
}

bool isValidPort(uint16_t port) {
    return port >= 1024 && port <= 65535;
}

bool isValidTransferValue(uint32_t value) {
    return value >= 0 && value <= 999999;
}

int createUdpSocket() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        throw std::runtime_error("Erro ao criar socket UDP");
    }
    return sock;
}

int configureBroadcast(int socket_fd) {
    int broadcast = 1;
    return setsockopt(socket_fd, SOL_SOCKET, SO_BROADCAST, 
                     &broadcast, sizeof(broadcast));
}

void setSocketTimeout(int socket_fd, int timeout_ms) {
    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    
    setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, 
               &timeout, sizeof(timeout));
}
