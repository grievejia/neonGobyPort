set (DynAAMemoryHooksSourceCodes
	MemoryHooks.cpp
)
add_library (DynAAMemoryHooks STATIC ${DynAAMemoryHooksSourceCodes})
install (TARGETS DynAAMemoryHooks ARCHIVE DESTINATION lib)