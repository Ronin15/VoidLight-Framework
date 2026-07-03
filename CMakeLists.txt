cmake_minimum_required(VERSION 3.28)
project(VoidLight_Template VERSION 1.0.0 LANGUAGES C CXX)

# Set build configurations
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING
        "Choose the type of build (Debug, Release, or Profile)" FORCE)
endif()

# Define available build configurations
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Profile" CACHE STRING "" FORCE)

# Set output directory paths based on build type
set(OUTPUT_BASE_DIR "${CMAKE_SOURCE_DIR}/bin")
string(TOLOWER "${CMAKE_BUILD_TYPE}" _build_type_lower)
set(OUTPUT_DIR "${OUTPUT_BASE_DIR}/${_build_type_lower}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_DIR})

# Debug build settings
if(APPLE)
    # macOS-specific debug flags with dSYM generation and LLDB compatibility
    set(CMAKE_C_FLAGS_DEBUG 
        "-g3 -O0 -gdwarf-4 -glldb -fno-limit-debug-info -DDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG
        "-g3 -O0 -Wall -Wextra -Wpedantic -gdwarf-4 -glldb -fno-limit-debug-info -fstandalone-debug -DDEBUG")
    
    # Ensure debug symbols are preserved (don't strip them)
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} -g")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -g")
else()
    # Other platforms
    set(CMAKE_C_FLAGS_DEBUG 
        "${CMAKE_C_FLAGS_DEBUG} -g3 -O0 -DDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g3 -O0 -Wall -Wextra -Wpedantic -DDEBUG")
endif()

# Release build settings - keep minimal safe flags for dependencies (SDL3 etc.)
# Aggressive optimizations like -ffast-math are applied only to our targets below
set(CMAKE_C_FLAGS_RELEASE "-O3 -flto -fno-strict-aliasing -fomit-frame-pointer")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -flto -fno-strict-aliasing -fomit-frame-pointer")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -flto")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -flto")

if(APPLE)
    # Apple Silicon specific optimizations
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -mcpu=native")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mcpu=native")
    endif()

    # Aggressive flags for our project targets only (not SDL3)
    set(VOIDLIGHT_AGGRESSIVE_FLAGS
        "-ffunction-sections -fdata-sections -ffast-math -ftree-vectorize -funroll-loops")
else()
    # x86-64 linker plugin for LTO
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -fuse-linker-plugin")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -fuse-linker-plugin")

    # Aggressive flags for our project targets only (not SDL3)
    set(VOIDLIGHT_AGGRESSIVE_FLAGS
        "-ffunction-sections -fdata-sections -Wl,--gc-sections -ffast-math -ftree-vectorize \
        -funroll-loops -march=x86-64-v3 -mtune=generic -msse4.2 -mavx -mavx2 -mfma")
endif()

# Profile build settings - Valgrind-compatible optimized build
# Uses O2 optimization without AVX/AVX2 instructions that Valgrind struggles with
# Keeps debug symbols for profiling annotation
set(CMAKE_C_FLAGS_PROFILE "-O2 -g -fno-strict-aliasing -DNDEBUG")
set(CMAKE_CXX_FLAGS_PROFILE "-O2 -g -fno-strict-aliasing -DNDEBUG")

if(APPLE)
    set(VOIDLIGHT_PROFILE_FLAGS "-ffunction-sections -fdata-sections -ftree-vectorize")
else()
    # x86-64 Profile flags - SSE4.2 max (no AVX) for Valgrind compatibility
    set(VOIDLIGHT_PROFILE_FLAGS
        "-ffunction-sections -fdata-sections -ftree-vectorize -march=x86-64-v2 -mtune=generic -msse4.2")
endif()

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

option(USE_MOLD_LINKER "Use mold linker if available (Linux only)" ON)

