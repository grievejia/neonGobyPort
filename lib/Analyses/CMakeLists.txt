set (DynAAAnalysesCodes
	AliasAnalysisChecker.cpp
	BaselineAliasAnalysis.cpp
	DynamicAliasAnalysis.cpp
#	DynamicPointerAnalysis.cpp
	ReductionVerifier.cpp
)
add_library (DynAAAnalyses SHARED ${DynAAAnalysesCodes})
target_link_libraries (DynAAAnalyses RCSID DynAAUtils)
install (TARGETS DynAAAnalyses LIBRARY DESTINATION lib)