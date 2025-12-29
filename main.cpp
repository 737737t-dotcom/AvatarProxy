#include "src/network/proxy_server.h"
#include <iostream>

int main() {
    try {
        ProxyConfig config;
        config.listen_address = "0.0.0.0:8123";
        config.remote_address = "game-server-01.prod.ava.101xp.com:8123";
        config.log_client_packets = true;
        config.log_server_packets = true;

        ProxyServer server(config);
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "\033[31mError: " << e.what() << "\033[0m" << std::endl;
        return 1;
    }
    
    return 0;
}
