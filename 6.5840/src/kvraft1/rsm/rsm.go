package rsm

import (
	"6.5840/labrpc"
	tester "6.5840/tester1"
	zdb "github.com/as-family/zdb"
)

func MakeRSM(servers []*labrpc.ClientEnd, me int, persister *tester.Persister, maxraftstate int, sm zdb.StateMachine) *zdb.RSM {
	return zdb.MakeRSM(servers, me, persister, maxraftstate, sm)
}
