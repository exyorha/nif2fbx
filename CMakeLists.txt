cmake_minimum_required(VERSION 3.12)
project(nif2fbx)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(FBX_SDK_ROOT "" CACHE PATH "FBX SDK Root Directory")
if(NOT FBX_SDK_ROOT)
	message(FATAL_ERROR "FBX SDK root directory must be specified with FBX_SDK_ROOT variable")
endif()

add_library(fbxsdk INTERFACE)
target_include_directories(fbxsdk INTERFACE "${FBX_SDK_ROOT}/include")
target_link_libraries(fbxsdk INTERFACE "${FBX_SDK_ROOT}/lib/vs2017/x64/release/libfbxsdk.lib")
target_compile_definitions(fbxsdk INTERFACE -DFBXSDK_SHARED)

if(${CMAKE_PROJECT_NAME} STREQUAL ${PROJECT_NAME})
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

	add_custom_target(copy_fbxsdk_dll ALL
		COMMAND
			cmake -E copy_if_different
			${FBX_SDK_ROOT}/lib/vs2017/x64/release/libfbxsdk.dll
			$<TARGET_FILE_DIR:nif2fbx-test>/libfbxsdk.dll
		VERBATIM
	)

	STRING(REPLACE "INCREMENTAL:YES" "INCREMENTAL:NO" replacementFlags ${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO})
	SET(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "/INCREMENTAL:NO ${replacementFlags}" )
	STRING(REPLACE "INCREMENTAL:YES" "INCREMENTAL:NO" replacementFlags ${CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO})
	SET(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO "/INCREMENTAL:NO ${replacementFlags}" )
endif()


add_subdirectory(nifparse/halffloat)
add_subdirectory(nifparse)
add_subdirectory(jsoncpp)
add_subdirectory(nif2fbxapi)
add_subdirectory(fbxsdknif)

if(${CMAKE_PROJECT_NAME} STREQUAL ${PROJECT_NAME})
	add_subdirectory(nif2fbx-test)
endif()
