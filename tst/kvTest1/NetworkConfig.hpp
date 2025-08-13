#ifndef NETWORK_CONFIG_H
#define NETWORK_CONFIG_H

#include <random>
#include <chrono>

class NetworkConfig {
public:
    NetworkConfig(bool r, double drop, double delay);
    bool reliable();
    bool delay();
    bool drop();
    std::chrono::microseconds delayTime();
private:
    bool reliability;
    double dropRate;
    double delayRate;
    std::default_random_engine rng;
    std::uniform_real_distribution<double> dist;
};

#endif // NETWORK_CONFIG_H