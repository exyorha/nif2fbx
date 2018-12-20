add_library(fbxsdknif SHARED
	FBXNIFPlugin.cpp
	FBXNIFPlugin.h
	FBXNIFPluginNS.h
	FBXSceneWriter.cpp
	FBXSceneWriter.h
	main.cpp
	NIFReader.cpp
	NIFReader.h
)
target_link_libraries(fbxsdknif PRIVATE fbxsdk nifparse)
