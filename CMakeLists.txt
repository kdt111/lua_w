cmake_minimum_required(VERSION 3.13)
project(lua_w LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

option(LUA_W_TESTS "Build tests" OFF)

if(${LUA_W_TESTS})
    # Link as lua_static
    set(lua_FETCH_VERSION 14f98e5fdcde3ccd7ea9188181dd7e50660a2999) # Version 5.4.4
    include(FetchContent)
    FetchContent_Declare(
        lua
        URL https://github.com/walterschell/Lua/archive/${lua_FETCH_VERSION}.tar.gz
    )
    FetchContent_GetProperties(lua)

    if(NOT lua_POPULATED)
        set(FETCHCONTENT_QUIET NO)
        FetchContent_Populate(lua)

        set(LUA_BUILD_COMPILER OFF CACHE BOOL "" FORCE)
        set(LUA_ENABLE_SHARED OFF CACHE BOOL "" FORCE)

        add_subdirectory(${lua_SOURCE_DIR} ${lua_BINARY_DIR})
    endif()

    add_executable(lua_w_tests tests.cpp)
    target_link_libraries(lua_w_tests lua_static)
endif()
