#include "goRaft/cgo/rsm_wrapper.hpp"
#include "goRaft/cgo/RsmHandle.hpp"
#include <goRaft/cgo/GoChannel.hpp>
#include <goRaft/cgo/GoPersister.hpp>
#include <common/KVStateMachine.hpp>
#include <storage/InMemoryKVStore.hpp>
#include <goRaft/cgo/RaftHandle.hpp>

RsmHandle* create_rsm(int id, int servers, uintptr_t rpc, uintptr_t channel, uintptr_t persister, int maxraftstate, uintptr_t sm) {
    auto handle = new RsmHandle {
        id,
        servers,
        maxraftstate,
        channel,
        persister
    };
    handle->raftHandle = create_raft(id, servers, rpc, channel, persister);
    if (handle->raftHandle == nullptr) {
        delete handle;
        return nullptr;
    }
    handle->goChannel = handle->raftHandle->goChannel;
    handle->persister = handle->raftHandle->persister;
    handle->storageEngine = std::make_unique<zdb::InMemoryKVStore>();
    handle->rsm = std::make_unique<zdb::KVStateMachine>(*handle->storageEngine.get(), *handle->goChannel.get(), *handle->raftHandle->raft.get());
    return handle;
}
