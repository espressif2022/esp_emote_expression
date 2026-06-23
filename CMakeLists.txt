if(NOT COMMAND idf_component_register)
    cmake_minimum_required(VERSION 3.16)
    project(esp_emote_expression_host C)

    include(FetchContent)

    set(ESP_EMOTE_GFX_ROOT "" CACHE PATH
        "Optional override for esp_emote_gfx source tree (skips fetch)")
    set(ESP_EMOTE_GFX_GIT_REPO "https://github.com/espressif2022/esp_emote_gfx.git" CACHE STRING
        "Git repository for esp_emote_gfx when fetching")
    set(ESP_EMOTE_GFX_REF "feat/sdl_support" CACHE STRING
        "Git branch/tag for esp_emote_gfx when fetching from GitHub")

    function(gfx_expression_fetch_heatshrink gfx_root)
        set(_dest "${gfx_root}/managed_components/laride__heatshrink")
        if(EXISTS "${_dest}/heatshrink_decoder.c")
            return()
        endif()

        message(STATUS "Fetching heatshrink 0.4.1 from GitHub")
        FetchContent_Declare(
            gfx_expression_heatshrink
            GIT_REPOSITORY https://github.com/laride/heatshrink.git
            GIT_TAG        a0d2650edea4762ea2ba71b71a02d9ed81da1841
        )
        FetchContent_GetProperties(gfx_expression_heatshrink)
        if(NOT gfx_expression_heatshrink_POPULATED)
            FetchContent_Populate(gfx_expression_heatshrink)
        endif()

        file(MAKE_DIRECTORY "${_dest}")
        file(COPY "${gfx_expression_heatshrink_SOURCE_DIR}/" DESTINATION "${_dest}")
    endfunction()

    function(gfx_expression_fetch_emote_gfx out_root)
        if(ESP_EMOTE_GFX_ROOT AND EXISTS "${ESP_EMOTE_GFX_ROOT}/CMakeLists.txt")
            message(STATUS "Using esp_emote_gfx from ESP_EMOTE_GFX_ROOT: ${ESP_EMOTE_GFX_ROOT}")
            set(${out_root} "${ESP_EMOTE_GFX_ROOT}" PARENT_SCOPE)
            return()
        endif()

        message(STATUS "Fetching esp_emote_gfx ${ESP_EMOTE_GFX_REF} from GitHub")
        FetchContent_Declare(
            gfx_expression_emote_gfx
            GIT_REPOSITORY ${ESP_EMOTE_GFX_GIT_REPO}
            GIT_TAG        ${ESP_EMOTE_GFX_REF}
        )
        FetchContent_GetProperties(gfx_expression_emote_gfx)
        if(NOT gfx_expression_emote_gfx_POPULATED)
            FetchContent_Populate(gfx_expression_emote_gfx)
        endif()
        if(NOT EXISTS "${gfx_expression_emote_gfx_SOURCE_DIR}/CMakeLists.txt")
            message(FATAL_ERROR
                "Failed to fetch esp_emote_gfx (${ESP_EMOTE_GFX_REF}). "
                "Check network access or set -DESP_EMOTE_GFX_ROOT=/path/to/esp_emote_gfx.")
        endif()
        set(${out_root} "${gfx_expression_emote_gfx_SOURCE_DIR}" PARENT_SCOPE)
    endfunction()

    set(CJSON_GIT_REPO "https://github.com/DaveGamble/cJSON.git" CACHE STRING
        "Git repository for cJSON when fetching")
    set(CJSON_REF "v1.7.19" CACHE STRING
        "Git tag for cJSON when fetching from GitHub")

    function(gfx_expression_fetch_cjson out_target)
        if(TARGET gfx_host_cjson)
            set(${out_target} gfx_host_cjson PARENT_SCOPE)
            return()
        endif()

        message(STATUS "Fetching cJSON ${CJSON_REF} from GitHub")
        FetchContent_Declare(
            gfx_expression_cjson
            GIT_REPOSITORY ${CJSON_GIT_REPO}
            GIT_TAG        ${CJSON_REF}
        )
        FetchContent_GetProperties(gfx_expression_cjson)
        if(NOT gfx_expression_cjson_POPULATED)
            FetchContent_Populate(gfx_expression_cjson)
        endif()
        add_library(gfx_host_cjson STATIC "${gfx_expression_cjson_SOURCE_DIR}/cJSON.c")
        target_include_directories(gfx_host_cjson PUBLIC "${gfx_expression_cjson_SOURCE_DIR}")
        set(${out_target} gfx_host_cjson PARENT_SCOPE)
    endfunction()

    gfx_expression_fetch_emote_gfx(_esp_emote_gfx_root)
    gfx_expression_fetch_heatshrink("${_esp_emote_gfx_root}")
    gfx_expression_fetch_cjson(_gfx_host_cjson)

    add_subdirectory("${_esp_emote_gfx_root}" "${CMAKE_BINARY_DIR}/esp_emote_gfx")

    file(GLOB GFX_EXPRESSION_SRCS CONFIGURE_DEPENDS
        "src/*.c"
        "src/default_assets/*.c")

    add_library(gfx_host_expression STATIC ${GFX_EXPRESSION_SRCS})
    target_include_directories(gfx_host_expression
        PUBLIC
            "include"
            "priv_include"
            "${_esp_emote_gfx_root}/src"
            "${_esp_emote_gfx_root}/simulation/port/include"
    )
    target_compile_features(gfx_host_expression PUBLIC c_std_11)
    target_link_libraries(gfx_host_expression PUBLIC gfx_host_core ${_gfx_host_cjson})

    add_executable(gfx_host_expression_demo "simulation/host/gfx_host_expression_demo.c")
    target_compile_features(gfx_host_expression_demo PRIVATE c_std_11)
    target_link_libraries(gfx_host_expression_demo
        PRIVATE
            gfx_host_expression
            gfx_backend_sdl
            gfx_host_runner
    )

    include(CTest)
    if(BUILD_TESTING)
        add_test(NAME gfx_host_expression_dummy_smoke
            COMMAND ${CMAKE_COMMAND}
                -D GFX_HOST_SDL_DEMO=$<TARGET_FILE:gfx_host_expression_demo>
                -P ${_esp_emote_gfx_root}/simulation/host/run_sdl_dummy_smoke.cmake
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

        add_custom_target(gfx_host_expression_smoke
            COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R gfx_host_expression_dummy_smoke
            DEPENDS gfx_host_expression_demo
            USES_TERMINAL)
    endif()

    return()
endif()

set(COMPONENT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(COMPONENT_SRC_DIR ${COMPONENT_DIR}/src)
set(COMPONENT_INCLUDE_DIRS ${COMPONENT_DIR}/include)
set(COMPONENT_PRIVATE_INCLUDE_DIRS ${COMPONENT_DIR}/priv_include)
set(COMPONENT_SRCS_C "")
set(COMPONENT_SRCS_CPP "")
set(COMPONENT_SRCS_C_COMPILE_FLAGS "")
set(COMPONENT_SRCS_CPP_COMPILE_FLAGS "")
set(COMPONENT_SRCS_COMPILE_OPTIONS "")

#
# Expression
#
if(CMAKE_SCRIPT_MODE_FILE)
    file(GLOB_RECURSE EXPRESSION_SRCS_C ${COMPONENT_SRC_DIR}/*.c ${COMPONENT_DIR}/default_assets/*.c)
    file(GLOB_RECURSE EXPRESSION_SRCS_CPP ${COMPONENT_SRC_DIR}/*.cpp ${COMPONENT_DIR}/default_assets/*.cpp)
else()
    file(GLOB_RECURSE EXPRESSION_SRCS_C CONFIGURE_DEPENDS
        ${COMPONENT_SRC_DIR}/*.c ${COMPONENT_DIR}/default_assets/*.c)
    file(GLOB_RECURSE EXPRESSION_SRCS_CPP CONFIGURE_DEPENDS
        ${COMPONENT_SRC_DIR}/*.cpp ${COMPONENT_DIR}/default_assets/*.cpp)
endif()
list(APPEND COMPONENT_SRCS_C ${EXPRESSION_SRCS_C})
list(APPEND COMPONENT_SRCS_CPP ${EXPRESSION_SRCS_CPP})

#
# Register Component
#
idf_component_register(
    SRCS ${COMPONENT_SRCS_C} ${COMPONENT_SRCS_CPP}
    INCLUDE_DIRS ${COMPONENT_INCLUDE_DIRS}
    PRIV_INCLUDE_DIRS ${COMPONENT_PRIVATE_INCLUDE_DIRS}
    WHOLE_ARCHIVE
)

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})

#
# Compile Options
#
target_compile_options(${COMPONENT_LIB}
    PUBLIC
        ${COMPONENT_SRCS_COMPILE_OPTIONS}
)
set_source_files_properties(${COMPONENT_SRCS_C}
    PROPERTIES
        COMPILE_FLAGS "${COMPONENT_SRCS_C_COMPILE_FLAGS}"
)
set_source_files_properties(${COMPONENT_SRCS_CPP}
    PROPERTIES
        COMPILE_FLAGS "${COMPONENT_SRCS_CPP_COMPILE_FLAGS}"
)
