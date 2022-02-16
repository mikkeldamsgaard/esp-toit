cmake_minimum_required(VERSION 3.5)

# Register without sources, this will create an INTERFACE lib and we can then specify link options later
idf_component_register(INCLUDE_DIRS "toit/src" "include"
        PRIV_INCLUDE_DIRS ${PRIV_INCLUDES}
        REQUIRES "esp_adc_cal" "mbedtls" "bt" "app_update" "ulp")


file(GLOB toit_core_SRC
        "toit/src/*.h"
        "toit/src/*.c"
        "toit/src/*.cc"
        )

list(FILTER toit_core_SRC EXCLUDE REGEX "/(toit|toit_run_image).cc$")

file(GLOB toit_resources_SRC
        "toit/src/resources/*.h"
        "toit/src/resources/*.cc"
        )

file(GLOB toit_event_sources_SRC
        "toit/src/event_sources/*.h"
        "toit/src/event_sources/*.cc"
        )

file(GLOB toit_api_source_SRC
        "include/*.h"
        "src/*.cc")

file(GLOB_RECURSE toit_project_SRC FOLLOW_SYMLINKS
        "${CMAKE_HOME_DIRECTORY}/toit/*.toit")

list(APPEND srcs ${toit_resources_SRC} ${toit_event_sources_SRC} ${toit_core_SRC} ${toit_api_source_SRC})

set(TOIT_SDK_DIR ${COMPONENT_PATH}/toit/build/host/sdk)
set(TOIT_COMPILER ${TOIT_SDK_DIR}/bin/toit.compile)
set(TOIT_INTERPRETER ${TOIT_SDK_DIR}/bin/toit.run)


function(toit_postprocess target_name component_path)
    set(TOIT_INTERPRETER ${component_path}/toit/build/host/sdk/bin/toit.run)

    if (EXISTS ${CMAKE_HOME_DIRECTORY}/toit/main.toit)
        add_custom_target(config.json
                COMMAND echo "'{\"wifi\":{\"ssid\":\"${CONFIG_ESP32_WIFI_SSID}\",\"password\":\"${CONFIG_ESP32_WIFI_PASSWORD}\"}}'" > config.json
                )

        idf_build_get_property(elf_name EXECUTABLE_NAME GENERATOR_EXPRESSION)

        add_custom_command(TARGET ${target_name}
                POST_BUILD
                COMMAND echo "Merging config.json into ${elf_name}.bin"
                COMMAND ${TOIT_INTERPRETER} ${component_path}/toit/tools/inject_config.toit config.json ${elf_name}.bin
                )

        add_dependencies(${target_name} config.json)
    endif ()
endfunction()

