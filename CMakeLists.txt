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

list(APPEND srcs ${toit_resources_SRC} ${toit_event_sources_SRC} ${toit_core_SRC} ${toit_api_source_SRC} )



set(TOIT_SDK_DIR ${COMPONENT_PATH}/toit/build/host/sdk)
set(TOIT_COMPILER ${TOIT_SDK_DIR}/bin/toit.compile)
set(TOIT_INTERPRETER ${TOIT_SDK_DIR}/bin/toit.run)

if (NOT EXISTS ${TOIT_SDK_DIR})
    message(FATAL_ERROR "Missing Toit host SDK dir. Please run 'make' in the toit dir ${COMPONENT_PATH}/toit" )
endif()

add_custom_command(OUTPUT toit_program.s
        COMMAND ${TOIT_COMPILER} -w toit_program.s ${CMAKE_HOME_DIRECTORY}/toit/main.toit
        DEPENDS ${CMAKE_HOME_DIRECTORY}/toit/main.toit
        VERBATIM)

add_custom_command(OUTPUT toit_image.S
        COMMAND ${TOIT_INTERPRETER} ${TOIT_SDK_DIR}/snapshots/snapshot_to_image.snapshot toit_program.s toit_image.S
        DEPENDS toit_program.s
        VERBATIM)

add_library(toit_image OBJECT toit_image.S)

enable_language(C ASM)

target_compile_definitions(${COMPONENT_LIB} INTERFACE
        -DMBEDTLS_SSL_IN_CONTENT_LEN=6000
        -DMBEDTLS_SSL_OUT_CONTENT_LEN=3700
        -DMBEDTLS_PLATFORM_MEMORY=1
        -DDEPLOY
        -DESP32)

target_compile_options(${COMPONENT_LIB} INTERFACE -UMBEDTLS_CONFIG_FILE)

        # Setup the static library, that needs to be fully included
add_library(toit_real STATIC ${srcs})
target_link_libraries(toit_real $<TARGET_OBJECTS:toit_image>)
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
        -Wno-error=maybe-uninitialized
#        -Wno-ignored-qualifiers
        -fno-rtti
        PUBLIC
        -UMBEDTLS_CONFIG_FILE
        )

set_source_files_properties(toit/src/interpreter.cc PROPERTIES COMPILE_FLAGS "-O3 ${TOIT_INTERPRETER_FLAGS} $ENV{LOCAL_INTERPRETER_CXXFLAGS}")
set_source_files_properties(toit/src/utils.cc PROPERTIES COMPILE_FLAGS "-DTOIT_MODEL=\"\\\"${TOIT_MODEL}\\\"\" -DVM_GIT_INFO=\"\\\"${VM_GIT_INFO}\\\"\" -DVM_GIT_VERSION=\"\\\"${TOIT_GIT_VERSION}\\\"\"")

# Some very specific code, to copy all IDF dependencies to the toit_real target
__component_get_property(reqs ${component_target} __REQUIRES)
foreach(req ${reqs})
    if(req IN_LIST build_component_targets)
        __component_get_property(req_lib ${req} COMPONENT_LIB)
        set_property(TARGET toit_real APPEND PROPERTY LINK_LIBRARIES ${req_lib})
        set_property(TARGET toit_real APPEND PROPERTY INTERFACE_LINK_LIBRARIES ${req_lib})
    endif()
endforeach()
#add_dependencies(${COMPONENT_LIB} toit_real)

# Hook up the interface component lib to the static whole archive lib
target_link_libraries(${COMPONENT_LIB} INTERFACE toit_real)
target_link_options(${COMPONENT_LIB} INTERFACE -Wl,--whole-archive ${CMAKE_CURRENT_BINARY_DIR}/libtoit_real.a -Wl,--no-whole-archive)


