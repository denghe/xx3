cmake_minimum_required(VERSION 3.24)

# Default to release build if not specified
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()

set(name examples_window)
project(${name})

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


include_directories(
	src/
#	../../include
#	../../deps
#	../../glfw/include
)

file(GLOB sources
	src/*.cpp
	src/*.c
	src/*.h
	src/*.hpp
	src/*.inc
#	../../include/*.h
)

add_executable(${name} ${sources})

source_group("" FILES ${sources})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/../../ FILES ${sources})

if (WIN32)
	target_link_libraries(${name} winmm)
endif()



SET(BUILD_TESTING OFF CACHE BOOL "BUILD_TESTING" FORCE)
SET(USE_MSVC_RUNTIME_LIBRARY_DLL OFF CACHE BOOL "USE_MSVC_RUNTIME_LIBRARY_DLL" FORCE)
SET(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "ZSTD_BUILD_PROGRAMS" FORCE)
SET(ZSTD_BUILD_SHARED OFF CACHE BOOL "ZSTD_BUILD_SHARED" FORCE)
SET(ZSTD_BUILD_TESTS OFF CACHE BOOL "ZSTD_BUILD_SHARED" FORCE)
SET(ZSTD_LEGACY_SUPPORT OFF CACHE BOOL "ZSTD_LEGACY_SUPPORT" FORCE)
SET(ZSTD_MULTITHREAD_SUPPORT_DEFAULT OFF CACHE BOOL "ZSTD_MULTITHREAD_SUPPORT_DEFAULT" FORCE)
SET(ZSTD_MULTITHREAD_SUPPORT OFF CACHE BOOL "ZSTD_MULTITHREAD_SUPPORT" FORCE)
add_subdirectory(../../zstd/build/cmake zstd)


file(GLOB xx_sources
	${CMAKE_CURRENT_SOURCE_DIR}/../../include/*.h
	${CMAKE_CURRENT_SOURCE_DIR}/../../include/*.natvis
	${CMAKE_CURRENT_SOURCE_DIR}/../../src/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/../../src/*.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../deps/*.h
	${CMAKE_CURRENT_SOURCE_DIR}/../../deps/*.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../deps/*.cpp
)
add_library(xx
    ${xx_sources}
)
target_include_directories(xx PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/../../glfw/include
	${CMAKE_CURRENT_SOURCE_DIR}/../../include
	${CMAKE_CURRENT_SOURCE_DIR}/../../deps
)
target_link_libraries(xx libzstd_static)
target_link_libraries(${name} xx)



SET(GLFW_BUILD_DOCS OFF CACHE BOOL "GLFW_BUILD_DOCS" FORCE)
SET(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "GLFW_BUILD_EXAMPLES" FORCE)
SET(GLFW_BUILD_TESTS OFF CACHE BOOL "GLFW_BUILD_TESTS" FORCE)
SET(GLFW_INSTALL OFF CACHE BOOL "GLFW_INSTALL" FORCE)
add_subdirectory(../../glfw glfw)
target_link_libraries(${name} glfw)



if(MSVC)

	# target_link_options(${name} PRIVATE "/SUBSYSTEM:WINDOWS")
	add_compile_definitions(UNICODE _UNICODE)
	add_compile_options("/utf-8")

	target_link_options(${name} PRIVATE
		$<$<CONFIG:MinSizeRel>:/LTCG>
		$<$<CONFIG:Release>:/LTCG>
		$<$<CONFIG:RelWithDebInfo>:/LTCG>
	)

	target_compile_options(${name} PRIVATE
		"$<$<CONFIG:MinSizeRel>:/GL>"
		"$<$<CONFIG:Release>:/GL>"
		"$<$<CONFIG:RelWithDebInfo>:/GL>"
	)

	target_link_options(${name} PRIVATE
		$<$<CONFIG:Debug>:/NODEFAULTLIB:libcmtd.lib>
		$<$<CONFIG:MinSizeRel>:/NODEFAULTLIB:libcmt.lib>
		$<$<CONFIG:Release>:/NODEFAULTLIB:libcmt.lib>
		$<$<CONFIG:RelWithDebInfo>:/NODEFAULTLIB:libcmt.lib>
	)
	
	target_compile_definitions(${name} PRIVATE
		"$<$<CONFIG:MinSizeRel>:_SECURE_SCL=0>"
		"$<$<CONFIG:MinSizeRel>:_HAS_ITERATOR_DEBUGGING=0>"
		"$<$<CONFIG:Release>:_SECURE_SCL=0>"
		"$<$<CONFIG:Release>:_HAS_ITERATOR_DEBUGGING=0>"
		"$<$<CONFIG:RelWithDebInfo>:_SECURE_SCL=0>"
		"$<$<CONFIG:RelWithDebInfo>:_HAS_ITERATOR_DEBUGGING=0>"
	)

	add_compile_options($<$<PLATFORM_ID:Windows>:/MP${Biogears_BUILD_PARALLEL_LEVEL}>)
	target_compile_options(${name} PRIVATE
		$<$<PLATFORM_ID:Windows>:/fp:fast>
		$<$<PLATFORM_ID:Windows>:/std:c17>
		$<$<PLATFORM_ID:Windows>:/bigobj>
		$<$<PLATFORM_ID:Windows>:/MP${Biogears_BUILD_PARALLEL_LEVEL}>
	)
	
	
	target_compile_options(xx PRIVATE
		$<$<PLATFORM_ID:Windows>:/MP${Biogears_BUILD_PARALLEL_LEVEL}>
	)



	set_target_properties(${name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

	set_source_files_properties(src/pch.cpp PROPERTIES COMPILE_FLAGS "/Ycpch.h")
	target_compile_options(${name} PRIVATE $<$<PLATFORM_ID:Windows>:/Yupch.h>)
	
else()

	target_precompile_headers(${name}
		PUBLIC $<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/src/pch.h>
	)
	
	target_compile_options(${name} PRIVATE -Wno-invalid-offsetof)

endif()
