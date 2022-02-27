#pragma once

// Classic include guard if the compiler doesn't support #pragma once
#ifndef LUA_W_INCLUDE_H
#define LUA_W_INCLUDE_H

#include <lua.hpp> // Lua header

#include <optional> // Used in: stack_get, call_lua_function, get_global (and anything that calls them)
#include <tuple> // Used in: call_c_func_impl (and anything that calls them)
#include <type_traits> // Used in: stack_push, stack_get, call_c_func_impl, call_lua_function (and anything that calls them)
#include <string> // Used in: stack_push, stack_get (and anything that calls them)
#include <memory> // Used in: Table class as tableKey
#include <new> // Used in TypeWrapper (for inplace new)
#include <utility> // Used in TypeWrapper (for checking if operator overloads exist)

#include <fstream> // Used only in: load_script_helper
#include <sstream> // Used only in: load_script_helper

// Lua helper functions
namespace lua_w
{
	// Some internal data
	namespace internal
	{
		// Helper for failing type matching in push and get templates
		template<bool flag = false>
		void no_match() { static_assert(flag, "No matching type found"); }

		// Helper for checking if the type has a lua_type_name method
		template<class, class = void>
		constexpr bool has_lua_type_name_v = false;
		
		template<class T>
		constexpr bool has_lua_type_name_v<T, std::void_t<decltype(T::lua_type_name())>> = std::is_same_v<decltype(T::lua_type_name()), const char*>;

		// Helper for failing when no lua_type_name method is found
		template<bool flage = false>
		void no_lua_type() { static_assert(flage, "Class has to have a static 'const char* lua_type_name()' method"); }

		// Helper for failing when the object is not copy constructible
		template<bool flage = false>
		void not_copy_constructible() { static_assert(flage, "To push the type to the stack it has to be copy constructible"); }
	}

	//----------------------------
	// HELPER FUNCTIONS AND OBJECTS
	//----------------------------

	namespace Libs
	{
		enum Libs : uint16_t
		{
			none = 0,
			base = 1 << 1,
			coroutine = 1 << 2,
			debug = 1 << 3,
			io = 1 << 4,
			math = 1 << 5,
			os = 1 << 6,
			package = 1 << 7,
			string = 1 << 8,
			table = 1 << 9,
			utf8 = 1 << 10,
			all = ((1 << 16) - 1)
		};
	}
	
	// A class that will open and close the state when the lifetime of the enclosing scope will be finished
	// eg. This State will be closed when the object that holds this State will be deleted
	class ScopedState
	{
	private:
		lua_State* L;
	public:
		// Returns the internal lua_State object
		inline lua_State* get_state() { return L; }
		
		ScopedState() { L = luaL_newstate(); }
		ScopedState(lua_State*&& state) { L = state; }
		ScopedState(const ScopedState&) = delete;
		~ScopedState() { lua_close(L); }

		// Returns the internal lua_State object
		inline lua_State* operator*() { return L; }
		ScopedState& operator=(const ScopedState& other) = delete;
	};

	// Creates a new lua state with libs provided as a bit mask
	// If you want to include everything pass Libs::all
	// If you want nothing pass Libs::none
	// If you want for example: base and math pass (Libs::base | Libs::math)
	lua_State* new_state_with_libs(uint16_t libs)
	{		
		lua_State* L = luaL_newstate();
		int popCount = 0;
		if (libs == Libs::all)
		{
			luaL_openlibs(L);
			return L;
		}
		else if (libs == Libs::none)
			return L;

		if (libs & Libs::base)
		{
			luaL_requiref(L, LUA_GNAME, luaopen_base, 1);
			++popCount;
		}
		if (libs & Libs::coroutine)
		{
			luaL_requiref(L, LUA_COLIBNAME, luaopen_coroutine, 1);
			++popCount;
		}
		if (libs & Libs::debug)
		{
			luaL_requiref(L, LUA_DBLIBNAME, luaopen_debug, 1);
			++popCount;
		}
		if (libs & Libs::io)
		{
			luaL_requiref(L, LUA_IOLIBNAME, luaopen_io, 1);
			++popCount;
		}
		if (libs & Libs::math)
		{
			luaL_requiref(L, LUA_MATHLIBNAME, luaopen_math, 1);
			++popCount;
		}	
		if (libs & Libs::os)
		{
			luaL_requiref(L, LUA_OSLIBNAME, luaopen_os, 1);
			++popCount;
		}
		if (libs & Libs::package)
		{
			luaL_requiref(L, LUA_LOADLIBNAME, luaopen_package, 1);
			++popCount;
		}
		if (libs & Libs::string)
		{
			luaL_requiref(L, LUA_STRLIBNAME, luaopen_string, 1);
			++popCount;
		}
		if (libs & Libs::table)
		{
			luaL_requiref(L, LUA_TABLIBNAME, luaopen_table, 1);
			++popCount;
		}
		if (libs & Libs::utf8)
		{
			luaL_requiref(L, LUA_UTF8LIBNAME, luaopen_utf8, 1);
			++popCount;
		}
		lua_pop(L, popCount);
		return L;
	}

