# lua_w
A `C++17`, header-only library that aims to make binding `C++` to `Lua` as simple as possible

## Features
### General
- **Header-only** - no complicated build systems required
- **Stateless** - no objects (aside from the `Lua's` state) hold any information required for the bindings to function
- **As simple as possible** - I tried to not reinvent the wheel, so everything that can be done simply with the default API should be done using it
- **Modular** - aside from basic stack manipulation every feature of the library can function on it's own
- **Commented code** - so you don't have to wonder how everything works
- Written in `C++17` without any macros

### Library
- Simple opening of specified `Lua` libraries
- Stack operation made as type safe as possible
- Registering `C++` functions of an arbitrary signature to be used in `Lua` (with some limitations)
- Calling `Lua` functions from `C++`
- Storing `Lua` functions as `C++` objects and calling them form this object
- Setting and getting global values from `Lua`
- Using `Lua's` Tables as `C++` objects and that includes:
	- Retrieving Tables form `Lua`
	- Pushing tables from `C++` to `Lua`
	- Keys and values can be of any supported type (type mixing in a single table is allowed)
	- A `for_each` method that allows traversall of tables that have a constant key type and a constant value type
- Binding custom classes to `Lua` and that includes:
	- Ability to call arbitrary methods (both const and non-const)
    - Ability to both get and set bound member variables
	- Ability to bind a constructor (one custom constructor or default constructor or both) so new instances of the class can be created in `Lua`
	- Ability to automaticly detect and bind some custom operators to `Lua` (+, -, *, /, unary -, ==, <, <=)
	- Ability to define custom behaviour for `Lua`'s metamethods (eg. define more operators than the detected ones)
	- `Lua`'s garbage collector repects calls to destructors
	- a custom `type` function that can also check for the registered types (for everything else will work the same as the regular type function)
	- Inheritance support (limited to one parent type) with full support of virtual methods
- Type safe retrieval of pointers from `Lua` using RTTI (you can opt-out of this feature)

... And all of this (and maybe something more in the future) in just about 1000 lines of code

## Usage
- To use the libray simply include the header `lua_w.h` to your files (aside from `Lua` the only used dependencies are the standard library)
- In a SINGLE .cpp file use the following construction, to generate definitions of non-templated structures
```c++
#define LUA_W_IMPLEMENTATION
#include "lua_w.h"
```
- A compiler and a standard library that both support `C++17` are required, as some `C++17` features are used (`if constexpr`, some newer stuff form `type_traits` and some others)
- The library was written with `Lua 5.4` in mind so different versions may not work
- The header file should be placed in a directory from which 
`#include <lua.hpp>` is accesible
- If you don't want to use safe pointer retrieval then define `LUA_W_NO_PTR_SAFETY` before including `lua_w.h` like so:
``` c++
#define LUA_W_NO_PTR_SAFETY
#include "lua_w.h"
```
Opting out of this feature will make all pointer retrievals form `Lua` unsafe (the pointer may not point to the requested data type). When this safety is NOT disabled every type that inherits form `lua_w::LuaBaseObject` can be safely retrieved with a guarranty that it points to the specified type (or that the data can be converted to the specified type)
- The library doesn't use any platform specific headers. I've developed and testes it on both Windows and Linux compiling with GCC and clang, so it should be platform independent (I haven't tested anything using MSVC as I don't use this compiler)

## Limitations
- The library doesn't support references AT ALL. This is by design since `Lua` doesn't support (or understand) them
- Registering multiple overload of the same function is not supported. You have to register them under different names. 
- The libary by default only supports at most two constructors, if you want more you can register static methods that implement those constructors.
- Operators will only be automaticly detected when both of their arguments are of the same type as the bound class. If you want to overload operatos then you will have to implement them manually as metamethods
- If you want to use `add_parent_type<TParentClass>()` when registering a type, register `TParentClass` first (If you won't then calling methods form the base type will not work). For simplicity only one base type is allowed (so no multiple inheritance)
- Pointer safety uses RTTI (for `dynamic_cast`) this only happens when retrieving pointers form `Lua`. This check can't really be implemented in `Lua` as it has no knowlege of the `C++` type system and even if it could be done `dynamic_cast` would probably be faster

## Example
```c++
#include <iostream>
#define LUA_W_NO_PTR_SAFETY
#define LUA_W_IMPLEMENTATION
#include "lua_w.h"

int main()
{
    lua_State* L = luaL_newstate();
    lua_w::open_libs(L, lua_w::Libs::base | lua_w::Libs::math);

    lua_w::set_global(L, "cpp_global", "Global from C++");

    luaL_dostring(L, R"(
        lua_global = 'Global from lua'
        lua_numeric_global = math.sin(math.pi * 2) -- Should be about 0
        print(cpp_global)
        if math.random(10) > 5 then
            x = 22
            print("'x' registered")
        end
    )");

    std::cout << lua_w::get_global<const char*>(L, "lua_global") << '\n';
    std::cout << lua_w::get_global<float>(L, "lua_numeric_global") << '\n';
    if (lua_w::has_global<double>(L, "x"))
        std::cout << "x = " << lua_w::get_global<double>(L, "x") << '\n';
    else
        std::cout << "No global 'x'\n";

    lua_close(L);
}
```
More examples and general library usage can be found in the `tests.cpp` file

## Licence
MIT License

Copyright (c) 2022 Jan Malek (https://github.com/kdt111)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
