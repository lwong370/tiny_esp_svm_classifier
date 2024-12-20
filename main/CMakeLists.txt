idf_component_register(SRCS "GPIO_Example.cpp" "../liblinear/linear.cpp"
                       INCLUDE_DIRS "." "../liblinear")

set(CMAKE_CXX_STANDARD 17)
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17)
spiffs_create_partition_image(storage ../partition FLASH_IN_PROJECT)
