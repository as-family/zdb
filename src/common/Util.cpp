// SPDX-License-Identifier: AGPL-3.0-or-later
/*
 * ZDB a distributed, fault-tolerant database.
 * Copyright (C) 2025 Ahmed Refaat Gadalla Mohamed
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#include "common/Util.hpp"
#include <algorithm>
#include <random>
#include <string_view>
#include <cstring>
#include <chrono>
#include <cstdint>

std::string zdb_generate_random_alphanumeric_string(std::size_t len) {
    static constexpr auto chars =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    thread_local auto rng = random_generator<>();
    auto dist = std::uniform_int_distribution{{}, std::strlen(chars) - 1};
    auto result = std::string(len, '\0');
    std::generate_n(begin(result), len, [&]() { return chars[dist(rng)]; });
    return result;
}

std::array<uint8_t, 16> generate_uuid_v7() {
    thread_local auto rng = random_generator<>();
    auto dist = std::uniform_int_distribution<uint8_t>{0, 255};
    
    std::array<uint8_t, 16> uuid{};
    
    // Get current time in milliseconds since Unix epoch
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    auto timestamp = static_cast<uint64_t>(ms);
    
    // Fill the first 48 bits (6 bytes) with timestamp
    uuid[0] = static_cast<uint8_t>((timestamp >> 40) & 0xFF);
    uuid[1] = static_cast<uint8_t>((timestamp >> 32) & 0xFF);
    uuid[2] = static_cast<uint8_t>((timestamp >> 24) & 0xFF);
    uuid[3] = static_cast<uint8_t>((timestamp >> 16) & 0xFF);
    uuid[4] = static_cast<uint8_t>((timestamp >> 8) & 0xFF);
    uuid[5] = static_cast<uint8_t>(timestamp & 0xFF);
    
    // Fill the remaining bytes with random data
    for (size_t i = 6; i < 16; ++i) {
        uuid[i] = dist(rng);
    }
    
    // Set version 7 (bits 12-15 of time_hi_and_version)
    uuid[6] = (uuid[6] & 0x0F) | 0x70;
    
    // Set variant bits (bits 6-7 of clock_seq_hi_and_reserved)
    uuid[8] = (uuid[8] & 0x3F) | 0x80;
    
    return uuid;
}

std::string uuid_v7_to_string(const std::array<uint8_t, 16>& uuid) {
    return std::string(reinterpret_cast<const char*>(uuid.data()), uuid.size());
}

std::array<uint8_t, 16> string_to_uuid_v7(const std::string& str) {
    std::array<uint8_t, 16> uuid{};
    if (str.size() == 16) {
        std::memcpy(uuid.data(), str.data(), 16);
    }
    return uuid;
}
