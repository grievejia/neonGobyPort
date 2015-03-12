set (DynAATransformsSourceCodes
	Reducer.cpp
)
add_library (DynAATransforms SHARED ${DynAATransformsSourceCodes})
target_link_libraries (DynAATransforms RCSID)