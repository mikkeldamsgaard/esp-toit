# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

#list(APPEND EXTRA_COMPONENT_DIRS)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(spottune-toit)

#target_link_options(spottune-toit.elf PRIVATE -Wl,--whole-archive /Users/mikkel/tmp/toit/build/esp32/lib/libtoit_image.a esp-idf/esp-toit-component/libesp-toit-component.a -Wl,--no-whole-archive -u toit_patchable_ubjson)
#target_link_options(spottune-toit.elf PRIVATE -Wl,--whole-archive esp-idf/esp-toit-component/libesp-toit-component.a -Wl,--no-whole-archive)