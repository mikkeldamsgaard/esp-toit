idf_component_register(SRCS "main.cc"
                    INCLUDE_DIRS "")


#add_prebuilt_library(toit_image "/Users/mikkel/tmp/toit/build/esp32/lib/libtoit_image.a")
#target_link_libraries(${COMPONENT_LIB} PRIVATE toit_image)
#
#
#target_compile_options(${COMPONENT_LIB} PRIVATE -DRAW=1 -DESP32 -DDEPLOY=1 -D__FREERTOS__=1 -Wno-sign-compare -fno-rtti -mlongcalls)