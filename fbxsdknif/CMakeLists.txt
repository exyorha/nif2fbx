add_library(fbxsdknif SHARED
	BSplineTrackDefinition.h
	BSplineDataSet.cpp
	BSplineDataSet.h
	FBXNIFPlugin.cpp
	FBXNIFPlugin.h
	FBXNIFPluginNS.h
	FBXSceneWriter.cpp
	FBXSceneWriter.h
	main.cpp
	NIFReader.cpp
	NIFReader.h
	NIFUtils.cpp
	NIFUtils.h
	SkeletonProcessor.cpp
	SkeletonProcessor.h
)
target_link_libraries(fbxsdknif PRIVATE fbxsdk nifparse)
