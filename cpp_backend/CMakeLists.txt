cmake_minimum_required(VERSION 3.16)

project(InternStartCpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)

add_executable(internstart_cpp
    src/main.cpp
    src/app.cpp
    src/controllers.cpp
    src/database.cpp
    src/domain.cpp
    src/http.cpp
    src/repositories.cpp
    src/router.cpp
    src/utils.cpp
)

target_include_directories(internstart_cpp PRIVATE include)
target_link_libraries(internstart_cpp PRIVATE SQLite::SQLite3 Threads::Threads)

target_compile_options(internstart_cpp PRIVATE
    -Wall
    -Wextra
    -pedantic
)