	// Helper function that reads the file from the path and returns it's content as a string
	// uses std::ifstream and std::stringstream
	std::string load_script_helper(const char* scriptPath)
	{
		auto file = std::ifstream(scriptPath);
		if (!file.is_open())
			return std::move(std::string());

		std::stringstream ssbuffer;
		ssbuffer << file.rdbuf();

		// Move out the value to prevent a copy
		return std::move(ssbuffer.str());
	}

	//----------------------------
	// SCRIPT EXECUTION
	//----------------------------
	
	// Executes the script with default error handeling
	// The returned value is the error code. If there were no errors it will returrn LUA_OK
	inline int execute_string(lua_State* L, const char* string)
	{
		luaL_loadstring(L, string);
		return lua_pcall(L, 0, LUA_MULTRET, 0);
	}

	// Loads and executes the script from the provided file path with default error handeling
	// Uses load_script_helper, to load the file
	inline int execute_file(lua_State* L, const char* filePath)
	{
		return execute_string(L, load_script_helper(filePath).c_str());
	}

	// Pops the error message form the stack.
	// Should only be used after calling 'execute_string' when its return value is not equal to LUA_OK
	const char* pop_error_message(lua_State* L)
	{
		if (lua_isstring(L, -1) && !lua_isnumber(L, -1))
		{
			const char* str = lua_tostring(L, -1);
			lua_pop(L, 1);
			return str;
		}
		else
			return "";
	}

	//----------------------------
	// TABLES
	//----------------------------

	// Forward declaration of the stack manipulation functions (tables nead this, and stack functions also have to know how to handle tables)
	
	template<typename TValue>
	void stack_push(lua_State* L, const TValue& value);

	template<typename TValue>
	std::optional<TValue> stack_get(lua_State* L, int idx);

	// Class that represents a lua table
	// Doesn't store any data, only the required pointers to access the bound table in the lua VM
	class Table
	{
	private:
		// Internal struct to hold data, it allows the table to use RAII and be shared by multiple objects
		struct TableData
		{
			lua_State* L;

			TableData(lua_State* L) : L(L) {}
			
			~TableData()
			{
				lua_pushnil(L);
				lua_rawsetp(L, LUA_REGISTRYINDEX, &L);
			}
		};		
		// The shared_ptr stores data neaded to access the table
		// This way the object can be coppied and there is no way that something will clear the table when
		// Some Table has this shared_ptr
		std::shared_ptr<TableData> tableData;
		Table() {}
		
	public:
		// Constructs a new table in the provided lua state
		Table(lua_State* L) : tableData(std::make_shared<TableData>(L))
		{
			// Push a new table
			lua_newtable(L);
			// Assign this table to the registry, so we can use it later
			lua_rawsetp(L, LUA_REGISTRYINDEX, &tableData->L);
		}

		// Pushes the table to the lua stack
		// SHOULDN'T be used on it's own
		// If the stateToPush is different than the state that this table is bound too the function pushes nil
		void push_to_stack(lua_State* stateToPush) const
		{
			auto& L = tableData->L;
			
			if (L != stateToPush)
			{
				lua_pushnil(stateToPush);
				return;
			}

			lua_rawgetp(L, LUA_REGISTRYINDEX, &L);
		}

		// Creates the table from the data on the stack
		// SHOULDN'T be used on it's own
		static Table create_from_stack(lua_State* L, int idx)
		{
			Table tab;
			tab.tableData = std::make_shared<TableData>(L);

			auto ptr = &tab.tableData->L;
			
			// Adjust the index if it was a pseudo index
			// We just pushed the pointer so it is now one lower from the top
			if (idx < 0)
				idx--;
			lua_pushvalue(L, idx);
			lua_rawsetp(L, LUA_REGISTRYINDEX, ptr);
			
			return tab;
		}

		// Returns the amoun of integer indexed elements in the table
		// This method doesn't account for anything in the table that has a different key
		// If raw is true then the calculation skips all metamethods
		lua_Integer length(bool raw = false) const
		{
			auto& L = tableData->L;
			
			lua_rawgetp(L, LUA_REGISTRYINDEX, &L);
			if (raw)
				lua_rawlen(L, -1);
			else
				lua_len(L, -1);
			lua_Integer retVal = lua_tointeger(L, -1);
			lua_pop(L, 2);
			return retVal;
		}

