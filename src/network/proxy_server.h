#pragma once
#include <string>
#include <cstdint>

struct ProxyConfig {
    std::string listen_address;
    std::string remote_address;
    bool log_client_packets = true;
    bool log_server_packets = true;
};

class ProxyServer {
public:
    explicit ProxyServer(const ProxyConfig& config);
    void run();

private:
    ProxyConfig config_;
    void handle_client(int client_socket);
    void forward_data(int from_socket, int to_socket, const std::string& direction, bool log_enabled);
    uint32_t read_packet_length(int socket);
};
