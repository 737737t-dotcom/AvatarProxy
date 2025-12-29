cmake_minimum_required(VERSION 3.16)
project(AvatarProxy CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED COMPONENTS system)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LZ4 REQUIRED liblz4)

file(GLOB_RECURSE SOURCES 
    "src/*.cpp"
    "main.cpp"
)

add_executable(avatar_proxy ${SOURCES})

target_include_directories(avatar_proxy PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${Boost_INCLUDE_DIRS}
    ${LZ4_INCLUDE_DIRS}
)

target_link_libraries(avatar_proxy PRIVATE
    ${Boost_LIBRARIES}
    ${LZ4_LIBRARIES}
    pthread
)
