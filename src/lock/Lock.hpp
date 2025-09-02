#ifndef LOCK_H
#define LOCK_H

#include "client/KVStoreClient.hpp"
#include <string>
#include "common/Types.hpp"

namespace zdb {

class Lock {
public:
    Lock(const Key key, KVStoreClient& c);
    void acquire();
    void release();
private:
    const Key lockKey;
    Value lockValue;
    KVStoreClient& client;
};

} // namespace zdb

#endif // LOCK_H
