#include "host_manager.hpp"
#include <fstream>
#include <sstream>
#include "../n3ds/pair_record.hpp"

const char* HostManager::HOSTS_FILE = "/3ds/moonlight/hosts.txt";

std::vector<Host> HostManager::loadHosts() {
    std::vector<Host> hosts;
    std::ifstream file(HOSTS_FILE);
    
    if (!file.is_open()) {
        return hosts;  // Return empty if file doesn't exist yet
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        // Format: ip:port|pin
        size_t pipe_pos = line.find('|');
        std::string addr_part = (pipe_pos != std::string::npos) 
            ? line.substr(0, pipe_pos) 
            : line;
        std::string pin_part = (pipe_pos != std::string::npos) 
            ? line.substr(pipe_pos + 1) 
            : "";
        
        size_t colon_pos = addr_part.find(':');
        if (colon_pos != std::string::npos) {
            std::string ip = addr_part.substr(0, colon_pos);
            int port = std::stoi(addr_part.substr(colon_pos + 1));
            hosts.push_back(Host(ip, port, pin_part));
        } else {
            hosts.push_back(Host(addr_part, 47989, pin_part));
        }
    }
    
    file.close();
    return hosts;
}

void HostManager::saveHosts(const std::vector<Host>& hosts) {
    std::ofstream file(HOSTS_FILE);
    
    if (!file.is_open()) {
        printf("[HostManager] Failed to open %s for writing\n", HOSTS_FILE);
        return;
    }
    
    for (const auto& host : hosts) {
        file << host.ip << ":" << host.port << "|" << host.pin << "\n";
    }
    
    file.close();
}

std::string HostManager::getAddress(const Host& host) {
    return host.ip + ":" + std::to_string(host.port);
}

Host HostManager::parseAddress(const std::string& address) {
    size_t colon_pos = address.find(':');
    if (colon_pos != std::string::npos) {
        std::string ip = address.substr(0, colon_pos);
        int port = std::stoi(address.substr(colon_pos + 1));
        return Host(ip, port);
    }
    return Host(address);
}
