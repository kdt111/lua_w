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
- Using `Lua`'s Tables as `C++` objects and that includes:
	- Retrieving Tables form `Lua`'s VM
	- Pushing tables from `C++` to `Lua`
	- Keys and values can be of any supported type (type mixing in a single table is allowed)
	- A `for_each` method that allows traversall of tables that have a constant key type and a constant value type
- Binding custom classes to `Lua` and that includes:
	- Ability to call arbitrary methods (both const and nonconst)
	- Ability to bind a constructor (one custom constructor or default constructor or both) so new instances of the class can be created in `Lua`
	- Ability to automaticly detect and bind some custom operators to `Lua` (+, -, *, /, unary -, ==, <, <=)
	- Ability to define custom behaviour for `Lua`'s metamethods (eg. define more operators than the detected ones)
	- `Lua`'s garbage collector repects calls to destructors
	- a custom `instanceof` function that allows to check in `Lua` if a varaible is some specific bound type
	- Some type safety when retrieving pointers form `Lua`

... And maybe something more in the future

## Usage
- To use the libray simply include the header `lua_w.h` to your files (aside from `Lua` the only used dependencies are the standard library) 
- A compile and a standard library that both support `C++17` are required, as some `C++17` features are used (`if constexpr`, `std::optional`, some newer stuff form `type_traits` and some others)
- The header file should be placed in a directory from which 
```c++
#include <lua.hpp>
```
is accesible

- The library doesn't use any platform specific headers, however due to time constraints I was only able to test it on Windows using MinGW (GCC 11.2.0)

## Examples
### Globals
```c++
#include <iostream>
#include "lua_w.h"

int main()
{
	lua_State* L = lua_w::new_state_with_libs(lua_w::Libs::base);

	lua_w::set_global(L, "cpp_global", "Global from C++");

	luaL_dostring(L, R"(
		lua_global = 'Global from lua'
		print(cpp_global)
	)");

	std::cout << lua_w::get_global<const char*>(L, "lua_global").value_or("NO GLOBAL FOUND") << '\n';

	lua_close(L);
}
```

### Tables
```c++
#include <iostream>
#include "lua_w.h"

int main()
{
	lua_State* L = lua_w::new_state_with_libs(lua_w::Libs::base);
	
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

	auto luaTab = lua_w::get_global<lua_w::Table>(L, "lua_tab").value();
	std::cout << "lua_tab length = " << luaTab.length() << '\n';
	std::cout << "lua_tab[1] = " << luaTab.get<int, double>(1).value() << '\n';
	std::cout << "lua_tab[2] = " << luaTab.get<int, const char*>(2).value() << '\n';
	std::cout << "lua_tab[\"Key\"] = " << luaTab.get<const char*, const char*>("Key").value() << '\n';

	auto dict = lua_w::get_global<lua_w::Table>(L, "dict").value();
	dict.for_each<const char*, const char*>([](const char* key, const char* value)
	{
		std::cout << "dict[\"" << key << "\"] = " << value << '\n';
	});

	auto array = lua_w::get_global<lua_w::Table>(L, "array").value();
	double valueToFind = 17;
	array.for_each<int, double>([&valueToFind](int key, double value)
	{
		std::cout << "array[" << key << "] = " << value;
		if(value == valueToFind)
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
#include "lua_w.h"

void test_func(const char* name, double i, double j)
{
	std::cout << name << " = " << (i + j) << '\n';
}

int main()
{
	lua_State* L = lua_w::new_state_with_libs(lua_w::Libs::base);

	lua_w::register_function(L, "test_func", &test_func);
	luaL_dostring(L, "test_func('3 + 7', 3, 7)");
	
	lua_close(L);
}
```

