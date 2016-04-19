cmake_minimum_required(VERSION 3.1)
project(SHT)

set(CMAKE_C_FLAGS "-std=c99 -Wall -Werror -pedantic -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls")


set(SOURCE_FILES main.c)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
add_executable(SHT ${SOURCE_FILES} shifting_hash_table.c shifting_hash_table.h lib/hash_function.c lib/md5.c lib/sha1.c)
