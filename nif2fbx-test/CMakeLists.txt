add_executable(nif2fbx-test
	main.cpp
)
target_link_libraries(nif2fbx-test PRIVATE fbxsdk)

add_dependencies(nif2fbx-test fbxsdknif)
