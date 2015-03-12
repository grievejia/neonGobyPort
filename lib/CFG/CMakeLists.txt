set (RCSCFGSourceCodes
	CallGraphDrawer.cpp
	Exec.cpp
	ExecOnce.cpp
	FPCallGraph.cpp
	ICFG.cpp
	ICFGBuilder.cpp
	IdentifyThreadFuncs.cpp
	InterReach.cpp
	IntraReach.cpp
	MBB.cpp
	PartialICFGBuilder.cpp
)
add_library (RCSCFG SHARED ${RCSCFGSourceCodes})