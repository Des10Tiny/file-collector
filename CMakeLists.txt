cmake_minimum_required(VERSION 3.25)
project(file_collector CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(CMAKE_EXPORT_COMPILE_COMMANDS AND UNIX)
  execute_process(
        COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_SOURCE_DIR}/compile_commands.json
    )
endif()

option(USE_SANITIZERS "Enable Address and UndefinedBehavior sanitizers" OFF)
option(USE_TSAN "Enable ThreadSanitizer" OFF)
option(BUILD_TESTS "Build tests" ON)

if(USE_SANITIZERS AND USE_TSAN)
  message(FATAL_ERROR
          "Cannot enable both USE_SANITIZERS and USE_TSAN simultaneously.")
endif()

if(USE_SANITIZERS)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g
                      -O1)
  add_link_options(-fsanitize=address,undefined)
endif()

if(USE_TSAN)
  add_compile_options(-fsanitize=thread -g -O1)
  add_link_options(-fsanitize=thread)
endif()

add_subdirectory(src)

if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

find_program(CLANG_FORMAT_EXE clang-format)
if(CLANG_FORMAT_EXE)
  file(GLOB_RECURSE FORMAT_SOURCES "include/*.hpp" "src/*.cpp" "tests/*.cpp")
  add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXE} -i ${FORMAT_SOURCES}
    )
endif()

find_program(CLANG_TIDY_EXE clang-tidy)
if(CLANG_TIDY_EXE)
  file(GLOB_RECURSE TIDY_SOURCES "include/*.hpp" "src/*.cpp" "tests/*.cpp")
  add_custom_target(tidy
        COMMAND ${CLANG_TIDY_EXE} ${TIDY_SOURCES} -p ${CMAKE_BINARY_DIR}
    )
endif()
