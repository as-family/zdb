#include "Porcupine.hpp"
#include <mutex>

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
