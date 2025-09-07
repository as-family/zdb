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
    return !connected || (!isReliable() && dist(rng) < dropRate);
}

std::chrono::microseconds NetworkConfig::delayTime() {
    return std::chrono::microseconds{static_cast<int>(dist(rng) * 40000 + 10000)};
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
