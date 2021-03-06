cmake_minimum_required(VERSION 3.5)
project(pony_gp C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(DIR ${pony_gp_SOURCE_DIR})

if (${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
	set(CMAKE_C_COMPILER "emcc")
endif()

add_executable(pony_gp main.c util/memmngr.c include/memmngr.h util/binary_tree.c include/binary_tree.h util/queue.c include/queue.h util/rand_util.c include/rand_util.h include/main.h include/misc_util.h util/hashmap.c include/hashmap.h include/params.h util/misc_util.c util/config_parser.c include/config_parser.h util/file_util.c include/file_util.h util/csv_parser.c include/csv_parser.h include/csv_data.h util/tests.c include/tests.h)

if (CMAKE_COMPILER_IS_GNUCC)
	target_link_libraries(pony_gp m)
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
	set_target_properties(pony_gp PROPERTIES LINK_FLAGS "--embed-file ../data -s NO_EXIT_RUNTIME=0 -s BINARYEN_TRAP_MODE=clamp")
endif()