		// Returns a value value form the table by the value index
		// NOTE: lua tables are indexed form 1, and this function follows this convention
		// If raw is true then the calculation skips all metamethods
		template<typename TValue>
		std::optional<TValue> get(lua_Number idx, bool raw = false) const
		{
			auto& L = tableData->L;
			
			lua_rawgetp(L, LUA_REGISTRYINDEX, &L);
			if (raw)
				lua_rawgeti(L, -1, idx);
			else
				lua_geti(L, -1, idx);
			auto retVal = stack_get<TValue>(L, -1);
			lua_pop(L, 1);
			return retVal;
		}

		// Pushes this value to the table
		// NOTE: lua tables are indexed form 1, and this function follows this convention
		// If raw is true then the calculation skips all metamethods
		template<typename TValue>
		void set(lua_Number idx, const TValue& value, bool raw = false) const
		{
			auto& L = tableData->L;

			lua_rawgetp(L, LUA_REGISTRYINDEX, &L);
			stack_push(L, value);
			if (raw)
				lua_rawseti(L, -2, idx);
			else
				lua_seti(L, -2, idx);
		}

		// Returns a value form the table by it's string key
		// If raw is true then the calculation skips all metamethods
		template<typename TValue>
		std::optional<TValue> get(const char* key, bool raw = false) const
		{
			auto& L = tableData->L;
			
			lua_rawgetp(L, LUA_REGISTRYINDEX, &L);
			if (raw)
			{
				// Push as a literal (to save lua form copping something that it will pop in the next instruction)
				lua_pushstring(L, key);
				lua_rawget(L, -2);
			}
			else
				lua_getfield(L, -1, key);
			auto retVal = stack_get<TValue>(L, -1);
			lua_pop(L, 1);
			return retVal;
		}

		// Pushes the value to the table with to the provided string key
		// If raw is true then the calculation skips all metamethods
		template<typename TValue>
		void set(const char* key, const TValue& value, bool raw = false) const
		{
			auto& L = tableData->L;
			
			lua_rawgetp(L, LUA_REGISTRYINDEX, &L);
			stack_push(L, value);
			if (raw)
			{
				lua_pushstring(L, key);
				lua_rawset(L, -3);
			}
			else
				lua_setfield(L, -2, key);
		}
	};

	//----------------------------
	// STACK MANIPULATIONS
	//----------------------------

	// Pushes the TValue on to the stack (can push numbers, bools, strings, cstrings, lua_w::Tables, all pointers and copies of objects registerd in the lua VM)
	template<typename TValue>
	void stack_push(lua_State* L, const TValue& value)
	{
		// Remove references, const and volatile kewyords to better match the types
		using value_t = std::decay_t<TValue>;

		if constexpr (std::is_same_v<value_t, Table>)
			value.push_to_stack(L);
		else if constexpr (std::is_same_v<value_t, bool>)
			lua_pushboolean(L, value);
		else if constexpr (std::is_convertible_v<value_t, lua_Number>)
			lua_pushnumber(L, static_cast<lua_Number>(value)); // Can push anything convertible to a lua_Number (double by default)
		else if constexpr (std::is_same_v<value_t, const char*> || std::is_same_v <value_t, char*>) // can push both const char* and char* (lua makes a copy of the sting)
			lua_pushstring(L, value);
		else if constexpr (std::is_same_v<value_t, std::string>) // Can also push strings as char* (lua will make a copy anyway)
			lua_pushstring(L, value.c_str());
		else if constexpr (std::is_pointer_v<value_t>)
		{
			using value_t_no_ptr = std::remove_pointer_t<value_t>;
			if constexpr (internal::has_lua_type_name_v<value_t_no_ptr>)
			{
				lua_pushlightuserdata(L, value);
				// If this pointer points to a type that can be registerd, we have to check if the type was registerd
				luaL_getmetatable(L, value_t_no_ptr::lua_type_name());
				if (lua_istable(L, -1))
					lua_setmetatable(L, -2);
				else
					lua_pop(L, 1); // if no type registration was done then pop the the one element and leave just the pointer
			}
			else // Just push a pointer if this type can't be registered
				lua_pushlightuserdata(L, value);
		}
		else if constexpr (internal::has_lua_type_name_v<value_t>)
		{
			if constexpr (std::is_copy_constructible_v<value_t>)
			{
				luaL_getmetatable(L, TValue::lua_type_name());
				// If the type is registerd we want to create an object, otherwise we leave the nil returned by the luaL_getmetatable on the stack
				if (lua_istable(L, -1))
				{
					TValue* ptr = (TValue*)lua_newuserdata(L, sizeof(TValue));
					new(ptr) TValue(value);
					lua_pushvalue(L, -2);
					lua_setmetatable(L, -2);
					lua_remove(L, lua_gettop(L) - 1); // Remove the test metatable value (it's under the userdata)
				}
			}
			else
				internal::not_copy_constructible(); // Only objects that can be copy constructible can be passed as copies to the lua VM
		}
		else
			internal::no_match(); // No matching type was found
	}

