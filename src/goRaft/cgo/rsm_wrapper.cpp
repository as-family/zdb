#include "goRaft/cgo/rsm_wrapper.hpp"
#include "goRaft/cgo/RsmHandle.hpp"

RsmHandle* create_rsm(int id, int servers, uintptr_t clients[], uintptr_t persister, int maxraftstate, uintptr_t sm) {
    auto handle = new RsmHandle {
        id,
        servers,
        maxraftstate
    };
    return handle;
}
