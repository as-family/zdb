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
#include "Porcupine.hpp"
#include <nlohmann/json.hpp>
#include <mutex>
#include <spdlog/spdlog.h>
#include <fstream>
#include <unistd.h>
#include <sys/wait.h>
#include <cstdlib>
#include <cstdio>
#include <ctime>

void Porcupine::append(Operation op) {
    std::lock_guard<std::mutex> lock(mtx);
    operations.push_back(op);
}

size_t Porcupine::size() const {
    std::lock_guard<std::mutex> lock(mtx);
    return operations.size();
}

std::vector<Porcupine::Operation> Porcupine::read() const {
    std::lock_guard<std::mutex> lock(mtx);
    std::vector<Operation> v{operations};
    return v;
}

void to_json(nlohmann::json& j, const Porcupine::Input& input) {
    j = nlohmann::json{
        {"op", input.op},
        {"key", input.key},
        {"value", input.value},
        {"version", input.version}
    };
}

void to_json(nlohmann::json& j, const Porcupine::Output& output) {
    j = nlohmann::json{
        {"value", output.value},
        {"version", output.version},
        {"error", static_cast<int>(output.error)}
    };
}

void to_json(nlohmann::json& j, const Porcupine::Operation& op) {
    j = nlohmann::json{
        {"input", op.input},
        {"output", op.output},
        {"start", op.start.time_since_epoch().count()},
        {"end", op.end.time_since_epoch().count()},
        {"clientId", op.clientId}
    };
}

bool Porcupine::check(int timeout) const {
    std::vector<Operation> ops = read();
    nlohmann::json j = ops;
    std::string filename = "/tmp/porcupine_operations_" + std::to_string(::getpid()) +
        "_" + std::to_string(::time(nullptr)) + ".json";
    std::ofstream ofs(filename, std::ios::trunc);
    if (!ofs) {
        spdlog::error("failed to open {} for writing", filename);
        return false;
    }
    ofs << j.dump(2) << '\n';
    ofs.close();
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        execl("./porcupine", "./porcupine", filename.c_str(), std::to_string(timeout).c_str(), "visualize", nullptr);
        perror("execl");
        exit(1);
    } else if (pid > 0) {
        int status = 0;
        const pid_t w = waitpid(pid, &status, 0);
        if (w == -1) {
            perror("waitpid");
            return false;
        }
        if (WIFEXITED(status)) {
            const int code = WEXITSTATUS(status);
            if (code != 0) {
                spdlog::error("porcupine exited with code {}", code);
            }
            return code == 0;
        }
        if (WIFSIGNALED(status)) {
            spdlog::error("porcupine terminated by signal {}", WTERMSIG(status));
        }
        return false;
    } else {
        // Fork failed
        perror("fork");
    }
    return false;
}
