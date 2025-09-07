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
#ifndef FULL_JITTER_H
#define FULL_JITTER_H

#include <random>
#include <chrono>

namespace zdb {

class FullJitter {
public:
    FullJitter();
    std::chrono::microseconds jitter(const std::chrono::microseconds v);
private:
    std::mt19937 rng;
};

} // namespace zdb

#endif // FULL_JITTER_H
