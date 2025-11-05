// SPDX-License-Identifier: AGPL-3.0-or-later
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

#ifndef GO_PERSISTER_HPP
#define GO_PERSISTER_HPP

#include <storage/Persister.hpp>
#include "raft/Raft.hpp"
#include <cstdint>

extern "C" int persister_go_invoke_callback(uintptr_t handle, void *state, int stateSize);
extern "C" int persister_go_read_callback(uintptr_t handle, void* buffer, int buffer_len);

class GoPersister : public zdb::Persister {
public:
    GoPersister(uintptr_t h);
    std::string loadBuffer() override;
    raft::PersistentState load() override;
    void save(const raft::PersistentState&) override;
    ~GoPersister() override;
private:
    uintptr_t handle;
};

#endif // GO_PERSISTER_HPP