if (EXISTS ${CMAKE_HOME_DIRECTORY}/toit/main.toit)

    if (NOT EXISTS ${TOIT_SDK_DIR})
        message(FATAL_ERROR "Missing Toit host SDK dir. Please run 'make' in the toit dir ${COMPONENT_PATH}/toit")
    endif ()

    if (CONFIG_ESP_TOIT_USE_JAGUAR)
        set(JAG_BIN ${COMPONENT_PATH}/jaguar/build/jag)
        set(TOIT_PKG ${TOIT_SDK_DIR}/bin/toit.pkg)

        if (NOT EXISTS ${JAG_BIN})
            message(FATAL_ERROR "Missing Jaguar binary. Please run 'make' in the jaguar dir ${COMPONENT_PATH}/jaguar")
        endif ()

        file(GLOB toit_jaguar_source_SRC
                "${COMPONENT_PATH}/jaguar/src/*.toit"
                "${COMPONENT_PATH}/jaguar/package.*")

        message("pw=${CONFIG_ESP32_WIFI_PASSWORD} use_jag=${CONFIG_ESP_TOIT_USE_JAGUAR}")

        add_custom_command(OUTPUT toit_program.s
                COMMAND ${TOIT_PKG} install --project-root=${COMPONENT_PATH}/jaguar
                COMMAND ${TOIT_COMPILER} -w toit_program.s --project-root ${COMPONENT_PATH}/jaguar ${COMPONENT_PATH}/jaguar/src/jaguar.toit
                DEPENDS ${toit_jaguar_source_SRC}
                VERBATIM)

        add_custom_command(OUTPUT toit_image.S
                COMMAND ${TOIT_INTERPRETER} ${TOIT_SDK_DIR}/snapshots/snapshot_to_image.snapshot toit_program.s toit_image.S
                DEPENDS toit_program.s
                VERBATIM)
    else ()
        add_custom_command(OUTPUT toit_program.s
                COMMAND ${TOIT_COMPILER} -w toit_program.s --project-root ${CMAKE_HOME_DIRECTORY}/toit ${CMAKE_HOME_DIRECTORY}/toit/main.toit
                DEPENDS ${toit_project_SRC}
                VERBATIM)

        add_custom_command(OUTPUT toit_image.S
                COMMAND ${TOIT_INTERPRETER} ${TOIT_SDK_DIR}/snapshots/snapshot_to_image.snapshot toit_program.s toit_image.S
                DEPENDS toit_program.s
                VERBATIM)
    endif ()


    add_library(toit_image OBJECT toit_image.S)

    enable_language(C ASM)

    target_compile_definitions(${COMPONENT_LIB} INTERFACE
            -DMBEDTLS_SSL_IN_CONTENT_LEN=6000
            -DMBEDTLS_SSL_OUT_CONTENT_LEN=3700
            -DMBEDTLS_PLATFORM_MEMORY=1
            -DDEPLOY
            -DESP32)

    target_compile_options(${COMPONENT_LIB} INTERFACE -UMBEDTLS_CONFIG_FILE)

    # Setup the static library, that needs to be --whole-archive included
    add_library(toit_real STATIC ${srcs})
    target_link_libraries(${COMPONENT_LIB} INTERFACE $<TARGET_OBJECTS:toit_image>)
    target_include_directories(toit_real PUBLIC include toit/src)
    #target_include_directories(${COMPONENT_LIB} PUBLIC include)
    set_property(TARGET toit_real PROPERTY CXX STANDARD 11)


    target_compile_definitions(toit_real
            PRIVATE -DRAW=1 -D__FREERTOS__=1
            PUBLIC -DMBEDTLS_SSL_IN_CONTENT_LEN=6000 -DMBEDTLS_SSL_OUT_CONTENT_LEN=3700 -DMBEDTLS_PLATFORM_MEMORY=1
            -DDEPLOY
            -DESP32
            )

    target_compile_options(toit_real PRIVATE
            -mlongcalls
            -fno-exceptions
            -ffunction-sections
            -fdata-sections
            -Os
            -Wno-sign-compare
            -Wall
            -Wno-extra
            -fno-rtti
            -Wno-error=format
            PUBLIC
            -UMBEDTLS_CONFIG_FILE
            )

    set(TOIT_GIT_VERSION "$ENV{TOIT_GIT_VERSION}")
    if ("${TOIT_GIT_VERSION}" STREQUAL "")
        execute_process(COMMAND sh -c "grep VM_GIT_VERSION ${COMPONENT_PATH}/toit/build/host/build.ninja  | head -1 | tr ' ' '\n' | grep VM_GIT_VERSION | cut -c 21- | sed 's/...$//'"
                OUTPUT_VARIABLE TOIT_GIT_VERSION)
        string(STRIP ${TOIT_GIT_VERSION} TOIT_GIT_VERSION)
    endif()

    set_source_files_properties(toit/src/interpreter.cc PROPERTIES COMPILE_FLAGS "-O3 ${TOIT_INTERPRETER_FLAGS} $ENV{LOCAL_INTERPRETER_CXXFLAGS}")
    set_source_files_properties(toit/src/utils.cc PROPERTIES COMPILE_FLAGS "-DTOIT_MODEL=\"\\\"${TOIT_MODEL}\\\"\" -DVM_GIT_INFO=\"\\\"${VM_GIT_INFO}\\\"\" -DVM_GIT_VERSION=\"\\\"${TOIT_GIT_VERSION}\\\"\"")

    # Some very specific code, to copy all IDF dependencies to the toit_real target
    __component_get_property(reqs ${component_target} __REQUIRES)
    foreach (req ${reqs})
        if (req IN_LIST build_component_targets)
            __component_get_property(req_lib ${req} COMPONENT_LIB)
            set_property(TARGET toit_real APPEND PROPERTY LINK_LIBRARIES ${req_lib})
            set_property(TARGET toit_real APPEND PROPERTY INTERFACE_LINK_LIBRARIES ${req_lib})
        endif ()
    endforeach ()
    add_dependencies(${COMPONENT_LIB} toit_real)

    # Hook up the interface component lib to the static whole archive lib
    #target_link_libraries(${COMPONENT_LIB} INTERFACE toit_real)
    target_link_options(${COMPONENT_LIB} INTERFACE -Wl,--whole-archive esp-idf/esp-toit/libtoit_real.a -Wl,--no-whole-archive)
endif ()

