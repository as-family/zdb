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
#include "raft/Types.hpp"

#include "common/Command.hpp"
#include "raft/Log.hpp"

namespace raft {

AppendEntriesArg::AppendEntriesArg(const proto::AppendEntriesArg& arg)
    : leaderId(arg.leaderid()),
      term(arg.term()),
      prevLogIndex(arg.prevlogindex()),
      prevLogTerm(arg.prevlogterm()),
      leaderCommit(arg.leadercommit()) {
    for (const auto& entry : arg.entries()) {
        auto e = LogEntry {entry.index(), entry.term(), zdb::commandFactory(entry.command())};
        entries.append(e);
    }
}

AppendEntriesArg::AppendEntriesArg(std::string l, uint64_t t, uint64_t pi, uint64_t pt, uint64_t c, const Log& g)
    : leaderId{l},
      term {t},
      prevLogIndex{pi},
      prevLogTerm{pt},
      leaderCommit {c},
      entries{g.data()} {}

RequestVoteArg::RequestVoteArg(const proto::RequestVoteArg& arg)
    : candidateId(arg.candidateid()),
      term(arg.term()),
      lastLogIndex(arg.lastlogindex()),
      lastLogTerm(arg.lastlogterm()) {}

} // namespace raft
