#ifndef ZDB_TYPES_MAP_HPP
#define ZDB_TYPES_MAP_HPP

#include <type_traits>
#include <raft/Types.hpp>
#include <proto/kvStore.pb.h>

namespace zdb {
template<class T>
struct map_to;

template<> struct map_to<raft::AppendEntriesArg> { using type = raft::AppendEntriesReply; };
template<> struct map_to<raft::RequestVoteArg> { using type = raft::RequestVoteReply; };
template<> struct map_to<raft::proto::AppendEntriesArg> { using type = raft::proto::AppendEntriesReply; };
template<> struct map_to<raft::proto::RequestVoteArg> { using type = raft::proto::RequestVoteReply; };
template<> struct map_to<kvStore::GetRequest> { using type = kvStore::GetReply; };
template<> struct map_to<kvStore::SetRequest> { using type = kvStore::SetReply; };
template<> struct map_to<kvStore::EraseRequest> { using type = kvStore::EraseReply; };
template<> struct map_to<kvStore::SizeRequest> { using type = kvStore::SizeReply; };

template<class T>
using map_to_t = typename map_to<T>::type;

template<class T>
concept has_mapping = requires { typename map_to<T>::type; };

template<class T, class Default = void>
using mapped_or_t = std::conditional_t<has_mapping<T>, map_to_t<T>, Default>;

} // namespace zdb
#endif // ZDB_TYPES_MAP_HPP