if(USE_MOLD_LINKER AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_program(MOLD_LINKER mold)

    if(MOLD_LINKER)
        message(STATUS "Using mold linker: ${MOLD_LINKER}")

        if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.29")
            set(CMAKE_LINKER_TYPE MOLD)
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            add_link_options("-fuse-ld=mold")
        endif()
    else()
        message(WARNING "mold linker requested but not found")
    endif()
endif()

# Use ccache as a compiler launcher when available to speed up rebuilds.
# Applies to our code and FetchContent dependencies (SDL3, SDL3_ttf, SDL3_mixer).
option(USE_CCACHE "Use ccache compiler launcher if available" ON)

if(USE_CCACHE)
    find_program(CCACHE_PROGRAM ccache)

    if(CCACHE_PROGRAM)
        message(STATUS "Using ccache: ${CCACHE_PROGRAM}")
        set(CMAKE_C_COMPILER_LAUNCHER   "${CCACHE_PROGRAM}")
        set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    else()
        message(STATUS "ccache not found - compiling without ccache")
    endif()
endif()

# Export compile commands for use with tools like clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Internal-only headers live under src/. Expose that path privately to the
# object library so implementation files can include ai/internal/* without
# leaking into the public API surface.
set(INTERNAL_SRC_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/src")

# Copy compile_commands.json to project root after build
add_custom_target(copy-compile-commands ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_BINARY_DIR}/compile_commands.json
    ${CMAKE_SOURCE_DIR}/compile_commands.json
)
# Include FetchContent module
include(FetchContent)

# Avoid network updates during configure; use existing cached deps
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

# Set CMake policy for Boost module (CMP0167)
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

# Check for Boost unit test framework (optional, only needed for tests)
# SaveGameManager now uses header-only binary serialization (no Boost dependency)
find_package(Boost 1.80.0 QUIET COMPONENTS unit_test_framework)

if(Boost_FOUND AND TARGET Boost::unit_test_framework)
    message(STATUS "Found Boost Unit Test Framework - Tests will be enabled")
    set(HAVE_BOOST_TEST TRUE)
else()
    message(STATUS "Boost Unit Test Framework not found - Tests will be disabled")
    set(HAVE_BOOST_TEST FALSE)
endif()

message(STATUS 
    "SaveGameManager using fast header-only binary serialization")

# Specify the stable release tags for each SDL3 component
# SDL3 3.4.0 is first stable release - includes native PNG loading (SDL_LoadPNG)
set(SDL3_GIT_TAG "release-3.4.12")         # SDL3 current stable release
set(SDL3_TTF_GIT_TAG "release-3.2.2")      # SDL3_ttf current stable release
set(SDL3_MIXER_GIT_TAG "release-3.2.0")    # SDL3_mixer current stable release

# Download and configure SDL3
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG ${SDL3_GIT_TAG}
)

# Build options for SDL3
set(SDL_SHARED TRUE CACHE BOOL "Build a SDL shared library")
set(SDL_STATIC FALSE CACHE BOOL "Build a SDL static library")
set(SDL_TEST FALSE CACHE BOOL "Build the SDL test framework")

# Download and configure SDL3_ttf
FetchContent_Declare(
    SDL3_ttf
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
    GIT_TAG ${SDL3_TTF_GIT_TAG}
)

# Build options for SDL3_ttf
set(SDLTTF_SAMPLES FALSE CACHE BOOL "Build the SDL3_ttf sample programs")
set(SDLTTF_VENDORED TRUE CACHE BOOL "Use vendored third-party libraries")
set(SDLTTF_DEPS_SHARED FALSE CACHE BOOL "Use shared dependencies")

# Download and configure SDL3_mixer
FetchContent_Declare(
    SDL3_mixer
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git
    GIT_TAG ${SDL3_MIXER_GIT_TAG}
)

# Build options for SDL3_mixer
set(SDLMIXER_SAMPLES FALSE CACHE BOOL "Build the SDL3_mixer sample programs")
set(SDLMIXER_VENDORED TRUE CACHE BOOL "Use vendored third-party libraries")
set(SDLMIXER_DEPS_SHARED FALSE CACHE BOOL "Use shared dependencies")
# SDL3_mixer 3.2.0 vendored mpg123: its cmake exports the binary dir as the build-interface
# include path but never generates or copies mpg123.h there (only via install()).
# Disable mpg123 and rely on SDL_mixer's built-in drmp3 decoder for MP3 instead.
set(SDLMIXER_MP3_MPG123 OFF CACHE BOOL "" FORCE)

