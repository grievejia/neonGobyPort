set (DynAAInstrumentersSourceCodes
	MemoryInstrumenter.cpp
	Preparer.cpp
)
add_library (DynAAInstrumenters SHARED ${DynAAInstrumentersSourceCodes})
target_link_libraries (DynAAInstrumenters RCSID DynAAUtils)
install (TARGETS DynAAInstrumenters LIBRARY DESTINATION lib)