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
#include <fstream>
#include <locale>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "pair_record.hpp"

// Create directory if it doesn't exist
static int ensure_directory_exists(const char* path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        #ifdef _WIN32
            return mkdir(path) == 0 ? 0 : -1;
        #else
            return mkdir(path, 0755) == 0 ? 0 : -1;
        #endif
    }
    return 0;
}

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

    // Ensure directory exists
    if (ensure_directory_exists(MOONLIGHT_3DS_PATH) != 0) {
        fprintf(stderr, "[pair_record] ERROR: Failed to create directory %s\n", MOONLIGHT_3DS_PATH);
        return;
    }

    // Prevent duplicates
    auto address_list = list_paired_addresses();
    for (auto entry : address_list) {
        if (entry == address) {
            return;
        }
    }
    address_list.push_back(address);

    char *address_file = (char *)MOONLIGHT_3DS_PATH "/paired";
    remove(address_file);

    FILE *fd = fopen(address_file, "w");
    if (fd == NULL) {
        fprintf(stderr, "[pair_record] ERROR: Failed to open file %s for writing\n", address_file);
        return;
    }

    for (auto addr_string : address_list) {
        trim(addr_string);
        if (fprintf(fd, "%s\n", addr_string.c_str()) < 0) {
            fprintf(stderr, "[pair_record] ERROR: Failed to write to file %s\n", address_file);
            fclose(fd);
            return;
        }
    }
    
    if (fclose(fd) != 0) {
        fprintf(stderr, "[pair_record] ERROR: Failed to close file %s\n", address_file);
    } else {
        fprintf(stderr, "[pair_record] Successfully saved paired address: %s\n", address.c_str());
    }
}

void remove_pair_address(std::string address, uint16_t port) {
    address += ":" + std::to_string(port);

    // Ensure directory exists
    if (ensure_directory_exists(MOONLIGHT_3DS_PATH) != 0) {
        fprintf(stderr, "[pair_record] ERROR: Failed to create directory %s\n", MOONLIGHT_3DS_PATH);
        return;
    }

    auto address_list = list_paired_addresses();

    char *address_file = (char *)MOONLIGHT_3DS_PATH "/paired";
    remove(address_file);

    FILE *fd = fopen(address_file, "w");
    if (fd == NULL) {
        fprintf(stderr, "[pair_record] ERROR: Failed to open file %s for writing\n", address_file);
        return;
    }

    for (auto addr_string : address_list) {
        if (addr_string != address) {
            trim(addr_string);
            if (fprintf(fd, "%s\n", addr_string.c_str()) < 0) {
                fprintf(stderr, "[pair_record] ERROR: Failed to write to file %s\n", address_file);
                fclose(fd);
                return;
            }
        }
    }

    if (fclose(fd) != 0) {
        fprintf(stderr, "[pair_record] ERROR: Failed to close file %s\n", address_file);
    } else {
        fprintf(stderr, "[pair_record] Successfully removed paired address: %s\n", address.c_str());
    }
}

// Replace an existing paired address with a new value. If the old address
// isn’t found, the list is left untouched. The new address is inserted in
// the original position so the ordering remains consistent.
void edit_pair_address(const std::string &old_address,
                       const std::string &new_address) {
    // Ensure directory exists
    if (ensure_directory_exists(MOONLIGHT_3DS_PATH) != 0) {
        fprintf(stderr, "[pair_record] ERROR: Failed to create directory %s\n", MOONLIGHT_3DS_PATH);
        return;
    }

    auto address_list = list_paired_addresses();

    char *address_file = (char *)MOONLIGHT_3DS_PATH "/paired";
    remove(address_file);

    FILE *fd = fopen(address_file, "w");
    if (fd == NULL) {
        fprintf(stderr, "[pair_record] ERROR: Failed to open file %s for writing\n", address_file);
        return;
    }

    for (auto addr_string : address_list) {
        if (addr_string == old_address) {
            addr_string = new_address;
        }
        trim(addr_string);
        if (fprintf(fd, "%s\n", addr_string.c_str()) < 0) {
            fprintf(stderr, "[pair_record] ERROR: Failed to write to file %s\n", address_file);
            fclose(fd);
            return;
        }
    }

    if (fclose(fd) != 0) {
        fprintf(stderr, "[pair_record] ERROR: Failed to close file %s\n", address_file);
    } else {
        fprintf(stderr, "[pair_record] Successfully edited paired address\n");
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