# Make the SDL3 packages available
FetchContent_MakeAvailable(SDL3 SDL3_ttf SDL3_mixer)

# Find all source files
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "src/core/*.cpp"
    "src/managers/*.cpp"
    "src/gameStates/*.cpp"
    "src/entities/*.cpp"
    "src/events/*.cpp"
    "src/collisions/*.cpp"
    "src/ai/*.cpp"
    "src/world/*.cpp"
    "src/utils/*.cpp"
    "src/controllers/**/*.cpp"
)

# Exclude VoidLightMain.cpp from library sources (contains main() which conflicts with test frameworks)
list(FILTER SOURCES EXCLUDE REGEX ".*VoidLightMain\\.cpp$")

# SDL3_GPU support is mandatory for this project.
# Add GPU module sources
file(GLOB GPU_SOURCES CONFIGURE_DEPENDS "src/gpu/*.cpp")
list(APPEND SOURCES ${GPU_SOURCES})

if(WIN32)
    set(HE_GPU_PREFERRED_DRIVER "direct3d12")
    set(HE_GPU_SHADER_FORMAT_DEFINE "HE_GPU_SHADERFORMAT_DXIL_AVAILABLE")
    set(HE_GPU_SHADER_OUTPUT_LABEL "DXIL")
elseif(APPLE)
    set(HE_GPU_PREFERRED_DRIVER "metal")
    set(HE_GPU_SHADER_FORMAT_DEFINE "HE_GPU_SHADERFORMAT_MSL_AVAILABLE")
    set(HE_GPU_SHADER_OUTPUT_LABEL "Metal")
else()
    set(HE_GPU_PREFERRED_DRIVER "vulkan")
    set(HE_GPU_SHADER_FORMAT_DEFINE "HE_GPU_SHADERFORMAT_SPIRV_AVAILABLE")
    set(HE_GPU_SHADER_OUTPUT_LABEL "SPIR-V")
endif()

# Find optional shader toolchain for regenerating checked-in shader binaries.
# Builds still succeed without these tools by using the prebuilt shader assets
# already present under res/shaders/.
set(VOIDLIGHT_CAN_COMPILE_GPU_SHADERS TRUE)

find_program(GLSLANG_VALIDATOR
    NAMES glslangValidator glslang
    HINTS ENV VULKAN_SDK
    PATH_SUFFIXES bin
)

if(NOT GLSLANG_VALIDATOR)
    set(VOIDLIGHT_CAN_COMPILE_GPU_SHADERS FALSE)
    message(WARNING "glslangValidator not found - using checked-in GPU shader binaries")
else()
    message(STATUS "Found glslangValidator: ${GLSLANG_VALIDATOR}")
endif()

if((WIN32 OR APPLE) AND VOIDLIGHT_CAN_COMPILE_GPU_SHADERS)
    find_program(SPIRV_CROSS
        NAMES spirv-cross
        HINTS ENV VULKAN_SDK /opt/homebrew/bin /usr/local/bin
        PATH_SUFFIXES bin
    )

    if(NOT SPIRV_CROSS)
        set(VOIDLIGHT_CAN_COMPILE_GPU_SHADERS FALSE)
        message(WARNING "spirv-cross not found - using checked-in GPU shader binaries")
    else()
        message(STATUS "Found spirv-cross: ${SPIRV_CROSS}")
    endif()
endif()

if(WIN32 AND VOIDLIGHT_CAN_COMPILE_GPU_SHADERS)
    find_program(DXC_EXECUTABLE
        NAMES dxc
        HINTS ENV DXC_PATH ENV WindowsSdkDir
        PATH_SUFFIXES bin bin/x64
    )

    if(NOT DXC_EXECUTABLE)
        set(VOIDLIGHT_CAN_COMPILE_GPU_SHADERS FALSE)
        message(WARNING "dxc not found - using checked-in GPU shader binaries")
    else()
        message(STATUS "Found dxc: ${DXC_EXECUTABLE}")
    endif()
endif()

