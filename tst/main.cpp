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

#include <memory>
#include <vector>
#include <gtest/gtest.h>
#include <spdlog/common.h>
#include "spdlog/async.h"
#include "spdlog/async_logger.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

int main(int argc, char** argv) {
    spdlog::init_thread_pool(8192, 1);
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        "logs/zdb.txt", 1024 * 1024 * 5, 3);
    std::vector<spdlog::sink_ptr> sinks {consoleSink, fileSink};
    const auto asyncLogger = std::make_shared<spdlog::async_logger>(
        "gAsync", sinks.begin(), sinks.end(),
        spdlog::thread_pool(), spdlog::async_overflow_policy::overrun_oldest);
    spdlog::register_logger(asyncLogger);
    spdlog::set_default_logger(asyncLogger);
    testing::InitGoogleTest(&argc, argv);
    const int result = RUN_ALL_TESTS();
    spdlog::shutdown();
    return result;
}
