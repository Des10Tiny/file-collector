add_library(file_collector
    file_collector.cpp
)

target_include_directories(file_collector
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_compile_options(file_collector PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)
