add_executable(aa-check aa-check.cpp)
target_link_libraries(aa-check Instrument DynamicAnalysis LLVMAnalysis LLVMSupport LLVMIRReader)