#pragma once

#include <string>
#include <vector>

struct Host {
    std::string ip;
    int port;
    std::string pin;  // verification code/PIN
    
    Host() : port(47989) {}
    Host(const std::string& ip_addr, int p = 47989, const std::string& pin_code = "") 
        : ip(ip_addr), port(p), pin(pin_code) {}
};

class HostManager {
public:
    static const char* HOSTS_FILE;
    
    // Load hosts from file
    static std::vector<Host> loadHosts();
    
    // Save hosts to file
    static void saveHosts(const std::vector<Host>& hosts);
    
    // Get complete address string (ip:port)
    static std::string getAddress(const Host& host);
    
    // Parse address string back to Host
    static Host parseAddress(const std::string& address);
};
