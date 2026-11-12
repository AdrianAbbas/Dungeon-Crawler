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
# Two plug-ins from one source (Direction Update 3). Every source file is compiled into
# both; the processor reads JucePlugin_IsSynth to know which it is.
#   DungeonCrawler    DUNGEON CRAWLER, the instrument: aumu / Dgci / Veil, VST3 instrument
#   DungeonCrawlerFX  DUNGEON CRAWLER FX, the effect as before: aufx / Dgcr / Veil (its AU
#                     codes, bundle ID and parameter IDs unchanged, so projects keep working)
# File and bundle names have no spaces (simpler scripts); hosts show PLUGIN_NAME.
set(DC_SOURCES
    Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Engine.cpp
    Source/DSP/Abyss.cpp Source/DSP/AncientBlood.cpp Source/DSP/Choir.cpp
    Source/DSP/Wraith.cpp Source/DSP/Echoes.cpp Source/DSP/Crypt.cpp
    Source/DSP/Relics.cpp Source/DSP/Signature.cpp Source/DSP/Vault.cpp Source/DSP/Convolver.cpp Source/DSP/Ensemble.cpp Source/DSP/Atmosphere.cpp Source/DSP/DungeonSynth.cpp Source/DSP/DungeonSynthModern.cpp Source/World/Locations.cpp Source/World/Search.cpp
    Source/Instrument/SampleBank.cpp Source/Instrument/Synth.cpp Source/Instrument/Designs.cpp Source/Instrument/Captures.cpp Source/UI/InstrumentPanel.cpp
    Source/UI/EngravedLookAndFeel.cpp Source/UI/Engraving.cpp Source/UI/Scenes.cpp Source/UI/ScenePlaces.cpp Source/UI/SceneCache.cpp
    Source/UI/Painted.cpp Source/UI/SearchPanel.cpp Source/UI/MixPanel.cpp)
# Fonts (SIL Open Font License, assets/fonts): Cinzel for titles, Cormorant Garamond for labels.
juce_add_binary_data(DungeonCrawlerFonts NAMESPACE DungeonCrawlerFonts SOURCES
    assets/fonts/CormorantGaramond-SemiBold.ttf assets/fonts/CormorantGaramond-Bold.ttf assets/fonts/Cinzel-SemiBold.ttf)
set_target_properties(DungeonCrawlerFonts PROPERTIES POSITION_INDEPENDENT_CODE ON)

# The developer's recordings (private AdrianAbbas/Samples, prepared by
# tools/sample-prep/prepare.py) ship inside the instrument's bundles at
# Contents/Resources/Samples. Without them every sampled instrument plays its
# synthesised fallback. They are never committed to this repository.
set(DC_SAMPLES_DIR "" CACHE PATH "Prepared samples (tools/sample-prep/prepare.py --out)")
if(DC_SAMPLES_DIR AND EXISTS "${DC_SAMPLES_DIR}/index.json")
    get_filename_component(DC_SAMPLES_DIR "${DC_SAMPLES_DIR}" ABSOLUTE)
    message(STATUS "DUNGEON CRAWLER: shipping the samples in ${DC_SAMPLES_DIR}")
else()
    set(DC_SAMPLES_DIR "")
    message(STATUS "DUNGEON CRAWLER: no prepared samples (DC_SAMPLES_DIR); sampled instruments use their synthesised fallbacks")
endif()
# The effect's voice sets (index-fx.json, written by prepare.py).
set(DC_FX_SAMPLE_SETS "")
if(DC_SAMPLES_DIR AND EXISTS "${DC_SAMPLES_DIR}/index-fx.json")
    file(READ "${DC_SAMPLES_DIR}/index-fx.json" dc_fx_json)
    string(JSON dc_fx_count LENGTH "${dc_fx_json}" instruments)
    if(dc_fx_count GREATER 0)
        math(EXPR dc_fx_last "${dc_fx_count} - 1")
        foreach(dc_i RANGE ${dc_fx_last})
            string(JSON dc_fx_set GET "${dc_fx_json}" instruments ${dc_i})
            list(APPEND DC_FX_SAMPLE_SETS "${dc_fx_set}")
        endforeach()
    endif()
    message(STATUS "DUNGEON CRAWLER FX: shipping its voice sets ${DC_FX_SAMPLE_SETS}")
endif()

