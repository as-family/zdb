#include "Porcupine.hpp"
#include <nlohmann/json.hpp>
#include <mutex>
#include <spdlog/spdlog.h>
#include <fstream>
#include <unistd.h>
#include <sys/wait.h>

Porcupine::Porcupine() : operations() {}

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
    std::vector<Operation> v {operations};
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
        {"error", output.error}
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
    std::string filename = "porcupine_operations.json";
    std::ofstream ofs(filename);
    ofs << j.dump(2);
    ofs.close();
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        execl("./porcupine", "./porcupine", filename.c_str(), std::to_string(timeout).c_str(), "visualize", nullptr);
        perror("execl");
        exit(1);
    } else if (pid > 0) {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
        return status == 0;
    } else {
        // Fork failed
        perror("fork");
    }
    return false;
}
