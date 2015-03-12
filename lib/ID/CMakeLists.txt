set (RCSIDSourceCodes
	IDAssigner.cpp
	IDLookUp.cpp
	IDManager.cpp
	IDTagger.cpp
	ValueRenaming.cpp
)
add_library (RCSID SHARED ${RCSIDSourceCodes})
install (TARGETS RCSID LIBRARY DESTINATION lib)