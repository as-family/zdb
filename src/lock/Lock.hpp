#ifndef LOCK_H
#define LOCK_H

#include "client/KVStoreClient.hpp"
#include <string>
#include "common/Types.hpp"

namespace zdb {

class Lock {
public:
    Lock(Key key, KVStoreClient& c);
    bool acquire();
    bool release();
private:
    Key lock_key;
    KVStoreClient& client;
};

} // namespace zdb

#endif // LOCK_H