function(dc_add_plugin target)
    cmake_parse_arguments(P "" "NAME;CODE;BUNDLE;PREFIX;DESCRIPTION;SYNTH;AUTYPE;VST3CAT" "" ${ARGN})
    juce_add_plugin(${target}
        COMPANY_NAME "Veil Audio"
        BUNDLE_ID "${P_BUNDLE}"
        PLUGIN_MANUFACTURER_CODE Veil
        PLUGIN_CODE ${P_CODE}
        PRODUCT_NAME "${target}"
        PLUGIN_NAME "${P_NAME}"
        AU_EXPORT_PREFIX ${P_PREFIX}
        DESCRIPTION "${P_DESCRIPTION}"
        FORMATS ${DC_FORMATS}
        # The effect takes MIDI too (optional MIDI travel, Milestone 9) where hosts send
        # MIDI to effects; its AU stays an effect (aufx). The instrument plays MIDI.
        IS_SYNTH ${P_SYNTH}
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        VST3_CATEGORIES ${P_VST3CAT}
        AU_MAIN_TYPE ${P_AUTYPE}
        # Lets sandboxed hosts (GarageBand) load it in-process. It must still work
        # out-of-process, which the AU host harness checks.
        AU_SANDBOX_SAFE TRUE
        COPY_PLUGIN_AFTER_BUILD FALSE)
    target_sources(${target} PRIVATE ${DC_SOURCES})
    target_include_directories(${target} PUBLIC Source)
    target_compile_definitions(${target} PUBLIC
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0
        # The painted art (tools/painted builds it into assets/painted) ships inside every
        # bundle at Contents/Resources/Painted; development builds and the tests fall back
        # to the source tree. Likewise the samples (instrument only).
        DC_PAINTED_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/painted"
        DC_DEV_SAMPLES_DIR="${DC_SAMPLES_DIR}")
    target_link_libraries(${target} PRIVATE juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags DungeonCrawlerFonts)
    foreach(dc_format IN LISTS DC_FORMATS)
        set(dc_target ${target}_${dc_format})
        if(TARGET ${dc_target})
            if(dc_format STREQUAL "Standalone" AND NOT APPLE)
                set(dc_res "$<TARGET_FILE_DIR:${dc_target}>")
            else()
                set(dc_res "$<TARGET_FILE_DIR:${dc_target}>/../Resources")
            endif()
            add_custom_command(TARGET ${dc_target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/assets/painted" "${dc_res}/Painted"
                VERBATIM)
            if(P_SYNTH AND DC_SAMPLES_DIR)
                add_custom_command(TARGET ${dc_target} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E rm -rf "${dc_res}/Samples"
                    COMMAND ${CMAKE_COMMAND} -E copy_directory "${DC_SAMPLES_DIR}" "${dc_res}/Samples"
                    COMMAND ${CMAKE_COMMAND} -E rm -f "${dc_res}/Samples/report.json" "${dc_res}/Samples/index-fx.json" "${dc_res}/Samples/brass-report.json"
                    VERBATIM)
            elseif(DC_SAMPLES_DIR AND DC_FX_SAMPLE_SETS)
                # The effect ships only the voice it uses in its places (Session 8): the
                # sets listed in index-fx.json, which becomes its index.json.
                set(dc_fx_copy)
                foreach(dc_set IN LISTS DC_FX_SAMPLE_SETS)
                    list(APPEND dc_fx_copy
                        COMMAND ${CMAKE_COMMAND} -E copy_directory "${DC_SAMPLES_DIR}/${dc_set}" "${dc_res}/Samples/${dc_set}"
                        COMMAND ${CMAKE_COMMAND} -E copy "${DC_SAMPLES_DIR}/${dc_set}.json" "${dc_res}/Samples/${dc_set}.json")
                endforeach()
                add_custom_command(TARGET ${dc_target} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E rm -rf "${dc_res}/Samples"
                    ${dc_fx_copy}
                    COMMAND ${CMAKE_COMMAND} -E copy "${DC_SAMPLES_DIR}/index-fx.json" "${dc_res}/Samples/index.json"
                    VERBATIM)
            endif()
        endif()
    endforeach()
endfunction()

dc_add_plugin(DungeonCrawler
    NAME "DUNGEON CRAWLER" CODE Dgci BUNDLE "audio.veil.dungeoncrawler.instrument"
    PREFIX DungeonCrawlerInstAU DESCRIPTION "Dark-fantasy instruments, each played in its place"
    SYNTH TRUE AUTYPE kAudioUnitType_MusicDevice VST3CAT "Instrument;Synth")
dc_add_plugin(DungeonCrawlerFX
    NAME "DUNGEON CRAWLER FX" CODE Dgcr BUNDLE "audio.veil.dungeoncrawler"
    PREFIX DungeonCrawlerAU DESCRIPTION "Dark-fantasy location effect"
    SYNTH FALSE AUTYPE kAudioUnitType_Effect VST3CAT "Fx;Reverb")

if(DC_BUILD_TESTS)
    enable_testing()
    juce_add_console_app(DungeonCrawlerTests PRODUCT_NAME "DungeonCrawlerTests")
    # The linked plugin target exports the same setting using its format macro.
    get_target_property(dc_test_definitions DungeonCrawlerTests COMPILE_DEFINITIONS)
    list(REMOVE_ITEM dc_test_definitions JUCE_STANDALONE_APPLICATION=1)
    set_target_properties(DungeonCrawlerTests PROPERTIES COMPILE_DEFINITIONS "${dc_test_definitions}")
    target_sources(DungeonCrawlerTests PRIVATE Tests/DungeonCrawlerTests.cpp)
    target_link_libraries(DungeonCrawlerTests PRIVATE DungeonCrawlerFX juce::juce_audio_utils)
    target_compile_definitions(DungeonCrawlerTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
        DC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME dungeon_crawler_tests COMMAND DungeonCrawlerTests)
endif()
