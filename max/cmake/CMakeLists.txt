cmake_minimum_required(VERSION 3.27)
project(mdeGranular VERSION 1.2 LANGUAGES C)

# Configure Max SDK
add_subdirectory(../sdk ./sdk)
set(C74SUPPORT_DIR ${PROJECT_SOURCE_DIR}/../sdk/source/max-sdk-base/c74support)
set(C74SCRIPT_DIR ${C74SUPPORT_DIR}/../script)
set(MAX_SDK_MAX_INCLUDES ${C74SUPPORT_DIR}/max-includes)
set(MAX_SDK_MSP_INCLUDES ${C74SUPPORT_DIR}/msp-includes)

if (APPLE)
	if (CMAKE_OSX_ARCHITECTURES STREQUAL "")
		set (CMAKE_OSX_ARCHITECTURES x86_64)
	endif()
	set (CMAKE_OSX_DEPLOYMENT_TARGET "10.11" CACHE STRING "Minimum OS X deployment version" FORCE)

	file (STRINGS "${C74SCRIPT_DIR}/max-linker-flags.txt" C74_SYM_MAX_LINKER_FLAGS)
	set (CMAKE_MODULE_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${C74_SYM_MAX_LINKER_FLAGS}")
endif()

add_library(mdeGranular MODULE)
set_target_properties(mdeGranular PROPERTIES OUTPUT_NAME mdeGranular~)

target_compile_definitions(mdeGranular PUBLIC MAXMSP)
target_include_directories(mdeGranular PUBLIC ${MAX_SDK_MAX_INCLUDES} ${MAX_SDK_MSP_INCLUDES})
target_sources(mdeGranular PRIVATE ${PROJECT_SOURCE_DIR}/../../src/mdeGranular~.c ${PROJECT_SOURCE_DIR}/../../src/mdeGranular~maxmsp.c)

if (WIN32)
	target_link_directories(mdeGranular PRIVATE ${C74SUPPORT_DIR}/max-includes/x64 ${C74SUPPORT_DIR}/msp-includes/x64)
	target_link_libraries(mdeGranular PRIVATE MaxAPI MaxAudio)
elseif (APPLE)
	find_library(
		MSP_LIBRARY "MaxAudioAPI"
		REQUIRED
		PATHS "${MAX_SDK_MSP_INCLUDES}"
		NO_DEFAULT_PATH
		NO_CMAKE_FIND_ROOT_PATH
	)
	target_link_libraries(mdeGranular PUBLIC ${MSP_LIBRARY})
	
	set_target_properties(mdeGranular PROPERTIES BUNDLE True)
	set_target_properties(mdeGranular PROPERTIES BUNDLE_EXTENSION "mxo")
	
	set_target_properties(mdeGranular PROPERTIES XCODE_ATTRIBUTE_WRAPPER_EXTENSION "mxo")
	set_target_properties(mdeGranular PROPERTIES MACOSX_BUNDLE_BUNDLE_VERSION "${VERSION}")
	set_target_properties(mdeGranular PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${C74SCRIPT_DIR}/Info.plist.in)
	set_target_properties(mdeGranular PROPERTIES XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${AUTHOR_DOMAIN}.${BUNDLE_IDENTIFIER}")
	
	# add_custom_command(
		# TARGET mdeGranular
		# POST_BUILD
		# COMMAND cp "${C74SCRIPT_DIR}/PkgInfo" "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/mdeGranular\~.mxo/Contents/PkgInfo"
		# COMMENT "Copy PkgInfo"
	# )
endif()
