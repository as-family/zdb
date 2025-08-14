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
    Porcupine();
    void append(Operation op);
    size_t size() const;
    std::vector<Operation> read() const;
    bool check(int timeout) const;
private:
    mutable std::mutex mtx;
    std::vector<Operation> operations;
};

#endif // PORCUPINE_H
