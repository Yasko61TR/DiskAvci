cmake_minimum_required(VERSION 3.20)
project(DiskAvci VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4 /utf-8)
else()
  add_compile_options(-Wall -Wextra)
endif()

# Tarama mantığı kütüphanede, komut satırı ve testler bunu kullanır.
add_library(diskavci_core src/scanner.cpp)
target_include_directories(diskavci_core PUBLIC src)

add_executable(diskavci src/main.cpp)
target_link_libraries(diskavci PRIVATE diskavci_core)

option(DISKAVCI_BUILD_TESTS "Testleri derle" ON)
if(DISKAVCI_BUILD_TESTS)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  enable_testing()
  add_executable(diskavci_tests tests/scanner_test.cpp)
  target_link_libraries(diskavci_tests PRIVATE diskavci_core GTest::gtest_main)

  include(GoogleTest)
  gtest_discover_tests(diskavci_tests)
endif()
