add_executable(ng_dump_log dump_log.cpp)
target_link_libraries(ng_dump_log DynAAUtilsStatic LLVMCore LLVMSupport)
install (TARGETS ng_dump_log RUNTIME DESTINATION bin)