	// Returns a value form the lua stack on the position idx if it exists or can be converted to the TValue type, otherwise returns an empty optional
	// idx = 1 is the first element from the BOTTOM of the stack
	// idx = -1 is the first element from the TOP of the stack
	// WARNING!!! Be carefull when you are requesting a char* or const char*. This memory is not mamaged by C++, so they may be deleted unexpecteadly
	// DEFINITLY DON'T MODIFY THEM
	// The safest bet is to request a std::string, or make a copy straight away
	// Also be carefull when requesting any other pointers, since the memory may or may not be managed by lua
	template<typename TValue>
	std::optional<TValue> stack_get(lua_State* L, int idx)
	{
		// Remove references, const and volatile kewyords to better match the types
		using value_t = std::decay_t<TValue>;

		if constexpr (std::is_same_v<value_t, Table>)
		{
			if (lua_istable(L, idx))
			{
				std::cout << "Table found!\n";
				return Table::create_from_stack(L, idx);
			}
			else
				return {};
		}
		else if constexpr (std::is_same_v <value_t, bool>)
		{
			if (lua_isboolean(L, idx))
				return lua_toboolean(L, idx);
			else
				return {};
		}
		else if constexpr (std::is_convertible_v<value_t, lua_Number>)
		{
			if (lua_isnumber(L, idx))
				return static_cast<value_t>(lua_tonumber(L, idx));
			else
				return {};
		}
		else if constexpr (std::is_same_v<value_t, const char*>) // NOTE: We are not matching to char*, it can be modified
		{
			if (lua_isstring(L, idx))
				return lua_tostring(L, idx);
			else
				return {};
		}
		else if constexpr (std::is_same_v<value_t, std::string>)
		{
			if (lua_isstring(L, idx))
				return std::string(lua_tostring(L, idx));
			else
				return {};
		}
		else if constexpr (std::is_pointer_v<value_t>) // WARNING!: There is no way to check the type of the object. So be shure that you are getting the pointer that you are requesting
		{
			if (lua_isuserdata(L, idx))
				return (TValue)lua_touserdata(L, idx);
			else
				return {};
		}
		else
			internal::no_match();
	}

	//----------------------------
	// FUNCTION CALLING
	//----------------------------
	
	// Some more internal data
	namespace internal
	{
		// Type alias for transforming two template arguments to a function pointer
		template<typename TRet, typename... TArgs>
		using FuncPtr_t = TRet(*)(TArgs...);

		// Pushes multiple values on to the stack (can push anything that stack_push can)
		template<typename... TValue>
		void stack_push_multiple(lua_State* L, TValue... values)
		{
			// Using a C++17 fold expression to push every type and value
			(stack_push(L, values), ...);
		}
		
		// Pops parameters form the stack to call a C function with them
		template<typename TValue>
		TValue pop_param_form_stack(lua_State* L)
		{
			// Pop values form the stack
			// From the bottom idx 1 is the first argument, 2 is the second...
			std::optional<TValue> value = stack_get<TValue>(L, 1);
			// Remove this element, it shifts everything down, so now the second argument is on the idx 1, third on idx 2 ...
			lua_remove(L, 1);
			// We don't check if the optional holds a value. If it doesn't then the C function can't be called anyway
			// So there will be some kind of error
			return value.value();
		}
	}

	// Calls a lua function with the arguments and an expected return type (either something or void)
	// TODO: Handle multiple return values
	template<typename TRet, typename... TArgs>
	std::optional<TRet> call_lua_function(lua_State* L, const char* funcName, TArgs... funcArgs)
	{
		// Get function's name
		lua_getglobal(L, funcName);
		// Push all of the arguments
		internal::stack_push_multiple(L, funcArgs...);
		// handle return value
		if constexpr (std::is_void_v<TRet>)
			lua_call(L, sizeof...(funcArgs), 0); // Take nothing if void is expected
		else
			lua_call(L, sizeof...(funcArgs), 1); // Take one argument if something is expected

		return stack_get<TRet>(L, -1); // get the value form the stack and return it
	}
	
