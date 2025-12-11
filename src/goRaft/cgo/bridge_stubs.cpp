// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdint>
#include <cstddef>

extern "C" {

// Default stub implementations for callbacks exported by Go code.
// When Go is not linked into the process, these provide safe defaults
// so the C++ shared library can be linked for unit tests.

int channel_go_invoke_callback(std::uintptr_t /*handle*/, void* /*cmd*/, int /*cmd_size*/, int /*index*/) {
    return -1; // indicate failure
}

int receive_channel_go_callback(std::uintptr_t /*handle*/, void* /*command*/) {
    return -1; // no data
}

void channel_close_callback(std::uintptr_t /*handle*/) {
    // no-op
}

int persister_go_read_callback(std::uintptr_t /*handle*/, void* /*buf*/, int /*buf_size*/) {
    return -1;
}

void persister_go_invoke_callback(std::uintptr_t /*handle*/, void* /*buf*/, int /*sz*/) {
    // no-op
}

int state_machine_go_apply_command(std::uintptr_t /*handle*/, void* /*cmd*/, int /*cmd_size*/) {
    return -1;
}

void go_invoke_callback(std::uintptr_t /*handle*/, std::uintptr_t /*cbHandle*/) {
    // no-op
}

} // extern "C"
