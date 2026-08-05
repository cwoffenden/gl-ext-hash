cmake_minimum_required(VERSION 3.22)

# mkdir out
# cmake -B out -G Xcode
# open out/gl-ext-hash.xcodeproj
#
# cmake -B out -DCMAKE_BUILD_TYPE=Release
# cmake --build out --config Release

project("gl-ext-hash" DESCRIPTION "OpenGL extension hash test")

include(CheckIPOSupported)

if (MSVC)
	# Need to remove CMake's own exception handling settings
	string(REGEX REPLACE "\/EH[a-z]+" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	add_compile_options(
		/W4 /WX
		/wd4576 /wd4201 /wd4706		# C99 type compound literal; anonymous struct/union; assignment in conditional
		/fp:precise /fp:except-
		$<$<CONFIG:DEBUG>:/Od>
		$<$<CONFIG:DEBUG>:/wd4189>	# -Wno-unused
		$<$<CONFIG:RELEASE>:/O2>
		$<$<CONFIG:RELEASE>:/Ot>	# favour fast code
		$<$<CONFIG:RELEASE>:/Oi>	# generate intrinsic functions
		$<$<CONFIG:RELEASE>:/GS->	# no buffer security checks
		$<$<CONFIG:RELEASE>:/Gy>	# enable function-level linking
		$<$<COMPILE_LANGUAGE:CXX>:/EHs-c->	# -fno-exceptions
		$<$<COMPILE_LANGUAGE:CXX>:/GR->		# -fno-rtti
	)
	add_link_options(
		$<$<CONFIG:RELEASE>:/OPT:REF>			# eliminate unreferenced functions
		$<$<CONFIG:RELEASE>:/OPT:ICF>			# merge identical functions
		$<$<CONFIG:RELEASE>:/INCREMENTAL:NO>	# disable incremental linking
		$<$<CONFIG:RELEASE>:/LTCG>				# not enabled with CMake's IPO
	)
	add_compile_definitions(
		NOMINMAX
		_HAS_EXCEPTIONS=0
		_CRT_SECURE_NO_WARNINGS
		$<$<CONFIG:DEBUG>:_DEBUG=1>
	)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:DEBUG>:DebugDLL>")
else()
	# TODO: look at which compilers will take -fno-signed-zeros (older GCC won't)
	add_compile_options(
		-Wall -Wextra -Werror
		-fno-math-errno
		$<$<CONFIG:DEBUG>:-O0>
		$<$<CONFIG:DEBUG>:-g3>
		$<$<CONFIG:DEBUG>:-Wno-unused>
		$<$<CONFIG:RELEASE>:-O3>
		$<$<CONFIG:RELEASE>:-g0>
		$<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
		$<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
	)
	add_compile_definitions(
		$<$<CONFIG:DEBUG>:_DEBUG=1>
	)
	if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "ppc64|powerpc")
		# We're on one of my own machines here (no one has PPC) so: add a tune
		# if the compiler accepts 'native', and the CPU for Macs (trying to
		# catch a G5 native build)
		if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
			if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 4.9)
				add_compile_options(-mtune=native)
				if (APPLE)
					if ("${CMAKE_SIZEOF_VOID_P}" STREQUAL "4")
						add_compile_options(-mcpu=native)
					endif()
				endif()
			endif()
		endif()
	endif()
endif()

check_ipo_supported(RESULT hasLTO)
if (hasLTO)
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()

file(GLOB INCS "*.h" "*.inl" "data/*.inl")
file(GLOB SRCS "*.c" "*.cpp")

source_group(TREE "${PROJECT_SOURCE_DIR}" PREFIX "Headers" FILES ${INCS})
source_group(TREE "${PROJECT_SOURCE_DIR}" PREFIX "Sources" FILES ${SRCS})

add_executable(${CMAKE_PROJECT_NAME} ${INCS} ${SRCS})

set_property(TARGET ${CMAKE_PROJECT_NAME} PROPERTY C_STANDARD 99)
add_compile_definitions(__STDC_LIMIT_MACROS)