	// Registers a C function of arbitrary signature into the lua VM.
	// The function will be called as normal if argument types and amounts match passed by lua match
	template<typename TRet, typename... TArgs>
	void register_function(lua_State* L, const char* funcName, internal::FuncPtr_t<TRet, TArgs...> funcPtr)
	{
		// Push the pointer to the function as light use data (so a pointer to anything) 
		lua_pushlightuserdata(L, (void*)funcPtr); // C style cast has to be made to avoid compilation errors
		// Register the function as a C closure (explanation - https://www.lua.org/pil/27.3.3.html)
		// The gist of it is that the call_c_func_impl will be able to retrive the pushed function pointer using upvalues
		// And will know what C function to call
		lua_pushcclosure(L, [](lua_State* L) -> int
			{
				// Retrieve the pointer to the C function form the upvalues that were passed to lua when this closure was created
				// You can think of upvalues as C++ lambda captures
				// Explanation - https://www.lua.org/pil/27.3.3.html
				// The pointer was passed as light user data so we retrieve it and cast to the required type
				internal::FuncPtr_t<TRet, TArgs...> ptr = (internal::FuncPtr_t<TRet, TArgs...>)lua_touserdata(L, lua_upvalueindex(1)); // C style cast cause of the void* type
				// Make a tuple of the required arguments by expanding the pack to pop the values form the stack
				std::tuple<TArgs...> args = { internal::pop_param_form_stack<TArgs>(L) ... };
				// C functions can return void or one value, so we only need to take care of two things
				if constexpr (std::is_void_v<TRet>)
				{
					// If return type is void just call the function using apply
					std::apply(ptr, args);
					return 0; // Returning 0 means not leaving anything on the stack
				}
				else
				{
					// TODO: handle functions returning pointers to common types
					TRet retVal = std::apply(ptr, args);
					stack_push<TRet>(L, retVal);
					return 1; // We leave one value on the stack
				}
			}, 1);
		// Assign the pushed closure a name to make it a global function
		lua_setglobal(L, funcName);
	}

	// Registers a function to lua with the classic signature (returning an int and taking a lua_State pointer as the only argument)
	// You will have to handle taking the arguments, and pushing return values manually, but it gives you more controll over how everything is handled
	void register_c_function(lua_State* L, const char* funcName, internal::FuncPtr_t<int, lua_State*> funcPtr)
	{
		lua_register(L, funcName, funcPtr);
	}

	//----------------------------
	// GLOBAL VALUES
	//----------------------------

	// Attempts to get a global value form the lua VM. If the value is not found or the type doesn't match then returns a empty optional
	template<typename TValue>
	inline std::optional<TValue> get_global(lua_State* L, const char* globalName)
	{
		// Attempt to get a global by name, the value will be pushed to the lua stack
		// If the global doesn't exist the function will push nil
		lua_getglobal(L, globalName);
		// Get the value form the stack
		// No need to check for nil since it is handles in the stack_get function (by returning an empty optional)
		auto value = stack_get<TValue>(L, -1);
		// Pop the value of the stack, so it doesn't stay there
		lua_pop(L, 1);
		return value;
	}

	// Creates or sets a global value in the lua VM
	template<typename TValue>
	inline void set_global(lua_State* L, const char* globalName, const TValue& value)
	{
		// Push the value to the stack
		stack_push(L, value);
		// Bind a global name to this value
		lua_setglobal(L, globalName);
	}

	//----------------------------
	// LUA FUNCTIONS OBJECTS
	//----------------------------

	// TODO: Implement

	//----------------------------
	// CLASS BINDING
	//----------------------------
	// Materials: http://lua-users.org/wiki/CppObjectBinding
	// https://www.youtube.com/playlist?list=PLLwK93hM93Z3nhfJyRRWGRXHaXgNX0Itk
	// TODO: Add operators

	// Internal stuff for class binding
	namespace internal
	{
		// A pointer to a member function type (every class function that is not static is a member)
		// Static functions should use FuncPtr_t
		template<class TClass, typename TRet, typename... TArgs>
		using MemberFuncPtr_t = TRet(TClass::*)(TArgs...);

		// A pointer to a member variable type
		template<class TClass, typename TValue>
		using MemberDataPtr_t = TValue(TClass::*);

		// A pointer to a const member function
		template<class TClass, typename TRet, typename... TArgs>
		using MemberConstFuncPtr_t = TRet(TClass::*)(TArgs...) const;

		// A storage struct for member function pointers (they are bigger than regular pointers)
		template<class TClass, typename TRet, typename... TArgs>
		struct MemberFuncPtrStore
		{
			MemberFuncPtr_t<TClass, TRet, TArgs...> ptr;
		};

		// A storage struct for const member function pointers (they are bigger than regular pointers)
		template<class TClass, typename TRet, typename... TArgs>
		struct MemberConstFuncPtrStore
		{
			MemberConstFuncPtr_t<TClass, TRet, TArgs...> ptr;
		};

		// SFINE variables for adding operators
		
		template<class, class = void>
		constexpr bool has_add_v = false;
		template<class T>
		constexpr bool has_add_v<T, std::void_t<decltype(std::declval<T>() + std::declval<T>())>> = std::is_same_v<decltype(std::declval<T>() + std::declval<T>()), T>;