# Shader source files
set(SHADER_SOURCES
    ${CMAKE_SOURCE_DIR}/res/shaders/sprite.vert.glsl
    ${CMAKE_SOURCE_DIR}/res/shaders/sprite.frag.glsl
    ${CMAKE_SOURCE_DIR}/res/shaders/text_alpha.frag.glsl
    ${CMAKE_SOURCE_DIR}/res/shaders/text_sdf.frag.glsl
    ${CMAKE_SOURCE_DIR}/res/shaders/color.vert.glsl
    ${CMAKE_SOURCE_DIR}/res/shaders/color.frag.glsl
    ${CMAKE_SOURCE_DIR}/res/shaders/composite.vert.glsl
    ${CMAKE_SOURCE_DIR}/res/shaders/composite.frag.glsl
)

set(GPU_SHADER_INTERMEDIATE_DIR "${CMAKE_BINARY_DIR}/generated_shaders")
file(MAKE_DIRECTORY ${GPU_SHADER_INTERMEDIATE_DIR})

if(VOIDLIGHT_CAN_COMPILE_GPU_SHADERS)
    # Compile each shader to the platform-native artifact.
    set(PLATFORM_SHADER_OUTPUTS "")
    foreach(SHADER_SOURCE ${SHADER_SOURCES})
        get_filename_component(SHADER_NAME ${SHADER_SOURCE} NAME)
        string(REPLACE ".glsl" ".spv" SHADER_SPV_NAME ${SHADER_NAME})
        string(REPLACE ".glsl" ".hlsl" SHADER_HLSL_NAME ${SHADER_NAME})
        string(REPLACE ".glsl" ".metal" SHADER_METAL_NAME ${SHADER_NAME})
        string(REPLACE ".glsl" ".dxil" SHADER_DXIL_NAME ${SHADER_NAME})

        if(WIN32)
            set(SHADER_SPV "${GPU_SHADER_INTERMEDIATE_DIR}/${SHADER_SPV_NAME}")
            set(SHADER_HLSL "${GPU_SHADER_INTERMEDIATE_DIR}/${SHADER_HLSL_NAME}")
            set(SHADER_FINAL_OUTPUT "${CMAKE_SOURCE_DIR}/res/shaders/${SHADER_DXIL_NAME}")

                add_custom_command(
                    OUTPUT ${SHADER_SPV}
                    COMMAND ${GLSLANG_VALIDATOR} -V ${SHADER_SOURCE} -o ${SHADER_SPV}
                    DEPENDS ${SHADER_SOURCE}
                    COMMENT "Compiling shader: ${SHADER_NAME} -> ${SHADER_SPV_NAME}"
                    VERBATIM
                )

                add_custom_command(
                    OUTPUT ${SHADER_HLSL}
                    COMMAND ${SPIRV_CROSS} ${SHADER_SPV} --hlsl --shader-model 60 --output ${SHADER_HLSL}
                    DEPENDS ${SHADER_SPV}
                    COMMENT "Cross-compiling shader: ${SHADER_SPV_NAME} -> ${SHADER_HLSL_NAME}"
                    VERBATIM
                )

                if(SHADER_NAME MATCHES "\\.vert\\.")
                    set(DXC_PROFILE "vs_6_0")
                else()
                    set(DXC_PROFILE "ps_6_0")
                endif()

                add_custom_command(
                    OUTPUT ${SHADER_FINAL_OUTPUT}
                    COMMAND ${DXC_EXECUTABLE} -T ${DXC_PROFILE} -E main -Fo ${SHADER_FINAL_OUTPUT} ${SHADER_HLSL}
                    DEPENDS ${SHADER_HLSL}
                    COMMENT "Compiling shader: ${SHADER_HLSL_NAME} -> ${SHADER_DXIL_NAME}"
                    VERBATIM
                )

                list(APPEND PLATFORM_SHADER_OUTPUTS ${SHADER_FINAL_OUTPUT})
            elseif(APPLE)
                set(SHADER_SPV "${GPU_SHADER_INTERMEDIATE_DIR}/${SHADER_SPV_NAME}")
                set(SHADER_FINAL_OUTPUT "${CMAKE_SOURCE_DIR}/res/shaders/${SHADER_METAL_NAME}")

                add_custom_command(
                    OUTPUT ${SHADER_SPV}
                    COMMAND ${GLSLANG_VALIDATOR} -V ${SHADER_SOURCE} -o ${SHADER_SPV}
                    DEPENDS ${SHADER_SOURCE}
                    COMMENT "Compiling shader: ${SHADER_NAME} -> ${SHADER_SPV_NAME}"
                    VERBATIM
                )

                if(SHADER_NAME MATCHES "\\.vert\\.")
                    set(METAL_ENTRY "vertexMain")
                    set(METAL_STAGE "vert")
                else()
                    set(METAL_ENTRY "fragmentMain")
                    set(METAL_STAGE "frag")
                endif()

                add_custom_command(
                    OUTPUT ${SHADER_FINAL_OUTPUT}
                    COMMAND ${SPIRV_CROSS} ${SHADER_SPV} --msl --output ${SHADER_FINAL_OUTPUT} --rename-entry-point main ${METAL_ENTRY} ${METAL_STAGE}
                    DEPENDS ${SHADER_SPV}
                    COMMENT "Cross-compiling shader: ${SHADER_SPV_NAME} -> ${SHADER_METAL_NAME}"
                    VERBATIM
                )

                list(APPEND PLATFORM_SHADER_OUTPUTS ${SHADER_FINAL_OUTPUT})
            else()
                set(SHADER_FINAL_OUTPUT "${CMAKE_SOURCE_DIR}/res/shaders/${SHADER_SPV_NAME}")

                add_custom_command(
                    OUTPUT ${SHADER_FINAL_OUTPUT}
                    COMMAND ${GLSLANG_VALIDATOR} -V ${SHADER_SOURCE} -o ${SHADER_FINAL_OUTPUT}
                    DEPENDS ${SHADER_SOURCE}
                    COMMENT "Compiling shader: ${SHADER_NAME} -> ${SHADER_SPV_NAME}"
                    VERBATIM
                )

                list(APPEND PLATFORM_SHADER_OUTPUTS ${SHADER_FINAL_OUTPUT})
            endif()
    endforeach()

    # Create shader compilation target
    add_custom_target(compile_shaders ALL DEPENDS ${PLATFORM_SHADER_OUTPUTS})
    message(STATUS "SDL3_GPU enabled - platform-native shaders will be compiled for ${HE_GPU_PREFERRED_DRIVER} (${HE_GPU_SHADER_OUTPUT_LABEL})")
