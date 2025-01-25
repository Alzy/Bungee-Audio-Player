cmake_minimum_required(VERSION 3.25)

# This tells CMake we have goodies in the /cmake folder
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include (PamplejuceVersion)
include (CPM)
include(PamplejuceMacOS)
include(JUCEDefaults)

# Project settings
set(PROJECT_NAME "BungeeAudioPlayer")
set(PRODUCT_NAME "Bungee Audio Player")
set(COMPANY_NAME "AGM LTD")
set(BUNDLE_ID "com.agmltd.bungeeaudioplayer")

project(${PROJECT_NAME} VERSION ${CURRENT_VERSION})

# Add JUCE as a submodule
add_subdirectory(JUCE)

# Add GUI application
juce_add_gui_app(${PROJECT_NAME}
    PRODUCT_NAME "${PRODUCT_NAME}"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icon.png"
)

# Source files
file(GLOB_RECURSE SourceFiles CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/source/*.h")
target_sources(${PROJECT_NAME} PRIVATE ${SourceFiles})

# Include Bungee library headers
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

# Link Bungee library
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    "${CMAKE_SOURCE_DIR}/lib/bungee/Windows/Release/bungee.lib"
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
    juce_audio_utils
    juce_gui_basics
    juce_gui_extra
    juce_dsp
)

# Copy DLLs to the build directory after build
add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/bungee/Windows/Release"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
)

# Set compile definitions
target_compile_definitions(${PROJECT_NAME}
    PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
    VERSION="${CURRENT_VERSION}"
)

# Assets (optional, if you have binary data to embed)
include(Assets)

add_compile_options(-gdwarf-4)

# MacOS tweaks for Xcode (TODO: reactivate this for macs... I'm only targeting windows right now)
#include(XcodePrettify)