		template<class, class = void>
		constexpr bool has_sub_v = false;
		template<class T>
		constexpr bool has_sub_v<T, std::void_t<decltype(std::declval<T>() - std::declval<T>())>> = std::is_same_v<decltype(std::declval<T>() - std::declval<T>()), T>;

		template<class, class = void>
		constexpr bool has_mult_v = false;
		template<class T>
		constexpr bool has_mult_v<T, std::void_t<decltype(std::declval<T>() * std::declval<T>())>> = std::is_same_v<decltype(std::declval<T>() * std::declval<T>()), T>;

		template<class, class = void>
		constexpr bool has_div_v = false;
		template<class T>
		constexpr bool has_div_v<T, std::void_t<decltype(std::declval<T>() / std::declval<T>())>> = std::is_same_v<decltype(std::declval<T>() / std::declval<T>()), T>;

		template<class, class = void>
		constexpr bool has_eq_v = false;
		template<class T>
		constexpr bool has_eq_v<T, std::void_t<decltype(std::declval<T>() == std::declval<T>())>> = std::is_same_v<decltype(std::declval<T>() == std::declval<T>()), bool>;

		template<class, class = void>
		constexpr bool has_lt_v = false;
		template<class T>
		constexpr bool has_lt_v<T, std::void_t<decltype(std::declval<T>() < std::declval<T>())>> = std::is_same_v<decltype(std::declval<T>() == std::declval<T>()), bool>;

		template<class, class = void>
		constexpr bool has_lte_v = false;
		template<class T>
		constexpr bool has_lte_v<T, std::void_t<decltype(std::declval<T>() <= std::declval<T>())>> = std::is_same_v<decltype(std::declval<T>() == std::declval<T>()), bool>;

		// Class for wrapping a type to be used in lua
		// You don't need to store objects of this class, just call the register_type function
		template<class TClass>
		class TypeWrapper
		{
		private:
			lua_State* L;
		public:
			TypeWrapper(lua_State* L) : L(L)
			{
				// Name of the type from the required static method
				// This is required for pushing userdata to the stack
				constexpr const char* name = TClass::lua_type_name();
				
				// Check if the type exists
				lua_getglobal(L, name);
				if (lua_istable(L, -1))
				{
					// If there is a global table named the same as this type, we assume that this type is already registered
					// Pop the value to clear the stack and return form the constructor
					lua_pop(L, 1);
					return;
				}
				
				lua_newtable(L); // Create a new table for the type
				lua_pushvalue(L, -1); // Push the value again, as a reference to the main table
				lua_setglobal(L, name); // Set the global (as the type table)

				luaL_newmetatable(L, name); // Register a metatable for the type
				lua_pushvalue(L, -2);
				lua_setfield(L, -2, "__index"); // Set the type table as the __index function (objects will look for method in this table)

				// Add a destructor in the __gc metamethod if the object requires it
				if constexpr (!std::is_trivially_destructible_v<TClass>)
				{
					lua_pushcfunction(L, [](lua_State* L) -> int
						{
							TClass* ptr = (TClass*)lua_touserdata(L, 1);
							ptr->~TClass();
							return 0;
						});
					lua_setfield(L, -2, "__gc"); // Set the __gc metatable field to the destructor call
				}

				lua_pop(L, 3); // Pop the type table, the metatable, and the nil that was given when checking if type was registerd
			}

			// Adds a constructor with the specified types
			// Constructors should be added last
			// There is support for only one constructor for now
			template<typename... TArgs>
			void add_constructor()
			{				
				// Get the type table and prepare the the key 'new' to add to it (Objects are created by calling TypeName.new(args...))
				lua_getglobal(L, TClass::lua_type_name());
				lua_pushliteral(L, "new");
				lua_pushcfunction(L, [](lua_State* L) -> int
					{
						TClass* ptr = (TClass*)lua_newuserdata(L, sizeof(TClass)); // Allocate memory for the object
						new(ptr) TClass{ internal::pop_param_form_stack<TArgs>(L)... }; // Call a inplace new constructor (Creates the object on the specified addres)
						luaL_getmetatable(L, TClass::lua_type_name()); // Get the metatable and assign it to the created object
						lua_setmetatable(L, -2);
						return 1;
					});
				lua_rawset(L, -3);
				lua_pop(L, 1); // Pop the type table
			}

