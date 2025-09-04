package zdb

import (
	"sync"

	"6.5840/labrpc"
	"6.5840/raftapi"
	tester "6.5840/tester1"
)

type Raft struct {
	mu        sync.Mutex
	peers     []*labrpc.ClientEnd
	me        int
	dead      int32
	persister *tester.Persister
	applyCh   chan raftapi.ApplyMsg
}

func (rf *Raft) GetState() (int, bool) {
	var term int
	var isleader bool
	return term, isleader
}

func (rf *Raft) Start(command interface{}) (int, int, bool) {
	index := -1
	term := -1
	isLeader := true
	return index, term, isLeader
}

func (rf *Raft) Kill() {
	// Your code here, if desired.
}

func (rf *Raft) Snapshot(index int, snapshot []byte) {
	// Your code here, if desired.
}

func (rf *Raft) PersistBytes() int {
	return rf.persister.RaftStateSize()
}

func Make(peers []*labrpc.ClientEnd, me int,
	persister *tester.Persister, applyCh chan raftapi.ApplyMsg) raftapi.Raft {
	rf := &Raft{}
	rf.peers = peers
	rf.me = me
	rf.persister = persister
	rf.applyCh = applyCh
	return rf
}
