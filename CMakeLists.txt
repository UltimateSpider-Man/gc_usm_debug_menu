cmake_minimum_required(VERSION 3.18)

project(usm_debug_menu_gamecube LANGUAGES NONE)

find_program(USM_POWERSHELL NAMES pwsh powershell REQUIRED)
add_custom_target(gamecube ALL
    COMMAND "${USM_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/build_gamecube.ps1"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Building and injecting the GameCube PowerPC payload"
    VERBATIM)