			// Adds detected operators to the type
			// For a operator to be detected it has to take this type as the right and left side of the operator
			TypeWrapper& add_detected_operators()
			{
				luaL_getmetatable(L, TClass::lua_type_name());

				// Register the add operator
				if constexpr (has_add_v<TClass>)
				{
					lua_pushcfunction(L, [](lua_State* L) -> int
						{
							if (!lua_isuserdata(L, 1) || !lua_isuserdata(L, 2))
								return 0;
							TClass* lhs = (TClass*)lua_touserdata(L, 1);
							TClass* rhs = (TClass*)lua_touserdata(L, 2);
							stack_push<TClass>(L, *lhs + *rhs);
							return 1;
						});
					lua_setfield(L, -2, "__add");
				}

				// Register the subtract operator
				if constexpr (has_sub_v<TClass>)
				{
					lua_pushcfunction(L, [](lua_State* L) -> int
						{
							if (!lua_isuserdata(L, 1) || !lua_isuserdata(L, 2))
								return 0;
							TClass* lhs = (TClass*)lua_touserdata(L, 1);
							TClass* rhs = (TClass*)lua_touserdata(L, 2);
							stack_push<TClass>(L, *lhs - *rhs);
							return 1;
						});
					lua_setfield(L, -2, "__sub");
				}

				// Register the multiply operator
				if constexpr (has_mult_v<TClass>)
				{
					lua_pushcfunction(L, [](lua_State* L) -> int
						{
							if (!lua_isuserdata(L, 1) || !lua_isuserdata(L, 2))
								return 0;
							TClass* lhs = (TClass*)lua_touserdata(L, 1);
							TClass* rhs = (TClass*)lua_touserdata(L, 2);
							stack_push<TClass>(L, *lhs * *rhs);
							return 1;
						});
					lua_setfield(L, -2, "__mul");
				}

				// Register the multiply operator
				if constexpr (has_div_v<TClass>)
				{
					lua_pushcfunction(L, [](lua_State* L) -> int
						{
							if (!lua_isuserdata(L, 1) || !lua_isuserdata(L, 2))
								return 0;
							TClass* lhs = (TClass*)lua_touserdata(L, 1);
							TClass* rhs = (TClass*)lua_touserdata(L, 2);
							stack_push<TClass>(L, *lhs / *rhs);
							return 1;
						});
					lua_setfield(L, -2, "__div");
				}

				// Register the equlality operator
				if constexpr (has_eq_v<TClass>)
				{
					lua_pushcfunction(L, [](lua_State* L) -> int
						{
							if (!lua_isuserdata(L, 1) || !lua_isuserdata(L, 2))
								return 0;
							TClass* lhs = (TClass*)lua_touserdata(L, 1);
							TClass* rhs = (TClass*)lua_touserdata(L, 2);
							lua_pushboolean(L, *lhs == *rhs);
							return 1;
						});
					lua_setfield(L, -2, "__eq");
				}

				// Register the less-than operator
				if constexpr (has_lt_v<TClass>)
				{
					lua_pushcfunction(L, [](lua_State* L) -> int
						{
							if (!lua_isuserdata(L, 1) || !lua_isuserdata(L, 2))
								return 0;
							TClass* lhs = (TClass*)lua_touserdata(L, 1);
							TClass* rhs = (TClass*)lua_touserdata(L, 2);
							lua_pushboolean(L, *lhs < *rhs);
							return 1;
						});
					lua_setfield(L, -2, "__lt");
				}

				// Register the less-than or equal operator
				if constexpr (has_lt_v<TClass>)
				{
					lua_pushcfunction(L, [](lua_State* L) -> int
						{
							if (!lua_isuserdata(L, 1) || !lua_isuserdata(L, 2))
								return 0;
							TClass* lhs = (TClass*)lua_touserdata(L, 1);
							TClass* rhs = (TClass*)lua_touserdata(L, 2);
							lua_pushboolean(L, *lhs <= *rhs);
							return 1;
						});
					lua_setfield(L, -2, "__le");
				}

				return *this;
			}

			// Registers a nonconst member function to lua
			template<typename TRet, typename... TArgs>
			TypeWrapper& add_method(const char* name, internal::MemberFuncPtr_t<TClass, TRet, TArgs...> methodPtr)
			{				
				lua_getglobal(L, TClass::lua_type_name()); // Get the type table
				lua_pushstring(L, name); // Push the name of the method

				// Create a userdata store for a member function pointer
				// They are bigger than regular pointers (so we store them in a struct)
				// In GCC 11.2.0 a void* takes 8 bytes, and a member pointer takes 16 bytes, so we can't store this in a lightuserdata
				auto store = (internal::MemberFuncPtrStore<TClass, TRet, TArgs...>*)lua_newuserdata(L, sizeof(internal::MemberFuncPtrStore<TClass, TRet, TArgs...>));
				store->ptr = methodPtr;

				// We will call the method as a closure (so we can take the member method pointer)
				lua_pushcclosure(L, [](lua_State* L) -> int
					{
						// Get the pointer store form the upvalues
						auto methodPtr = ((internal::MemberFuncPtrStore<TClass, TRet, TArgs...>*)lua_touserdata(L, lua_upvalueindex(1)))->ptr;
						TClass* obj = (TClass*)lua_touserdata(L, 1);
						lua_remove(L, 1); // Pop the object pointer form the stack, so we are only left with the arguments on it (we collect arguments by goint throught the stack form the begining)
						std::tuple<TArgs...> args = { pop_param_form_stack<TArgs>(L) ... }; // pop the arguments into a tuple
						if constexpr (std::is_void_v<TRet>)
						{
							// Concatenate the arguments tuple with the object pointer to know on what object to call the method
							std::apply(methodPtr, std::tuple_cat(std::make_tuple(obj), args));
							return 0;
						}
						else
						{
							// Concatenating is the same
							// Returning a value works the same as in calling regular function
							TRet retVal = std::apply(methodPtr, std::tuple_cat(std::make_tuple(obj), args));
							stack_push(L, retVal);
							return 1;
						}
					}, 1);
				// Raw set the method to the type table
				lua_rawset(L, -3);
				return *this;
			}

