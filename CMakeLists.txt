cmake_minimum_required(VERSION 3.5)

# Register without sources, this will create an INTERFACE lib and we can then specify link options later
idf_component_register(INCLUDE_DIRS "include"
        SRCS "src/toit_api.cc"
        REQUIRES "toit")
