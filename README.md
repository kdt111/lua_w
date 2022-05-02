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
Opting out of this feature will make all pointer retrievals form `Lua` unsafe (the pointer may not point to the requested data type). When this safety is NOT disabled the every type that inherits form `lua_w::LuaBaseObject` can be safely retrieved with a guarranty that it points to the specified type (or that the data can be converted to the specified type)
- The library doesn't use any platform specific headers, however due to time constraints I was only able to test it on Windows using MinGW (GCC 11.2.0)

## Limitations
- The library doesn't support references AT ALL. This is by design since `Lua` doesn't support (or understand) them
- By default this library doesn't handle retrieval of `std::string` from `Lua`, but you can pass them to `Lua`
- Registering multiple overload of the same function is not supported. You have to register them under different names. 
- The libary by default only supports at most two constructors, if you want more you can register static methods that implement those constructors.
- Operators will only be automaticly detected when both of their arguments are of the same type as the bound class. If you want to overload operatos then you will have to implement them manually as metamethods
- If you want to use `add_parent_type<TParentClass>()` when registering a type, register `TParentClass` first (If you won't then calling methods form the base type will not work). For simplicity only one base type is allowed (so no multiple inheritance)
- Pointer safety uses RTTI (for `dynamic_cast`) this only happens when retrieving pointers form `Lua`. This check can't really be implemented in `Lua` as it has no knowlege of the `C++` type system and even if it could be done `dynamic_cast` would probably be faster

## Examples
### Globals
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

### Tables
```c++
#include <iostream>
#define LUA_W_NO_PTR_SAFETY
#define LUA_W_IMPLEMENTATION
#include "lua_w.h"

int main()
{
	lua_State* L = luaL_newstate();
	lua_w::open_libs(L, lua_w::Libs::base);

	lua_w::Table cTable(L);
	cTable.set(1, 12.7);
	cTable.set(2, "Some string");
	cTable.set("Key", "Value");

	lua_w::set_global(L, "c_tab", cTable);

	luaL_dostring(L, R"script(
		lua_tab = {12.7, "Some string", ["Key"]="Value"}
		dict = {x="x value", y="y value"}
		array = {222.97, 185.6, -987.25, 17}

		print("---PRINING IN LUA---")
		for key, value in pairs(c_tab) do
			print(key.." = "..value)
		end
	)script");

	std::cout << "---PRINTING IN C++---\n";

	auto luaTab = lua_w::get_global<lua_w::Table>(L, "lua_tab");
	std::cout << "lua_tab length = " << luaTab.length() << '\n';
	std::cout << "lua_tab[1] = " << luaTab.get<int, double>(1) << '\n';
	std::cout << "lua_tab[2] = " << luaTab.get<int, const char*>(2) << '\n';
	std::cout << "lua_tab[\"Key\"] = " << luaTab.get<const char*, const char*>("Key")<< '\n';

	auto dict = lua_w::get_global<lua_w::Table>(L, "dict");
	dict.for_each<const char*, const char*>([](const char* key, const char* value)
	{
		std::cout << "dict[\"" << key << "\"] = " << value << '\n';
	});

	auto array = lua_w::get_global<lua_w::Table>(L, "array");
	double valueToFind = 17;
	array.for_each<int, double>([&valueToFind](int key, double value)
	{
		std::cout << "array[" << key << "] = " << value;
		if (value == valueToFind)
			std::cout << " (FOUND VALUE!)\n";
		else
			std::cout << '\n';
	});

	lua_close(L);
}
```

### Binding functions
```c++
#include <iostream>
#define LUA_W_NO_PTR_SAFETY
#define LUA_W_IMPLEMENTATION
#include "lua_w.h"

void test_func(const char* name, double i, double j)
{
    std::cout << name << " = " << (i + j) << '\n';
}

int main()
{
    lua_State* L = luaL_newstate();

    lua_w::register_function(L, "test_func", &test_func);
    luaL_dostring(L, R"script(
		test_func("testVariable", 3, 7)
	)script");

    // This will error out (third argument is of a invalid type)
    if (luaL_dostring(L, R"script(
		test_func("testVariable", 3, "string")
	)script"))
        std::cout << "Error: " << lua_tostring(L, -1) << '\n';

    lua_close(L);
}
```

### Using `Lua's` functions
```c++
#include <iostream>
#define LUA_W_NO_PTR_SAFETY
#define LUA_W_IMPLEMENTATION
#include "lua_w.h"

int main()
{
	lua_State* L = luaL_newstate();
	lua_w::open_libs(L, lua_w::Libs::base);

	luaL_dostring(L, R"script(
		function lua_func(x, y, z)
			return x.." + "..y.." + "..z.." = "..(x + y + z)
		end

		function echo(text)
			print('echo! "'..text..'"')
		end

		function returns_a_function()
			local x = 7
			return (function(value) 
						x = x + value
						return x
					end)
		end
	)script");

	std::cout << lua_w::call_lua_function<const char*>(L, "lua_func", 3.0, 50.0, 22.7) << '\n';
	
	// Global functions can also be used like that
	auto function = lua_w::get_global<lua_w::Function>(L, "echo");
	function("Some text form C++");

    // Closures, and returning functions are also supported
	auto returnedFunction = lua_w::call_lua_function<lua_w::Function>(L, "returns_a_function");
	std::cout << "Should return 10 = " << returnedFunction.call<int>(3) << '\n'; // (x = 7) x + 3 = 10
	std::cout << "Should return 15 = " << returnedFunction.call<int>(5) << '\n'; // (x = 10) x + 5 = 15
	std::cout << "Should return 8 = " << returnedFunction.call<int>(-7) << '\n'; // (x = 15) x + (-7) = 8

	lua_close(L);
}
```

### Binding classes
```c++
#include <cmath>
#include <iostream>
#define LUA_W_NO_PTR_SAFETY
#define LUA_W_IMPLEMENTATION
#include "lua_w.h"

class Object
{
public:
    constexpr static const char* lua_type_name() { return "Object"; }
    unsigned long long get_address() const { return (unsigned long long)this; }
    virtual void print() const { std::cout << '[' << this << "]\n"; }
};

class Vec2 : public Object
{
public:
    double x, y;
    Vec2() : x(0), y(0) {}
    Vec2(double x, double y) : x(x), y(y) {}

    double magnitude() const { return sqrt(x * x + y * y); }

    friend Vec2 operator+(const Vec2& lhs, const Vec2& rhs) { return Vec2(lhs.x + rhs.x, lhs.y + rhs.y); }
    friend Vec2 operator-(const Vec2& lhs, const Vec2& rhs) { return Vec2(lhs.x - rhs.x, lhs.y - rhs.y); }
    friend Vec2 operator*(const Vec2& lhs, double rhs) { return Vec2(lhs.x * rhs, lhs.y * rhs); }
    friend Vec2 operator-(const Vec2& vec) { return Vec2(-vec.x, -vec.y); }
    friend bool operator==(const Vec2& lhs, const Vec2& rhs) { return lhs.x == rhs.x && lhs.y == rhs.y; }

    constexpr static const char* lua_type_name() { return "Vec2"; }

    static Vec2 one() { return Vec2(1.0, 1.0); }

    void print() const override { std::cout << '(' << x << ", " << y << ")\n"; }
};

int main()
{
    lua_State* L = luaL_newstate();
    lua_w::open_libs(L, lua_w::Libs::base);

    lua_w::register_type_function(L);

    lua_w::register_type<Object>(L)
        .add_method("get_address", &Object::get_address)
        .add_method("print", &Object::print);

    lua_w::register_type<Vec2>(L)
        .add_parent_type<Object>()
        .add_detected_operators()
        .add_member("x", &Vec2::x)
        .add_member("y", &Vec2::y)
        .add_method("magnitude", &Vec2::magnitude)
        // Custom overload of the multiplication operator (to multiply vectors by numbers)
        .add_metamethod("__mul", [](lua_State* L) -> int
        {
            if (!lua_isuserdata(L, 1) || !lua_isnumber(L, 2))
                return 0;
            Vec2* lhs = (Vec2*)lua_touserdata(L, 1);
            lua_Number rhs = lua_tonumber(L, 2);
            lua_w::internal::stack_push<Vec2>(L, *lhs * rhs);
            return 1;
        })
        // Custom overload of the __tostring method (for better printing)
        .add_metamethod("__tostring", [](lua_State* L) -> int
        {
            Vec2* self = (Vec2*)lua_touserdata(L, 1);
            lua_pushfstring(L, "(%f, %f)", self->x, self->y);
            return 1;
        })
        .add_static_method("one", &Vec2::one)
        .add_custom_and_default_constructors<double, double>();

    luaL_dostring(L, R"script(
		local v1 = Vec2(3, 4)
		local v2 = Vec2(7, 5)
		local num = 77.5
        
        print("Types:")
		print("type(num) = "..type(num))
		print("type(v1) = "..type(v1))
		print("type(nil) = "..type(nil))

        print("\nConstructors:")
		print("default vector = "..tostring(Vec2()))
		print("Vec2.one() = "..tostring(Vec2.one()))

        print("\nMember variables and methods:")
        print("v1:magnitude() = "..v1:magnitude())
        print("v1.x = "..v1:x())
        v1:x(10)
        print("v1.x = "..v1:x())

        print("\nInheritance and virtual methods:")
		print("[Inherited method] Address = "..v1:get_address())
		print("Virtual print:")
		v1:print()

        print("\nOperator overloading:")
        print("v1 = "..tostring(v1))
		print("v2 = "..tostring(v2))
		print("v1 + v2 = "..tostring(v1 + v2))
		print("v1 - v2 = "..tostring(v1 - v2))
		print("v1 * 2 = "..tostring(v1 * 2))
		print("-v1 = "..tostring(-v1))

		print("v1 == v2 is "..tostring(v1 == v2))
		print("v1 == Vec2(10, 4) is "..tostring(v1 == Vec2(10, 4)))
	)script");

    lua_close(L);
}
```

### Pointer safety
```c++
#include <iostream>
#define LUA_W_IMPLEMENTATION
#include "lua_w.h"

struct Type1 : lua_w::LuaBaseObject
{
	static constexpr const char* lua_type_name() { return "Type1"; }
};

struct Type2 : public Type1
{
	static constexpr const char* lua_type_name() { return "Type2"; }
};

struct NotRegisteredType : public lua_w::LuaBaseObject {};

int main()
{
	lua_State* L = luaL_newstate();

	lua_w::register_type<Type1>(L).add_constructor();
	lua_w::register_type<Type2>(L).add_parent_type<Type1>().add_constructor();

	NotRegisteredType ntr;

	lua_w::set_global(L, "ntr", &ntr);

	luaL_dostring(L, R"script(
		t1 = Type1()
		t2 = Type2()
	)script");

	std::cout << "ntr as 'NotRegisteredType': ";
	if (lua_w::has_global<NotRegisteredType*>(L, "ntr"))
		std::cout << "VALID\n";
	else
		std::cout << "INVALID\n";

	std::cout << "ntr as 'Type1': ";
	if (lua_w::has_global<Type1*>(L, "ntr"))
		std::cout << "VALID\n";
	else
		std::cout << "INVALID\n";

	std::cout << "t1 as 'Type1': ";
	if (lua_w::has_global<Type1*>(L, "t1"))
		std::cout << "VALID\n";
	else
		std::cout << "INVALID\n";

	std::cout << "t1 as 'Type2': ";
	if (lua_w::has_global<Type2*>(L, "t1"))
		std::cout << "VALID\n";
	else
		std::cout << "INVALID\n";

	std::cout << "t2 as 'Type2': ";
	if (lua_w::has_global<Type2*>(L, "t2"))
		std::cout << "VALID\n";
	else
		std::cout << "INVALID\n";

	std::cout << "t2 as 'Type1': ";
	if (lua_w::has_global<Type1*>(L, "t2"))
		std::cout << "VALID\n";
	else
		std::cout << "INVALID\n";

	lua_close(L);
}
```
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
