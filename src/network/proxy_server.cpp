#include "proxy_server.h"
#include "../protocol/packet_parser.h"
#include <iostream>
#include <thread>
#include <vector>
#include <sstream>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <netdb.h>

ProxyServer::ProxyServer(const ProxyConfig& config) : config_(config) {}

void ProxyServer::run() {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        throw std::runtime_error("Failed to create socket");
    }

    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8123);

    if (bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(server_socket);
        throw std::runtime_error("Failed to bind socket");
    }

    if (listen(server_socket, 10) < 0) {
        close(server_socket);
        throw std::runtime_error("Failed to listen on socket");
    }

    std::cout << "\033[32mProxy listening on " << config_.listen_address << "\033[0m" << std::endl;
    std::cout << "\033[34mForwarding to " << config_.remote_address << "\033[0m" << std::endl;

    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_socket = accept(server_socket, (sockaddr*)&client_addr, &client_len);
        
        if (client_socket >= 0) {
            std::cout << "\033[36mNew connection from " << inet_ntoa(client_addr.sin_addr) << "\033[0m" << std::endl;
            std::thread(&ProxyServer::handle_client, this, client_socket).detach();
        }
    }
}

void ProxyServer::handle_client(int client_socket) {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        close(client_socket);
        return;
    }

    struct hostent* host = gethostbyname("game-server-01.prod.ava.101xp.com");
    if (!host) {
        close(client_socket);
        close(server_socket);
        return;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8123);
    memcpy(&server_addr.sin_addr, host->h_addr, host->h_length);

    if (connect(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(client_socket);
        close(server_socket);
        return;
    }

    std::thread client_to_server([this, client_socket, server_socket]() {
        forward_data(client_socket, server_socket, "CLIENT->SERVER", config_.log_client_packets);
    });

    std::thread server_to_client([this, server_socket, client_socket]() {
        forward_data(server_socket, client_socket, "SERVER->CLIENT", config_.log_server_packets);
    });

    client_to_server.join();
    server_to_client.join();

    close(client_socket);
    close(server_socket);
}

void ProxyServer::forward_data(int from_socket, int to_socket, const std::string& direction, bool log_enabled) {
    std::vector<uint8_t> buffer;
    buffer.reserve(8192);
    
    while (true) {
        uint32_t packet_length = read_packet_length(from_socket);
        if (packet_length == 0) break;

        buffer.resize(4 + packet_length);
        *reinterpret_cast<uint32_t*>(buffer.data()) = htonl(packet_length);

        ssize_t bytes_read = recv(from_socket, buffer.data() + 4, packet_length, MSG_WAITALL);
        if (bytes_read != packet_length) break;

        bool should_send_original = true;
        
        if (log_enabled) {
            try {
                auto packet = PacketParser::parse(buffer);
                
                if (direction == "CLIENT->SERVER") {
                    std::cout << "\033[33m" << direction << ": {\"type\":" << (int)packet.message_type;
                } else {
                    std::cout << "\033[35m" << direction << ": {\"type\":" << (int)packet.message_type;
                }
                
                if (!packet.data.is_null()) {
                    std::cout << ",\"data\":" << packet.data.to_json();
                }
                std::cout << "}\033[0m" << std::endl;
                
            } catch (const std::exception& e) {
                if (direction == "CLIENT->SERVER") {
                    std::cout << "\033[33m" << direction << ": {\"type\":\"parse_error\",\"error\":\"" << e.what() << "\"}\033[0m" << std::endl;
                } else {
                    std::cout << "\033[35m" << direction << ": {\"type\":\"parse_error\",\"error\":\"" << e.what() << "\"}\033[0m" << std::endl;
                }
            }
        }
        
        // Send packet
        send(to_socket, buffer.data(), 4 + packet_length, 0);
    }
}

uint32_t ProxyServer::read_packet_length(int socket) {
    uint32_t length;
    ssize_t bytes_read = recv(socket, &length, 4, MSG_WAITALL);
    return (bytes_read == 4) ? ntohl(length) : 0;
}
