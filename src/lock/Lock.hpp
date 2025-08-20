#ifndef LOCK_H
#define LOCK_H

#include "client/KVStoreClient.hpp"
#include <string>
#include "common/Types.hpp"
#include <mutex>

namespace zdb {

class Lock {
public:
    Lock(const Key key, KVStoreClient& c);
    void acquire();
    void release();
private:
    const Key lock_key;
    KVStoreClient& client;
    std::mutex m;
    std::string clientID;
};

} // namespace zdb

#endif // LOCK_H
