cmake_minimum_required(VERSION 3.12)
project(jsoncpp)

find_package(PythonInterp REQUIRED)

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/include/json.h
	       ${CMAKE_CURRENT_BINARY_DIR}/include/json-forwards.h
		   ${CMAKE_CURRENT_BINARY_DIR}/json.cpp
	COMMAND cmake -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/include
	COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/jsoncpp/amalgamate.py
	        -s ${CMAKE_CURRENT_BINARY_DIR}/json.cpp
			-i ${CMAKE_CURRENT_BINARY_DIR}/include/json.h
			-t ${CMAKE_CURRENT_SOURCE_DIR}/jsoncpp
	VERBATIM
)

add_library(jsoncpp STATIC  
	${CMAKE_CURRENT_BINARY_DIR}/include/json.h
	${CMAKE_CURRENT_BINARY_DIR}/include/json-forwards.h
	${CMAKE_CURRENT_BINARY_DIR}/json.cpp
)
target_include_directories(jsoncpp PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/include)

