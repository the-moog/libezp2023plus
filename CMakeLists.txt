cmake_minimum_required(VERSION 3.27)
project(libezp2023plus C)

set(CMAKE_C_STANDARD 17)

include_directories(include/)
link_libraries(usb-1.0)

add_library(ezp2023plus SHARED src/ezp_prog.c src/ezp_chips_data_file.c)
