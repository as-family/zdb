#ifndef NETWORK_CONFIG_H
#define NETWORK_CONFIG_H

#include <random>
#include <chrono>

class NetworkConfig {
public:
    NetworkConfig(bool r, double drop, double delay);
    bool isReliable() const;
    bool shouldDelay() const;
    bool shouldDrop() const;
    std::chrono::microseconds delayTime();
    void setReliability(bool r);
    void disconnect();
    void connect();
    bool isConnected();
private:
    bool reliability;
    double dropRate;
    double delayRate;
    bool connected;
    mutable std::default_random_engine rng;
    mutable std::uniform_real_distribution<double> dist;
};

#endif // NETWORK_CONFIG_H