else()
    message(STATUS "SDL3_GPU enabled - using checked-in shader binaries for ${HE_GPU_PREFERRED_DRIVER} (${HE_GPU_SHADER_OUTPUT_LABEL})")
endif()

# Create object library for shared compilation (without main())
add_library(VoidLightObjects OBJECT ${SOURCES})
target_include_directories(VoidLightObjects PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/core
    ${CMAKE_CURRENT_SOURCE_DIR}/include/managers
    ${CMAKE_CURRENT_SOURCE_DIR}/include/gameStates
    ${CMAKE_CURRENT_SOURCE_DIR}/include/entities
    ${CMAKE_CURRENT_SOURCE_DIR}/include/events
    ${CMAKE_CURRENT_SOURCE_DIR}/include/entities/playerStates
    ${CMAKE_CURRENT_SOURCE_DIR}/include/entities/npcStates
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ai
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ai/behaviors
    ${CMAKE_CURRENT_SOURCE_DIR}/include/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/include/world
    ${CMAKE_CURRENT_SOURCE_DIR}/include/collisions
    ${CMAKE_CURRENT_SOURCE_DIR}/include/gpu
)
target_include_directories(VoidLightObjects PRIVATE
    ${INTERNAL_SRC_INCLUDE}
)
target_link_libraries(VoidLightObjects PUBLIC
    SDL3::SDL3
    SDL3_ttf::SDL3_ttf
    SDL3_mixer::SDL3_mixer
)

# Pass project name to code for file logging paths
target_compile_definitions(VoidLightObjects PRIVATE
    VOIDLIGHT_APP_NAME="${PROJECT_NAME}"
)

target_compile_definitions(VoidLightObjects PUBLIC ${HE_GPU_SHADER_FORMAT_DEFINE})

