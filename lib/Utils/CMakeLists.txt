set (DynAAUtilsSourceCodes
	LogDumper.cpp
	LogProcessor.cpp
	Utils.cpp
)
add_library (DynAAUtilsObj OBJECT ${DynAAUtilsSourceCodes})
add_library (DynAAUtils SHARED $<TARGET_OBJECTS:DynAAUtilsObj>)
add_library (DynAAUtilsStatic STATIC $<TARGET_OBJECTS:DynAAUtilsObj>)
install (TARGETS DynAAUtils LIBRARY DESTINATION lib)