### Binding classes
```c++
#include "lua_w.h"

class Vec2
{
private:
	double x, y;
public:
	Vec2() : x(0), y(0) {}
	Vec2(double x, double y) : x(x), y(y) {}

	double get_x() const { return x; }
	double get_y() const { return y; }
	void set_x(double newX) { x = newX; }
	void set_y(double newY) { y = newY; }

	double magnitude() const { return sqrt(x * x + y * y); }

	friend Vec2 operator+(const Vec2& lhs, const Vec2& rhs) { return Vec2(lhs.x + rhs.x, lhs.y + rhs.y); }
	friend Vec2 operator-(const Vec2& lhs, const Vec2& rhs) { return Vec2(lhs.x - rhs.x, lhs.y - rhs.y); }
	friend Vec2 operator*(const Vec2& lhs, double rhs) { return Vec2(lhs.x * rhs, lhs.y * rhs); }
	friend Vec2 operator-(const Vec2& vec) { return Vec2(-vec.x, -vec.y); }
	friend bool operator==(const Vec2& lhs, const Vec2& rhs) { return lhs.x == rhs.x && lhs.y == rhs.y; }

	constexpr static const char* lua_type_name() { return "Vec2"; }
};

int main()
{
	lua_State* L = lua_w::new_state_with_libs(lua_w::Libs::base);

	lua_w::register_instanceof_function(L);

	lua_w::register_type<Vec2>(L)
		.add_detected_operators()
		.add_const_method("get_x", &Vec2::get_x)
		.add_const_method("get_y", &Vec2::get_y)
		.add_method("set_x", &Vec2::set_x)
		.add_method("set_y", &Vec2::set_y)
		// Custom overload of the multiplication operator (to multiply vectors by numbers)
		.add_metamethod("__mul", [](lua_State* L) -> int
		{
			if (!lua_isuserdata(L, 1) || !lua_isnumber(L, 2))
				return 0;
			Vec2* lhs = (Vec2*)lua_touserdata(L, 1);
			lua_Number rhs = lua_tonumber(L, 2);
			lua_w::stack_push<Vec2>(L, *lhs * rhs);
			return 1;
		})
		// Custom overload of the len operator (to get the magnitude of the vector)
		.add_metamethod("__len", [](lua_State* L) -> int
		{
			Vec2* vec = (Vec2*)lua_touserdata(L, 1);
			lua_pushnumber(L, vec->magnitude());
			return 1;
		})
		.add_custom_and_default_constructors<double, double>();


	luaL_dostring(L, R"script(
		function format_vec2(vec)
			if instanceof(vec, Vec2) then
				return "("..vec:get_x()..", "..vec:get_y()..")"
			else
				return "NOT A VEC2"
			end
		end

		local v1 = Vec2.new(3, 4)
		local v2 = Vec2.new(7, 5)

		local num = 77.5

		print("num = "..format_vec2(num))

		print("default vector = "..format_vec2(Vec2.new()))

		print("v1 = "..format_vec2(v1))
		print("v2 = "..format_vec2(v2))

		print("v1 + v2 = "..format_vec2(v1 + v2))

		print("v1 - v2 = "..format_vec2(v1 - v2))

		print("v1 * 2 = "..format_vec2(v1 * 2))

		print("-v1 = "..format_vec2(-v1))

		print("v1 == v2 is "..tostring(v1 == v2))

		print("v1 == Vec2.new(3, 4) is "..tostring(v1 == Vec2.new(3, 4)))

		print("v1.magnitude() = "..#v1)
	)script");

	lua_close(L);
}
```

### Pointer safety
```c++
#include <iostream>
#include "lua_w.h"

class Type1
{
public:
	static constexpr const char* lua_type_name() 
	{
		return "Type1";
	}
};

class Type2
{
public:
	static constexpr const char* lua_type_name()
	{
		return "Type2";
	}
};


int main()
{
	lua_State* L = lua_w::new_state_with_libs(lua_w::Libs::base);

	lua_w::register_type<Type1>(L).add_constructor();
	lua_w::register_type<Type2>(L).add_constructor();

	luaL_dostring(L, R"script(
		t1 = Type1.new()
		t2 = Type2.new()
	)script");

	std::cout << "t1 as 'Type1': ";
	if(lua_w::get_global<Type1*>(L, "t1"))
		std::cout << "VALID\n";
	else
		std::cout << "INVALID\n";

	std::cout << "t1 as 'Type2': ";
	if (lua_w::get_global<Type2*>(L, "t1"))
		std::cout << "VALID\n";
	else
		std::cout << "INVALID\n";
	
	std::cout << "t2 as 'Type2': ";
	if (lua_w::get_global<Type2*>(L, "t2"))
		std::cout << "VALID\n";
	else
		std::cout << "INVALID\n";

	std::cout << "t2 as 'Type1': ";
	if (lua_w::get_global<Type1*>(L, "t2"))
		std::cout << "VALID\n";
	else
		std::cout << "INVALID\n";

	lua_close(L);
}
```
