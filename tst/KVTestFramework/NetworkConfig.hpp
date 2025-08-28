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
    void setReliability(bool r);
    void disconnect();
    void connect();
    bool isConnected();
private:
    bool reliability;
    double dropRate;
    double delayRate;
    bool connected;
    std::default_random_engine rng;
    std::uniform_real_distribution<double> dist;
};

#endif // NETWORK_CONFIG_H
