#include "NetworkConfig.hpp"

NetworkConfig::NetworkConfig(bool r, double drop, double delay)
    : reliability(r), dropRate(drop), delayRate(delay), connected {true}, rng(std::random_device{}()), dist{0.0, 1.0} {}

bool NetworkConfig::reliable() {
    return reliability;
}

bool NetworkConfig::delay() {
    return !reliable() && dist(rng) < delayRate;
}

bool NetworkConfig::drop() {
    return !connected || !reliable() && dist(rng) < dropRate;
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
