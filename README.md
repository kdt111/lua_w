# lua_w
A `C++17`, header-only library implemented mostly as different templates that aims to make integrating native `C++` objects to `Lua` as simple as possible

## Features
### General
- **Header-only** - no complicated build systems required
- **Stateless** - no objects (aside from the lua' state) hold any information required for the bindings to function
- **Modular** - aside from basic stack manipulation every feature of the library can function on it's own
- **Commented code** - so you don't have to wonder how everything works
- Written in `C++17` without any macros

### Library
- Helper class for handeling the `lua_State` pointers in a more modern fashion (eg. opening the state in constructor and closing it in a destructor)
- Opening a new `lua_State` with specified libraries
- Stack operation made as type safe as possible (using `std::optional`)
- Registering `C++` functions of an arbitrary signature to be used in `Lua`
- Calling `Lua` functions from `C++`
- Setting and getting global values from `Lua`
- Using `Lua` Tables as `C++` objects
- Binding custom classes to `Lua` and that includes:
	- Ability to call arbitrary methods (both const and nonconst)
	- Ability to bind a constructor so new instances of the class can be created in `Lua`
	- Ability to automaticly detect and bind some custom operators to `Lua`
	- `Lua`'s garbage collector repects calls to destructors
	- a custom `instanceof` function that allows to check in `Lua` if a varaible is some specific bound type

... And maybe something more in the future

## Simple example

## Usage
- A compile and a standard library that both support `C++17` are required, as some `C++17` features are used (`if constexpr`, `std::optional`, some newer stuff form `type_traits` and some others)
- The header file should be placed in a directory from which 
```c++
#include <lua.hpp>
```
is accesible

- The library doesn't use any platform specific headers, however due to time constraints I was only able to test it on Windows using MinGW (GCC 11.2.0)

