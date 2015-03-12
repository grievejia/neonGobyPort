set (RCSPointerAnalysisSourceCodes
	PointerAnalysis.cpp
	BasicPointerAnalysis.cpp
	PointToDrawer.cpp
)
add_library (RCSPointerAnalysis SHARED ${RCSPointerAnalysisSourceCodes})