#include "NetworkConfig.hpp"

NetworkConfig::NetworkConfig(bool r, double drop, double delay)
    : reliability(r), dropRate(drop), delayRate(delay), connected {true}, rng(std::random_device{}()), dist{0.0, 1.0} {}

bool NetworkConfig::isReliable() const {
    return reliability;
}

bool NetworkConfig::shouldDelay() const {
    return !isReliable() && dist(rng) < delayRate;
}

bool NetworkConfig::shouldDrop() const {
    return !connected || !isReliable() && dist(rng) < dropRate;
}

std::chrono::microseconds NetworkConfig::delayTime() {
    return std::chrono::microseconds(static_cast<int>(dist(rng) * 40000 + 10000));
}

void NetworkConfig::setReliability(bool r) {
    reliability = r;
}

void NetworkConfig::connect() {
    connected = true;
}

void NetworkConfig::disconnect() {
    connected = false;
}

bool NetworkConfig::isConnected() {
    return connected;
}
