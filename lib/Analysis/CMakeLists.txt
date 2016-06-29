set (DynamicAnalysisSourceCodes
	DynamicAliasAnalysis.cpp
)
add_library (DynamicAnalysis STATIC ${DynamicAnalysisSourceCodes})
target_link_libraries (DynamicAnalysis DynamicLog LLVMSupport)