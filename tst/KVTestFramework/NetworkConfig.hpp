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
