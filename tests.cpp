#define LUA_W_IMPLEMENTATION
#include "lua_w.h"

int main() {
    lua_State* L = luaL_newstate();
    lua_w::open_libs(L, lua_w::Libs::base);

    luaL_dostring(L, "print('Hello, wordl! form Lua')");
}