if(TARGET compile_shaders)
    add_dependencies(VoidLightObjects compile_shaders)
endif()

# Apply aggressive optimization flags to our project targets only (not SDL3)
# These flags like -ffast-math conflict with SDL3's audio code
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    separate_arguments(_voidlight_opt_flags NATIVE_COMMAND "${VOIDLIGHT_AGGRESSIVE_FLAGS}")
    target_compile_options(VoidLightObjects PRIVATE ${_voidlight_opt_flags})
elseif(CMAKE_BUILD_TYPE STREQUAL "Profile")
    # Profile build: Valgrind-compatible optimization (no AVX, SSE4.2 max)
    separate_arguments(_voidlight_opt_flags NATIVE_COMMAND "${VOIDLIGHT_PROFILE_FLAGS}")
    target_compile_options(VoidLightObjects PRIVATE ${_voidlight_opt_flags})
endif()

# Create static library for tests
add_library(VoidLightLib STATIC $<TARGET_OBJECTS:VoidLightObjects>)
target_include_directories(VoidLightLib PUBLIC
    $<TARGET_PROPERTY:VoidLightObjects,INTERFACE_INCLUDE_DIRECTORIES>
)
target_link_libraries(VoidLightLib PUBLIC
    $<TARGET_PROPERTY:VoidLightObjects,INTERFACE_LINK_LIBRARIES>
)
target_compile_definitions(VoidLightLib PUBLIC
    $<TARGET_PROPERTY:VoidLightObjects,INTERFACE_COMPILE_DEFINITIONS>
)

# Include VoidLightMain.cpp (contains main()) only in the executable, not the library
add_executable(${PROJECT_NAME}
    $<TARGET_OBJECTS:VoidLightObjects>
    src/core/VoidLightMain.cpp
)

# Convenience alias for the dev loop: build and link just the main binary
add_custom_target(app DEPENDS ${PROJECT_NAME})

# Configure window icon
# Note: The icon is set programmatically via SDL_SetWindowIcon in GameEngine.cpp
# This section handles platform-specific icon integration
if(WIN32)
    # Add the icon to the executable on Windows
    set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_SOURCE_DIR}/res/img/icon.ico")

    # Generate a resource file that includes the icon
    configure_file(
    "${CMAKE_SOURCE_DIR}/platforms/windows/windows_icon.rc.in"
    "${CMAKE_BINARY_DIR}/windows_icon.rc"
    @ONLY
  )

    # Add the resource file to the executable
    target_sources(${PROJECT_NAME} PRIVATE
        "${CMAKE_BINARY_DIR}/windows_icon.rc")
elseif(APPLE)
    # For macOS, bundle the icon with the application
    set(APP_ICON_MACOS "${CMAKE_SOURCE_DIR}/res/img/icon.icns")
    if(EXISTS "${APP_ICON_MACOS}")
        set(MACOSX_BUNDLE_ICON_FILE icon.icns)
        set_source_files_properties(${APP_ICON_MACOS} PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources")
        message(STATUS "Using macOS icon: ${APP_ICON_MACOS}")
    else()
        message(STATUS "macOS icon not found at ${APP_ICON_MACOS} - "
                       "using default icon")
    endif()
endif()

# Enable SDL console output for all debug builds (all platforms)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${PROJECT_NAME} PRIVATE SDL_MAIN_HANDLED)
    message(STATUS "SDL console output enabled for debug build")
endif()

if(WIN32)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set_target_properties(${PROJECT_NAME} PROPERTIES
            WIN32_EXECUTABLE FALSE
        )
    else()
        # Use Windows subsystem for Release builds (no console)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            WIN32_EXECUTABLE TRUE
        )
    endif()
endif()

