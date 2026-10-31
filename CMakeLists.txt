cmake_minimum_required(VERSION 3.22)
# macOS defaults: one universal binary that loads on Apple Silicon and Intel Macs
# back to 10.15. These must be set before project(); override on the command line.
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Target architectures")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")
endif()
project(DUNGEON_CRAWLER VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
option(DC_BUILD_TESTS "Build the offline processor tests" ON)
set(JUCE_PATH "" CACHE PATH "Existing JUCE 8.0.16 source (otherwise fetched)")
if(JUCE_PATH)
    add_subdirectory("${JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG ea77444750de45e036e0279bb0291144bf8e3688 # JUCE 8.0.16
        GIT_SHALLOW FALSE)
    FetchContent_MakeAvailable(JUCE)
endif()
set(DC_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND DC_FORMATS AU)
endif()
# File and bundle names use "DungeonCrawler" (no space: simpler scripts); hosts show
# PLUGIN_NAME, "Veil Audio: DUNGEON CRAWLER".
juce_add_plugin(DungeonCrawler
    COMPANY_NAME "Veil Audio"
    BUNDLE_ID "audio.veil.dungeoncrawler"
    PLUGIN_MANUFACTURER_CODE Veil
    PLUGIN_CODE Dgcr
    PRODUCT_NAME "DungeonCrawler"
    PLUGIN_NAME "DUNGEON CRAWLER"
    AU_EXPORT_PREFIX DungeonCrawlerAU
    DESCRIPTION "Dark-fantasy location effect"
    FORMATS ${DC_FORMATS}
    # An audio effect: audio in, audio out, no MIDI.
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Reverb
    AU_MAIN_TYPE kAudioUnitType_Effect
    # Lets sandboxed hosts (GarageBand) load it in-process. It must still work
    # out-of-process, which the AU host harness checks.
    AU_SANDBOX_SAFE TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(DungeonCrawler PRIVATE
    Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Engine.cpp
    Source/DSP/Abyss.cpp Source/DSP/AncientBlood.cpp Source/DSP/Choir.cpp
    Source/DSP/Wraith.cpp Source/DSP/Echoes.cpp Source/DSP/Crypt.cpp
    Source/UI/EngravedLookAndFeel.cpp)
target_include_directories(DungeonCrawler PUBLIC Source)
target_compile_definitions(DungeonCrawler PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0)
target_link_libraries(DungeonCrawler PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags)
if(DC_BUILD_TESTS)
    enable_testing()
    juce_add_console_app(DungeonCrawlerTests PRODUCT_NAME "DungeonCrawlerTests")
    # The linked plugin target exports the same setting using its format macro.
    get_target_property(dc_test_definitions DungeonCrawlerTests COMPILE_DEFINITIONS)
    list(REMOVE_ITEM dc_test_definitions JUCE_STANDALONE_APPLICATION=1)
    set_target_properties(DungeonCrawlerTests PROPERTIES COMPILE_DEFINITIONS "${dc_test_definitions}")
    target_sources(DungeonCrawlerTests PRIVATE Tests/DungeonCrawlerTests.cpp)
    target_link_libraries(DungeonCrawlerTests PRIVATE DungeonCrawler juce::juce_audio_utils)
    target_compile_definitions(DungeonCrawlerTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
    add_test(NAME dungeon_crawler_tests COMMAND DungeonCrawlerTests)
endif()
