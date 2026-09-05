cmake_minimum_required(VERSION 3.21)

project(ForHonorStamina VERSION 0.0.1 LANGUAGES CXX)

# Output folder
if(DEFINED ENV{SKYRIM_FOLDER} AND IS_DIRECTORY "$ENV{SKYRIM_FOLDER}/Data")
    set(OUTPUT_FOLDER "$ENV{SKYRIM_FOLDER}/Data")
endif()

if(DEFINED ENV{SKYRIM_MODS_FOLDER} AND IS_DIRECTORY "$ENV{SKYRIM_MODS_FOLDER}")
    set(OUTPUT_FOLDER "$ENV{SKYRIM_MODS_FOLDER}/${PROJECT_NAME}")
endif()

# ============================================================
# Local CommonLibSSE-NG
# ============================================================

set(CommonLibPath "C:/Users/spenc/Documents/Code/GitHub/CommonLibSSE-NG")

# We only need CommonLib as a dependency; don't build its tests.
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)

add_subdirectory(
    ${CommonLibPath}
    CommonLibSSE
    EXCLUDE_FROM_ALL
)

include("${CommonLibPath}/cmake/CommonLibSSE.cmake")

# ============================================================
# SKSE Plugin
# ============================================================

add_commonlibsse_plugin(${PROJECT_NAME} SOURCES
    src/plugin.cpp
    src/Stamina.cpp
    src/Events.cpp
    src/Hooks/Hooks.cpp
    src/StaminaDamage.cpp
    src/Settings.cpp
    src/Menu.cpp
    src/PrecisionHandler.cpp
    src/MultiHittingBalance.cpp
    src/MovementSpeed.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Includes"
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)

target_precompile_headers(${PROJECT_NAME} PRIVATE
    src/PCH.h
)

# ============================================================
# Deployment
# ============================================================

if(DEFINED OUTPUT_FOLDER)

    set(DLL_FOLDER "${OUTPUT_FOLDER}/SKSE/Plugins")

    message(STATUS "SKSE plugin output folder: ${DLL_FOLDER}")

    add_custom_command(
        TARGET "${PROJECT_NAME}"
        POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${DLL_FOLDER}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:${PROJECT_NAME}>"
            "${DLL_FOLDER}/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
        VERBATIM
    )

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_custom_command(
            TARGET "${PROJECT_NAME}"
            POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_PDB_FILE:${PROJECT_NAME}>"
                "${DLL_FOLDER}/$<TARGET_PDB_FILE_NAME:${PROJECT_NAME}>"
            VERBATIM
        )
    endif()

endif()