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
#ifndef PORCUPINE_H
#define PORCUPINE_H

#include <cstdint>
#include <string>
#include "common/Error.hpp"
#include <chrono>
#include <vector>
#include <mutex>

class Porcupine {
public:
    static const uint8_t SET_OP = 1;
    static const uint8_t GET_OP = 2;
    struct Input {
        uint8_t op;
        std::string key;
        std::string value;
        uint64_t version;
    };
    struct Output {
        std::string value;
        uint64_t version;
        zdb::ErrorCode error;
    };
    struct Operation {
        Input input;
        Output output;
        std::chrono::time_point<std::chrono::steady_clock> start;
        std::chrono::time_point<std::chrono::steady_clock> end;
        int clientId;
    };
    void append(Operation op);
    size_t size() const;
    std::vector<Operation> read() const;
    bool check(int timeout) const;
private:
    mutable std::mutex mtx;
    std::vector<Operation> operations;
};

#endif // PORCUPINE_H
