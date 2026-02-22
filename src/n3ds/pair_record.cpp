/*
 * This file is part of Moonlight Embedded.
 *
 * Copyright (C) 2015-2017 Iwan Timmer
 *
 * Moonlight is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Moonlight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Moonlight; if not, see <http://www.gnu.org/licenses/>.
 */

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <locale>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pair_record.hpp"

// trim from start (in place)
inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
                return !std::isspace(ch);
            }));
}

// trim from end (in place)
inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(),
                         [](unsigned char ch) { return !std::isspace(ch); })
                .base(),
            s.end());
}

// trim from both ends (in place)
void trim(std::string &s) {
    rtrim(s);
    ltrim(s);
}

void add_pair_address(std::string address, uint16_t port) {
    address += ":" + std::to_string(port);

    // Prevent duplicates
    auto address_list = list_paired_addresses();
    for (auto entry : address_list) {
        if (entry == address) {
            fprintf(stderr, "[pair_record] Already paired: %s\n", address.c_str());
            return;
        }
    }
    address_list.push_back(address);

    const char *address_file = MOONLIGHT_3DS_PATH "/paired";
    
    FILE *fd = fopen(address_file, "w");
    if (fd == NULL) {
        fprintf(stderr, "[pair_record] ERROR: Cannot write to %s\n", address_file);
        return;
    }

    bool success = true;
    for (auto addr_string : address_list) {
        trim(addr_string);
        if (fprintf(fd, "%s\n", addr_string.c_str()) < 0) {
            success = false;
            break;
        }
    }
    
    fclose(fd);
    if (success) {
        fprintf(stderr, "[pair_record] SAVED: %s\n", address.c_str());
    }
}

void remove_pair_address(std::string address, uint16_t port) {
    address += ":" + std::to_string(port);

    auto address_list = list_paired_addresses();

    const char *address_file = MOONLIGHT_3DS_PATH "/paired";
    
    FILE *fd = fopen(address_file, "w");
    if (fd == NULL) {
        fprintf(stderr, "[pair_record] ERROR: Cannot write to %s\n", address_file);
        return;
    }

    bool found = false;
    for (auto addr_string : address_list) {
        if (addr_string != address) {
            trim(addr_string);
            fprintf(fd, "%s\n", addr_string.c_str());
        } else {
            found = true;
        }
    }

    fclose(fd);
    if (found) {
        fprintf(stderr, "[pair_record] REMOVED: %s\n", address.c_str());
    }
}

// Replace an existing paired address with a new value. If the old address
// isn’t found, the list is left untouched. The new address is inserted in
// the original position so the ordering remains consistent.
void edit_pair_address(const std::string &old_address,
                       const std::string &new_address) {
    auto address_list = list_paired_addresses();

    const char *address_file = MOONLIGHT_3DS_PATH "/paired";

    FILE *fd = fopen(address_file, "w");
    if (fd == NULL) {
        fprintf(stderr, "[pair_record] ERROR: Cannot write to %s\n", address_file);
        return;
    }

    bool found = false;
    for (auto addr_string : address_list) {
        if (addr_string == old_address) {
            addr_string = new_address;
            found = true;
        }
        trim(addr_string);
        fprintf(fd, "%s\n", addr_string.c_str());
    }

    fclose(fd);
    if (found) {
        fprintf(stderr, "[pair_record] UPDATED: %s\n", new_address.c_str());
    }
}

std::vector<std::string> list_paired_addresses() {
    std::vector<std::string> addresses = std::vector<std::string>();
    std::ifstream pair_file(MOONLIGHT_3DS_PATH "/paired");
    
    if (!pair_file.is_open()) {
        fprintf(stderr, "[pair_record] Info: No paired devices found (file doesn't exist yet)\n");
        return addresses;
    }
    
    std::string line;
    while (std::getline(pair_file, line)) {
        if (!line.empty()) {
            trim(line);
            if (!line.empty()) {
                addresses.push_back(line);
            }
        }
    }
    
    pair_file.close();
    return addresses;
}