			// registers a const member function to lua
			template<typename TRet, typename... TArgs>
			TypeWrapper& add_const_method(const char* name, internal::MemberConstFuncPtr_t<TClass, TRet, TArgs...> methodPtr)
			{
				lua_getglobal(L, TClass::lua_type_name()); // Get the type table
				lua_pushstring(L, name); // Push the name of the method

				// Create a userdata store for a member function pointer
				// They are bigger than regular pointers (so we store them in a struct)
				// In GCC 11.2.0 a void* takes 8 bytes, and a member pointer takes 16 bytes, so we can't store this in a lightuserdata
				auto store = (internal::MemberConstFuncPtrStore<TClass, TRet, TArgs...>*)lua_newuserdata(L, sizeof(internal::MemberConstFuncPtrStore<TClass, TRet, TArgs...>));
				store->ptr = methodPtr;

				// We will call the method as a closure (so we can take the member method pointer)
				lua_pushcclosure(L, [](lua_State* L) -> int
					{
						// Get the pointer store form the upvalues
						auto methodPtr = ((internal::MemberConstFuncPtrStore<TClass, TRet, TArgs...>*)lua_touserdata(L, lua_upvalueindex(1)))->ptr;
						TClass* obj = (TClass*)lua_touserdata(L, 1);
						lua_remove(L, 1); // Pop the object pointer form the stack, so we are only left with the arguments on it (we collect arguments by goint throught the stack form the begining)
						std::tuple<TArgs...> args = { pop_param_form_stack<TArgs>(L) ... }; // pop the arguments into a tuple
						if constexpr (std::is_void_v<TRet>)
						{
							// Concatenate the arguments tuple with the object pointer to know on what object to call the method
							std::apply(methodPtr, std::tuple_cat(std::make_tuple(obj), args));
							return 0;
						}
						else
						{
							// Concatenating is the same
							// Returning a value works the same as in calling regular function
							TRet retVal = std::apply(methodPtr, std::tuple_cat(std::make_tuple(obj), args));
							stack_push(L, retVal);
							return 1;
						}
					}, 1);
				// Raw set the method to the type table
				lua_rawset(L, -3);
				return *this;
			}
		};
	}

	// Registers a C++ type in the lua VM
	// Wrapped types are required to have a static method with the signature: 'const char* lua_type_name(void)'
	template<class TClass>
	internal::TypeWrapper<TClass> register_type(lua_State* L)
	{
		if constexpr (internal::has_lua_type_name_v<TClass>)
			return internal::TypeWrapper<TClass>(L);
		else
			internal::no_lua_type();
	}

	// Registers a global function called 'instanceof' that takes two arguments (userdata and a type table)
	// Returns true in lua when the userdata has the same type as the type table
	void register_instance_of_function(lua_State* L)
	{
		lua_getglobal(L, "instanceof");
		if (lua_iscfunction(L, -1)) // Check if the function is already registered
		{
			lua_pop(L, 1); // Pop the function
			return;
		}

		lua_pushcfunction(L, [](lua_State* L) -> int
			{
				if (lua_isuserdata(L, 1))
				{
					void* ptr = lua_touserdata(L, 1);
					lua_getmetatable(L, 1); // Now on index -1
					if (lua_istable(L, -1))
					{
						lua_pushliteral(L, "__index");
						lua_rawget(L, -2); // Get the __index field from the metatable
						lua_pushboolean(L, lua_rawequal(L, -1, 2)); // Push the value of the equality check
					}
					else
						lua_pushboolean(L, false); // Doesn't have a metatable, so can't check
				}
				else
					lua_pushboolean(L, false); // Is not userdata, so can't check

				return 1;
			});
		lua_setglobal(L, "instanceof");
	}

	//----------------------------
	// COROUTINES
	//----------------------------

	// TODO: Implement...

	//----------------------------
	// THREADS
	//----------------------------

	// TODO: Implement...

}
#endif // End of LUA_W_INCLUDE_H