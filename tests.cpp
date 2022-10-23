#include <assert.h>
#include <stdlib.h>
#include <iostream>
#include <cstring>
#include <string>

#define LUA_W_IMPLEMENTATION
#include "lua_w.h"

#define SETUP lua_State* L = luaL_newstate();         \
              lua_w::init(L);                         \
              lua_w::open_libs(L, lua_w::Libs::all);  \
              lua_w::register_type_function(L);       \
              {                                       \

#define TEARDOWN }              \
                 lua_close(L);  \

void should_handle_globals() {
    SETUP

    lua_w::set_global(L, "num", 22);
    lua_w::set_global(L, "str", "C++ string");

    assert(luaL_dostring(L, R"(
        assert(num == 22)
        assert(str == "C++ string")

        lua_num = 17
        lua_str = "Lua string"
    )") == LUA_OK);

    assert(lua_w::get_global<double>(L, "lua_num") == 17);
    assert(std::strcmp(lua_w::get_global<const char*>(L, "lua_str"), "Lua string") == 0);

    TEARDOWN
}

void should_handle_functions() {
    SETUP

    lua_w::register_function(L, "c_func", +[](double a, double b) -> double {
       return (a + b) * 2;  
    });

    assert(luaL_dostring(L, R"(
        assert(c_func(3, 4) == (3 + 4) * 2)

        function lua_func(a)
            return 512 + a;
        end
    )") == LUA_OK);

    assert(lua_w::call_lua_function<double>(L, "lua_func", 10.0) == 522);

    TEARDOWN
}

void should_handle_function_objects() {
    SETUP

    assert(luaL_dostring(L, R"(
    function func(a, b, c)
        return "Res = "..(a + b + c)
    end

    function closure(num)
        local num = 7
        return (function()
            num = num + 1
            return num
        end) 
    end
    )") == LUA_OK);

    auto func = lua_w::get_global<lua_w::Function>(L, "func");
    assert(strcmp(func.call<const char*>(1, 2, 3), "Res = 6.0") == 0);

    auto closure = lua_w::get_global<lua_w::Function>(L, "closure");
    auto inner = closure.call<lua_w::Function>();
    assert(inner.call<int>() == 8);
    assert(inner.call<int>() == 9);
    assert(inner.call<int>() == 10);

    TEARDOWN
}

int main() {
    should_handle_globals();
    should_handle_functions();
    should_handle_function_objects();
    std::cout << "Tests passed!\n";
}