if(APPLE)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set_target_properties(${PROJECT_NAME} PROPERTIES
            MACOSX_BUNDLE FALSE      # Don't create a bundle in debug mode
            BUILD_WITH_INSTALL_RPATH TRUE
            BUILD_RPATH "@executable_path"
            INSTALL_RPATH "@executable_path"
            # Enable dSYM generation for debug symbols
            XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym"
            XCODE_ATTRIBUTE_GCC_GENERATE_DEBUGGING_SYMBOLS YES
        )

        # Generate dSYM bundle for debugging with verbose output
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND dsymutil $<TARGET_FILE:${PROJECT_NAME}> 
                -o $<TARGET_FILE:${PROJECT_NAME}>.dSYM
            COMMAND echo "dSYM bundle created at: $<TARGET_FILE:${PROJECT_NAME}>.dSYM"
            COMMENT "Generating dSYM bundle for debugging"
        )
        
        # Prevent accidental stripping of debug symbols
        set_target_properties(${PROJECT_NAME} PROPERTIES
            XCODE_ATTRIBUTE_STRIP_INSTALLED_PRODUCT NO
            XCODE_ATTRIBUTE_COPY_PHASE_STRIP NO
            XCODE_ATTRIBUTE_STRIP_STYLE "non-global"
        )

        message(STATUS "dSYM generation enabled for debug symbols")
        message(STATUS "LLDB debugging flags enabled (-glldb, -fstandalone-debug)")
    else()
        # For release builds, create a proper macOS application bundle
        # Set bundle metadata
        set(MACOSX_BUNDLE_BUNDLE_NAME "${PROJECT_NAME}")
        set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.voidlight.${PROJECT_NAME}")
        set(MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}")
        set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")

        set_target_properties(${PROJECT_NAME} PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/platforms/macos/Info.plist.in"
            BUILD_WITH_INSTALL_RPATH TRUE
            BUILD_RPATH "@executable_path/../Frameworks;@executable_path"
            INSTALL_RPATH "@executable_path/../Frameworks;@executable_path"
        )

        # Copy SDL3 dylibs to app bundle Frameworks directory for release
        # Use TARGET_SONAME_FILE to get the versioned symlink name (e.g., libSDL3_ttf.0.dylib)
        # which is what the binary actually links against
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/Frameworks"
            # Copy SDL3 (already has correct soname)
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_SONAME_FILE:SDL3::SDL3>
                "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/Frameworks/"
            # Copy SDL3_ttf with soname
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_SONAME_FILE:SDL3_ttf::SDL3_ttf>
                "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/Frameworks/"
            # Copy SDL3_mixer with soname
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_SONAME_FILE:SDL3_mixer::SDL3_mixer>
                "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/Frameworks/"
        )

        # Copy resources to app bundle
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/res"
                "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/Resources/res"
            COMMENT "Copying resources to app bundle"
        )

        # Code sign the app bundle (required for Game Mode and other macOS features)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>"
            COMMENT "Code signing app bundle"
        )
    endif()
endif()

# Link against SDL3 libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    $<TARGET_PROPERTY:VoidLightObjects,INTERFACE_LINK_LIBRARIES>
)

# Specify include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    $<TARGET_PROPERTY:VoidLightObjects,INTERFACE_INCLUDE_DIRECTORIES>
)

# Propagate compile definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE
    $<TARGET_PROPERTY:VoidLightObjects,INTERFACE_COMPILE_DEFINITIONS>
)

# Copy SDL3 runtime libraries to build directory (Windows-specific)
if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3>
            $<TARGET_FILE:SDL3_ttf::SDL3_ttf>
            $<TARGET_FILE:SDL3_mixer::SDL3_mixer>
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
endif()

# Install SDL3 DLLs on Windows
if(WIN32)
    install(FILES
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE:SDL3_ttf::SDL3_ttf>
        $<TARGET_FILE:SDL3_mixer::SDL3_mixer>
        DESTINATION bin
    )
endif()

# Create output directory if it doesn't exist
file(MAKE_DIRECTORY ${OUTPUT_DIR})

# Display status message indicating successful configuration
message(STATUS
    "SDL3, SDL3_ttf, and SDL3_mixer have been configured successfully")
message(STATUS "${CMAKE_BUILD_TYPE} binaries will be output to: ${OUTPUT_DIR}")

# Enable testing only if Boost Test is available (tests require Boost.Test framework)
if(HAVE_BOOST_TEST)
    enable_testing()
    add_subdirectory(tests)
    message(STATUS "Tests enabled - Boost Test found")
else()
    message(STATUS 
        "Tests disabled - Boost Test not found (required for test framework)")
endif()
