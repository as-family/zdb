cmake_minimum_required(VERSION 3.30.0)

set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "a9e1cf81-9932-4810-974b-6eccaf14e457")

project(zdb CXX)

set(CMAKE_CXX_MODULE_STD ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(zdb)
target_sources(zdb
  PRIVATE
    src/main.cpp
  PRIVATE
  FILE_SET cxx_modules TYPE CXX_MODULES
    FILES
      src/example.cppm
)
