/*
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
*/

#pragma once

// Classic include guard if the compiler doesn't support #pragma once
#ifndef LUA_W_INCLUDE_H
#define LUA_W_INCLUDE_H

// Use this directive to DISABLE pointer safety (pointer safety uses RTTI)
// #define LUA_W_NO_PTR_SAFETY

#include <lua.hpp>

#include <tuple> // Used in: registered_function, call_lua_func_impl(_void), Function class, TypeWrapper class (and anything that calls them)
#include <type_traits> // Used in: stack_push, stack_get, registered_function, call_lua_function(_void)... (and anything that calls them)
#include <memory> // Used in: Table class and Function class
#include <new> // Used in TypeWrapper (for inplace new)
#include <utility> // Used in TypeWrapper (for checking if operator overloads exist)
#include <string> // Used in stack_push and stack_get for C++ string support

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
    
        class Error : public std::runtime_error {
            const char* typeName;
        public:
            Error(const char* type, const char* message);
            const char* type() const;
        };
    }

    //----------------------------
    // HELPER FUNCTIONS
    //----------------------------

    // Lua's standard libraries
    namespace Libs {
        enum Libs : uint16_t {
            base = 1, 			coroutine = 1 << 1, debug = 1 << 2, 
            io = 1 << 3, 		math = 1 << 4, 		os = 1 << 5,	
            package = 1 << 6,	string = 1 << 7, 	table = 1 << 8, 
            utf8 = 1 << 9, 		all = ((1 << 16) - 1)
        };
    }

    // Adds lua_w data to the created state
    // You should always call this, but it is only required when using
    // lua_w::Table or lua_w::Function
    void init(lua_State* L);
    
    // Opens the passed in libs to the passed Lua state
    // If you want to include everything pass Libs::all
    // If you want for example: base and math pass (Libs::base | Libs::math)
    void open_libs(lua_State* L, uint16_t libs) noexcept;

    #ifndef LUA_W_NO_PTR_SAFETY
    // Base class for all of the registered Lua types
    class LuaBaseObject { public: virtual ~LuaBaseObject() {} };
    #endif

    // Internal data for functions and tables
    namespace internal {
        template<typename TValue>
        void stack_push(lua_State* L, const TValue& value) noexcept;

        template<typename TValue>
        TValue stack_get(lua_State* L, int idx);

        // Object that holds a holds a reference to a Lua object so it is accessible in C++ and will not be garbage collected by Lua
        struct LuaObjectReference {
            lua_State* L;
            inline const void* get_object_id() const noexcept { return (void*)this; }
            LuaObjectReference(lua_State* L) {
                // Try to get the main lua state, so we will not save a temporary state
                lua_getfield(L, LUA_REGISTRYINDEX, "LUA_W_MAIN_STATE");
                if (lua_islightuserdata(L, -1)) {
                    this->L = (lua_State*)lua_touserdata(L, -1);
                    lua_pop(L, 1);
                }
                else
                    throw lua_w::internal::Error(nullptr, "lua_w was not initialized");
            }
            ~LuaObjectReference() {
                lua_pushnil(L);
                lua_rawsetp(L, LUA_REGISTRYINDEX, get_object_id());
            }
        };

        // Calls a lua function that is already on the stack. This function can have one return value
        template<typename TRet, typename... TArgs>
        TRet call_lua_func_impl(lua_State* L, TArgs... args) {
            (internal::stack_push(L, args), ...); // Push all of the arguments
            lua_call(L, sizeof...(args), 1);
            auto retVal = internal::stack_get<TRet>(L, -1); // get the value form the stack and return it
            lua_pop(L, 1);
            return retVal;
        }

        template<typename... TArgs>
        void call_lua_func_impl_no_ret(lua_State* L, TArgs... args) {
            (internal::stack_push(L, args), ...); // Push all of the arguments
            lua_call(L, sizeof...(args), 0);
        }

        // Helper for checking if the signature of the 'for_each' table function matches the required types
        template<class, typename, typename, class = void>
        constexpr bool for_each_matches_v = false;

        template<class T, typename TKey, typename TValue>
        constexpr bool for_each_matches_v<T, TKey, TValue, std::void_t<decltype(std::declval<T>()(std::declval<TKey>(), std::declval<TValue>()))>> = true;
    }

    //----------------------------
    // TABLES
    //----------------------------

    // Class that represents a lua table
    // Doesn't store any data, only the required pointers to access the bound table in the lua VM
    class Table {
        std::shared_ptr<internal::LuaObjectReference> tablePtr;
        Table(const std::shared_ptr<internal::LuaObjectReference>& ref) : tablePtr(ref) {}
    public:
        // Only used to retrieve tables form the stack
        // No need to use this function on it's own
        static Table get_form_stack(lua_State* L, int idx) noexcept {
            Table tab(std::make_shared<internal::LuaObjectReference>(L));

            lua_pushvalue(L, idx);
            lua_rawsetp(L, LUA_REGISTRYINDEX, tab.tablePtr->get_object_id());

            return tab;
        }

        // Constructs a new table on the provided lua_State
        Table(lua_State* L) : tablePtr(std::make_shared<internal::LuaObjectReference>(L)) {
            lua_newtable(L);
            lua_rawsetp(L, LUA_REGISTRYINDEX, tablePtr->get_object_id());
        }

        // Pushes the table that this object holds on to the stack
        // No need to use this function on it's own
        void push_to_stack(lua_State* L) const noexcept {
            lua_rawgetp(L, LUA_REGISTRYINDEX, tablePtr->get_object_id());
        }

        // Returns the amount of elements in the table
        lua_Unsigned length() const noexcept {
            lua_State* L = tablePtr->L;
            lua_rawgetp(L, LUA_REGISTRYINDEX, tablePtr->get_object_id());

            lua_len(L, -1);
            lua_Unsigned retVal = lua_tonumber(L, -1);

            lua_pop(L, 2);
            return retVal;
        }

        // Returns a value that was keyed by the passed in key
        // TKey, and TValue can be anything that can be pushed and pulled from the stack
        template<typename TValue, typename TKey>
        TValue get(const TKey& key) const {
            using key_t = std::decay_t<TKey>;
            lua_State* L = tablePtr->L;
            lua_rawgetp(L, LUA_REGISTRYINDEX, tablePtr->get_object_id());

            if constexpr (std::is_same_v<key_t, const char*> || std::is_same_v<key_t, char*>)
                lua_getfield(L, -1, key);
            else if constexpr (std::is_same_v<key_t, std::string>)
                lua_getfield(L, -1, key.c_str());
            else if constexpr (std::is_convertible_v<key_t, lua_Integer>)
                lua_geti(L, -1, (lua_Integer)key);
            else {
                internal::stack_push(L, key);
                lua_gettable(L, -2);
            }

            auto retVal = internal::stack_get<TValue>(L, -1);

            lua_pop(L, 2);
            return retVal;
        }

        // Sets the passed value in the table under the passed in key
        // TKey, and TValue can be anything that can be pushed and pulled from the stack
        template<typename TKey, typename TValue>
        void set(const TKey& key, const TValue& value) const noexcept {
            using key_t = std::decay_t<TKey>;
            lua_State* L = tablePtr->L;
            lua_rawgetp(L, LUA_REGISTRYINDEX, tablePtr->get_object_id());

            if constexpr (std::is_same_v<key_t, const char*> || std::is_same_v<key_t, char*>) {
                internal::stack_push(L, value);
                lua_setfield(L, -2, key);
            }
            else if constexpr (std::is_same_v<key_t, std::string>) {
                internal::stack_push(L, value);
                lua_setfield(L, -2, key.c_str());
            }
            else if constexpr (std::is_convertible_v<key_t, lua_Integer>) {
                internal::stack_push(L, value);
                lua_seti(L, -2, (lua_Integer)key);
            }
            else {
                internal::stack_push(L, key);
                internal::stack_push(L, value);
                lua_settable(L, -3);
            }
            lua_pop(L, 1);
        }
    
        template<typename TKey, typename TValue, typename Function>
        void for_each(const Function& function) const {
            static_assert(internal::for_each_matches_v<Function, TKey, TValue>, "The 'for_each' callable can't be called with the 'TKey', and 'TValue' types");
            lua_State* L = tablePtr->L;
            lua_rawgetp(L, LUA_REGISTRYINDEX, tablePtr->get_object_id());
            lua_pushnil(L);
            try {
                while (lua_next(L, -2) != 0) {
                    function(internal::stack_get<TKey>(L, -2), internal::stack_get<TValue>(L, -1));
                    lua_pop(L, 1);
                }
                lua_pop(L, 1);
            } catch (const lua_w::internal::Error& e) {
                luaL_error(L, "While iterating over a table, key or value couldn't be retrieved or were of a wrong type (%s)", e.what());
            }
        }
    };

    //----------------------------
    // Functions
    //----------------------------

    // Class that represents a lua function
    class Function {
        std::shared_ptr<internal::LuaObjectReference> funcPtr;
        Function(const std::shared_ptr<internal::LuaObjectReference>& ref) : funcPtr(ref) {}
    public:
        // Only used to retrieve functions form the stack
        // No need to use this function on it's own
        static Function get_form_stack(lua_State* L, int idx) noexcept {
            Function func(std::make_shared<internal::LuaObjectReference>(L));

            lua_pushvalue(L, idx);
            lua_rawsetp(L, LUA_REGISTRYINDEX, func.funcPtr->get_object_id());

            return func;
        }

        // A call operator for functions that don't require a return value
        // This can only be used when you don't need the return value of the function. Otherwise use the 'call' function
        template<typename... TArgs>
        void operator()(TArgs... args) {
            lua_State* L = funcPtr->L;
            lua_rawgetp(L, LUA_REGISTRYINDEX, funcPtr->get_object_id());
            internal::call_lua_func_impl_no_ret<TArgs...>(L, std::move(args) ...);
        }

        // Calls a function and expects something in return
        template<typename TRet, typename... TArgs>
        TRet call(TArgs... args) const noexcept {
            lua_State* L = funcPtr->L;
            lua_rawgetp(L, LUA_REGISTRYINDEX, funcPtr->get_object_id());
            if constexpr (std::is_void_v<TRet>)
                internal::call_lua_func_impl_no_ret<TArgs...>(L, std::move(args) ...);
            else
                return internal::call_lua_func_impl<TRet, TArgs...>(L, std::move(args) ...);
        }

        // Pushes the function that this object holds on to the stack
        // No need to use this function on it's own
        void push_to_stack(lua_State* L) const noexcept {
            lua_rawgetp(L, LUA_REGISTRYINDEX, funcPtr->get_object_id());
        }
    };

    //----------------------------
    // STACK MANIPULATIONS
    //----------------------------

    namespace internal {
        // NOTE: You only need to use this function if you want to directly manipulate the stack
        // Pushes the TValue on to the stack (can push numbers, bools, c-style strings, lua_w::Tables, lua_w::Functions, all pointers and copies of objects registerd in the lua VM)
        template<typename TValue>
        void stack_push(lua_State* L, const TValue& value) noexcept {
            static_assert(!std::is_reference_v<TValue>, "Using references is not supported, use pointers instead");
            using value_t = std::decay_t<TValue>; // Remove references, const and volatile kewyords to better match the types
            if constexpr (std::is_same_v<value_t, Table> || std::is_same_v<value_t, Function>) // Table and Function have the same interface
                value.push_to_stack(L);
            else if constexpr (std::is_same_v<value_t, bool>)
                lua_pushboolean(L, value);
            else if constexpr (std::is_convertible_v<value_t, lua_Number>)
                lua_pushnumber(L, static_cast<lua_Number>(value)); // Can push anything convertible to a lua_Number (double by default)
            else if constexpr (std::is_same_v<value_t, const char*> || std::is_same_v <value_t, char*>) // Lua makes a copy of the string
                lua_pushstring(L, value);
            else if constexpr (std::is_same_v<value_t, std::string>)
                lua_pushstring(L, value.c_str());
            else if constexpr (std::is_pointer_v<value_t>)
                lua_pushlightuserdata(L, (void*)value);
            else if constexpr (internal::has_lua_type_name_v<value_t>) {
                static_assert(std::is_copy_constructible_v<value_t>, "To push a full object to the stack this object has to be copy constructible");
                // Allocate memory, call copy constructor, set metatable...
                TValue* ptr = (TValue*)lua_newuserdatauv(L, sizeof(TValue), 0);
                new(ptr) TValue(value);
                luaL_setmetatable(L, std::remove_pointer_t<value_t>::lua_type_name());
            }
            else
                internal::no_match(); // No matching type was found
        }

        // NOTE: You only need to use this function if you want to directly manipulate the stack
        // Returns a value from the provided stack position. If the value can't be converted to the required type this function will throw an exception
        // Pointers may be managed by Lua so be careful if you are taking ownership of them
        template<typename TValue>
        TValue stack_get(lua_State* L, int idx) {
            static_assert(!std::is_reference_v<TValue>, "Using references is not supported, use pointers instead");
            using value_t = std::decay_t<TValue>; // Remove references, const and volatile kewyords to better match the types
            if constexpr (std::is_same_v<value_t, Function>)
                return lua_isfunction(L, idx) ? Function::get_form_stack(L, idx) : throw lua_w::internal::Error("function", "Required value is not a function");
            else if constexpr (std::is_same_v<value_t, Table>)
                return lua_istable(L, idx) ? Table::get_form_stack(L, idx) : throw lua_w::internal::Error("table", "Required value is not a table");
            else if constexpr (std::is_same_v <value_t, bool>)
                return lua_isboolean(L, idx) ? lua_toboolean(L, idx) : throw lua_w::internal::Error("bool", "Required value is not a bool");
            else if constexpr (std::is_convertible_v<value_t, lua_Number>)
                return lua_isnumber(L, idx) ? static_cast<TValue>(lua_tonumber(L, idx)) : throw lua_w::internal::Error("number", "Required value is not numeric");
            else if constexpr (std::is_same_v<value_t, const char*>)
                return lua_isstring(L, idx) ? lua_tostring(L, idx) : throw lua_w::internal::Error("string", "Required value is not a string");
            else if constexpr (std::is_same_v<value_t, std::string>)
                return lua_isstring(L, idx) ? std::string(lua_tostring(L, idx)) : throw lua_w::internal::Error("string", "Required value is not a string");
            else if constexpr (std::is_pointer_v<value_t>) {
                #ifndef LUA_W_NO_PTR_SAFETY
                if constexpr (std::is_convertible_v<value_t, LuaBaseObject*>) {
                    TValue ptr = dynamic_cast<TValue>((LuaBaseObject*)lua_touserdata(L, idx));
                    if(ptr)
                        return ptr;
                    else
                        throw lua_w::internal::Error("pointer", "Can't convert to the specified type");
                }
                else // WARNING!: There is no way to ensure that the pointer is of the appropriate type (we can only check it it is no null)
                #endif
                    return (TValue)lua_touserdata(L, idx);
            }
            else
                internal::no_match();
        }
    }

    //----------------------------
    // FUNCTION CALLING
    //----------------------------

    namespace internal {
        // Type alias for transforming two template arguments to a function pointer
        template<typename TRet, typename... TArgs>
        using FuncPtr_t = TRet(*)(TArgs...);

        // Function that will be invoked by Lua and call the required C function
        template<typename TRet, typename... TArgs>
        int registered_function(lua_State* L) noexcept {
            // Get all of the arguments from the function
            int argCounter = 1;
            try {
                // Retrieve the pointer to the C function form the upvalues that were passed to lua when this closure was created
                // You can think of upvalues as C++ lambda captures
                // Explanation - https://www.lua.org/pil/27.3.3.html
                // The pointer was passed as light user data so we retrieve it and cast to the required type
                auto ptr = (FuncPtr_t<TRet, TArgs...>)lua_touserdata(L, lua_upvalueindex(1)); // C style cast cause of the void* type
                std::tuple<TArgs...> args = { internal::stack_get<TArgs>(L, argCounter++) ... };
                // C functions can return void or one value, so we only need to take care of two things
                if constexpr (std::is_void_v<TRet>) {
                    // If return type is void just call the function using apply
                    std::apply(ptr, std::move(args));
                    return 0; // Returning 0 means not leaving anything on the stack
                } else {
                    internal::stack_push<TRet>(L, std::apply(ptr, std::move(args)));
                    return 1; // We leave one value on the stack
                }
            } catch (const lua_w::internal::Error& e) {
                luaL_typeerror(L, argCounter - 1, e.type());
                return 0;
            }
        }
    }

    // Calls a GLOBAL Lua function with the arguments and an expected return type
    template<typename TRet, typename... TArgs>
    TRet call_lua_function(lua_State* L, const char* funcName, TArgs... funcArgs) {
        // nodiscard helps with overload resolution. If the return value is discarded the compiler will choose the overload that returns void
        lua_getglobal(L, funcName); // Get function by name
        if constexpr (std::is_void_v<TRet>)
            internal::call_lua_func_impl_no_ret<TArgs...>(L, std::move(funcArgs) ...);
        else
            return internal::call_lua_func_impl<TRet, TArgs...>(L, std::move(funcArgs) ...);
    }
    
    // Creates a C function wrapper and LEAVES it on top of the stack
    template<typename TRet, typename... TArgs>
    void wrap_function(lua_State* L, internal::FuncPtr_t<TRet, TArgs...> funcPtr) noexcept {
        // Push the pointer to the function as a upvalue (so it can be retrieved in the registerd_function)
        lua_pushlightuserdata(L, (void*)funcPtr); // C style cast has to be made to avoid compilation errors
        // Register the function as a C closure (explanation - https://www.lua.org/pil/27.3.3.html)
        lua_pushcclosure(L, &internal::registered_function<TRet, TArgs...>, 1);
    }

    // Registers a C function of arbitrary signature as a global Lua function
    // The function will be called as normal if all arguments are present and have required types
    template<typename TRet, typename... TArgs>
    void register_function(lua_State* L, const char* funcName, internal::FuncPtr_t<TRet, TArgs...> funcPtr) noexcept {	
        wrap_function(L, funcPtr); // Wrap the function as a C closure
        lua_setglobal(L, funcName); // Assign the pushed closure a name to make it a global function
    }

    //----------------------------
    // GLOBAL VALUES
    //----------------------------

    // Attempts to get a global value form the lua VM.
    // If the global doen't exist or is of a wrong type this function throws an exception
    // If you don't want to handle exceptions check beforhand if everything matches using 'has_global'
    template<typename TValue>
    TValue get_global(lua_State* L, const char* globalName) {
        lua_getglobal(L, globalName); // Attempt to get a global by name, the value will be pushed to the lua stack
        auto value = internal::stack_get<TValue>(L, -1); // Get the value form the stack
        lua_pop(L, 1); // Pop the value of the stack, so it doesn't stay there
        return value;
    }

    // Creates or sets a global value in the lua VM
    template<typename TValue>
    void set_global(lua_State* L, const char* globalName, const TValue& value) noexcept {
        internal::stack_push(L, value); // Push the value to the stack
        lua_setglobal(L, globalName); // Bind a global name to this value
    }

    // Allows to safely check if a global exists and has the required type
    template<typename TValue>
    bool has_global(lua_State* L, const char* globalName) noexcept {
        try  { 
            get_global<TValue>(L, globalName); 
            return true; 
        } catch (const lua_w::internal::Error& e) {
            return false;
        }
    }

    //----------------------------
    // CLASS BINDING
    //----------------------------

    // Internal stuff for class binding
    namespace internal {
        // A pointer to a member function type (every class function that is not static is a member)
        // Static functions should use FuncPtr_t
        template<class TClass, typename TRet, typename... TArgs>
        using MemberFuncPtr_t = TRet(TClass::*)(TArgs...);

        // A pointer to a const member function
        template<class TClass, typename TRet, typename... TArgs>
        using MemberConstFuncPtr_t = TRet(TClass::*)(TArgs...) const;

        template<class TClass, typename TProp>
        using MemberPtr_t = TProp(TClass::*);

        // A storage struct for member function pointers (they are bigger than regular pointers)
        template<class TClass, typename TRet, typename... TArgs>
        struct MemberFuncPtrStore {
            MemberFuncPtr_t<TClass, TRet, TArgs...> ptr;
        };

        // A storage struct for const member function pointers (they are bigger than regular pointers)
        template<class TClass, typename TRet, typename... TArgs>
        struct MemberConstFuncPtrStore {
            MemberConstFuncPtr_t<TClass, TRet, TArgs...> ptr;
        };

        // Storage struct for member variables
        template<class TClass, typename TProp>
        struct MemberPtrStore {
            MemberPtr_t<TClass, TProp> ptr;
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
        constexpr bool has_lt_v<T, std::void_t<decltype(std::declval<T>() < std::declval<T>())>> = std::is_same_v<decltype(std::declval<T>() < std::declval<T>()), bool>;

        template<class, class = void>
        constexpr bool has_lte_v = false;
        template<class T>
        constexpr bool has_lte_v<T, std::void_t<decltype(std::declval<T>() <= std::declval<T>())>> = std::is_same_v<decltype(std::declval<T>() <= std::declval<T>()), bool>;

        template<class, class = void>
        constexpr bool has_unary_minus_v = false;
        template<class T>
        constexpr bool has_unary_minus_v<T, std::void_t<decltype(-std::declval<T>())>> = std::is_same_v<decltype(-std::declval<T>()), T>;

        // Implementation of a method call from Lua
        template<typename StoreType, class TClass, typename TRet, typename... TArgs>
        int call_method_impl(lua_State* L) {
            int argCounter = 2;
            try {
                // Get the method pointer form the storage struct
                auto methodPtr = ((StoreType*)lua_touserdata(L, lua_upvalueindex(1)))->ptr;
                // First argument is the pointer to the object to call the method on
                std::tuple<TClass*, TArgs...> args = { (TClass*)lua_touserdata(L, 1), internal::stack_get<TArgs>(L, argCounter++) ... };
                if constexpr (std::is_void_v<TRet>) {
                    std::apply(methodPtr, std::move(args));
                    return 0;
                } else {
                    internal::stack_push(L, std::apply(methodPtr, std::move(args)));
                    return 1; 
                }
            } catch (const lua_w::internal::Error& e) {
                luaL_typeerror(L, argCounter - 1, e.type());
                return 0;
            }
        }

        template<typename StoreType, typename MethodPtrType, class TClass, typename TRet, typename... TArgs>
        void wrap_method(lua_State* L, MethodPtrType methodPtr) {
            // Create a userdata store for a member function pointer
            // They are bigger than regular pointers (so we store them in a struct)
            // In GCC 11.2.0 a void* takes 8 bytes, and a member pointer takes 16 bytes, so we can't store this in a lightuserdata
            auto store = (StoreType*)lua_newuserdatauv(L, sizeof(StoreType), 0);
            store->ptr = methodPtr;
            // We will call the method as a closure (so we can take the member method pointer)
            lua_pushcclosure(L, &call_method_impl<StoreType, TClass, TRet, TArgs...>, 1);
        }

        // Class for wrapping a type to be used in lua
        // You don't need to store objects of this class, just call the register_type function
        template<class TClass>
        class TypeWrapper {
            lua_State* L;

            void add_constructor_impl(lua_CFunction constructionFunction) const noexcept {
                luaL_getmetatable(L, TClass::lua_type_name());
                lua_getfield(L, -1, "__index"); // __index field is the type table
                get_type_table_metatable(); // Metatable for the '__call metamethod
                lua_pushcfunction(L, constructionFunction); // Push the construction function
                lua_setfield(L, -2, "__call");
                lua_pop(L, 3); // Pop the metatable,  the type table and it's metatable
            }
        
            // Pushes the metatable on the stack (will create one if it doesn't exist) 
            void get_type_table_metatable() const noexcept {
                if(lua_getmetatable(L, -1)) // Object has a metatable and it is on the stack now
                    return;
                else {
                    lua_newtable(L); // Create the table
                    lua_pushvalue(L, -1); // Push the reference to it
                    lua_setmetatable(L, -3);
                }
            }
        public:
            TypeWrapper(lua_State* L) : L(L) {
                // Name of the type from the required static method
                // This is required for pushing userdata to the stack
                constexpr const char* name = TClass::lua_type_name();
                
                // Check if the type exists
                if (luaL_getmetatable(L, name) == LUA_TTABLE) {
                    // If there is a metatable named the same as this type, we assume that this type is already registered
                    // Pop the value to clear the stack and return form the constructor
                    lua_pop(L, 1);
                    return;
                }

                lua_newtable(L); // Create a new table for the type
                lua_pushvalue(L, -1);
                lua_setglobal(L, name); // Set the type table as a global (for access to static method)

                luaL_newmetatable(L, name); // Register a metatable for the type
                lua_pushvalue(L, -2);
                lua_setfield(L, -2, "__index"); // Set the type table as the __index function (objects will look for method in this table)

                // Add a destructor in the __gc metamethod if the object requires it
                if constexpr (!std::is_trivially_destructible_v<TClass>) {
                    lua_pushcfunction(L, [](lua_State* L) -> int {
                        TClass* ptr = (TClass*)lua_touserdata(L, 1);
                        ptr->~TClass();
                        return 0;
                    });
                    lua_setfield(L, -2, "__gc"); // Set the __gc metatable field to the destructor call
                }

                lua_pushstring(L, name);
                lua_setfield(L, -2, "__name");

                lua_pushliteral(L, "Can't access the metatable of a registered type");
                lua_setfield(L, -2, "__metatable");

                lua_pop(L, 3); // Pop the type table, the metatable, and the nil that was given when checking if type was registerd
            }

            // Adds a constructor with the specified types
            // Constructor has to be added last
            // If you only want a default constructor then don't pass any types to this method
            template<typename... TArgs>
            void add_constructor() const noexcept {				
                add_constructor_impl([](lua_State* L) -> int {
                    int argCounter = 2; // Omit the first argument (it's the type table)
                    try {
                        TClass* ptr = (TClass*)lua_newuserdatauv(L, sizeof(TClass), 0); // Allocate memory for the object
                        new(ptr) TClass{ internal::stack_get<TArgs>(L, argCounter++).value() ... }; // Call a inplace new constructor (Creates the object on the specified addres)
                        luaL_setmetatable(L, TClass::lua_type_name()); // Get the metatable and assign it to the created object
                        return 1;
                    } catch (const lua_w::internal::Error& e) {
                        luaL_typeerror(L, argCounter - 1, e.type());
                        return 0;
                    }
                });
                
            }

            // Adds a custom AND a default constructor (with no parameters)'
            // Constructor has to be added last
            // If you only want a default constructor use 'add_constructor()' with no passed types
            template<typename... TArgs>
            void add_custom_and_default_constructors() const noexcept {
                static_assert(std::is_default_constructible_v<TClass>, "'TClass' is not default constructible");
                add_constructor_impl([](lua_State* L) -> int {
                    int argCounter = 2; // Omit the first argument (it's the type table)
                    try {
                        TClass* ptr = (TClass*)lua_newuserdatauv(L, sizeof(TClass), 0); // Allocate memory for the object
                        if (lua_gettop(L) == 2) // Check if no arguments were passed first is the type table second is the created userdata
                            new(ptr) TClass(); // Call a default constructor (if no arguments were passed)
                        else
                            new(ptr) TClass{ internal::stack_get<TArgs>(L, argCounter++) ... }; // Call a inplace new constructor (Creates the object on the specified addres)
                        luaL_setmetatable(L, TClass::lua_type_name()); // Get the metatable and assign it to the created object
                        return 1;
                    } catch (const lua_w::internal::Error& e) {
                        luaL_typeerror(L, argCounter - 1, e.type());
                        return 0;
                    }
                });
            }

            // Adds detected operators to the type
            // For a operator to be detected it has to take this type as the right and left side of the operator
            const TypeWrapper& add_detected_operators() const noexcept {
                luaL_getmetatable(L, TClass::lua_type_name());

                // Register the add operator
                if constexpr (has_add_v<TClass>) {
                    lua_pushcfunction(L, [](lua_State* L) -> int {
                        if (!lua_isuserdata(L, 1) || !lua_isuserdata(L, 2))
                            return 0;
                        TClass* lhs = (TClass*)lua_touserdata(L, 1);
                        TClass* rhs = (TClass*)lua_touserdata(L, 2);
                        internal::stack_push<TClass>(L, *lhs + *rhs);
                        return 1;
                    });
                    lua_setfield(L, -2, "__add");
                }

                // Register the subtract operator
                if constexpr (has_sub_v<TClass>) {
                    lua_pushcfunction(L, [](lua_State* L) -> int {
                        if (!lua_isuserdata(L, 1) || !lua_isuserdata(L, 2))
                            return 0;
                        TClass* lhs = (TClass*)lua_touserdata(L, 1);
                        TClass* rhs = (TClass*)lua_touserdata(L, 2);
                        internal::stack_push<TClass>(L, *lhs - *rhs);
                        return 1;
                    });
                    lua_setfield(L, -2, "__sub");
                }

                // Register the multiply operator
                if constexpr (has_mult_v<TClass>) {
                    lua_pushcfunction(L, [](lua_State* L) -> int {
                        if (!lua_isuserdata(L, 1) || !lua_isuserdata(L, 2))
                            return 0;
                        TClass* lhs = (TClass*)lua_touserdata(L, 1);
                        TClass* rhs = (TClass*)lua_touserdata(L, 2);
                        internal::stack_push<TClass>(L, *lhs * *rhs);
                        return 1;
                    });
                    lua_setfield(L, -2, "__mul");
                }

                // Register the multiply operator
                if constexpr (has_div_v<TClass>) {
                    lua_pushcfunction(L, [](lua_State* L) -> int {
                        if (!lua_isuserdata(L, 1) || !lua_isuserdata(L, 2))
                            return 0;
                        TClass* lhs = (TClass*)lua_touserdata(L, 1);
                        TClass* rhs = (TClass*)lua_touserdata(L, 2);
                        internal::stack_push<TClass>(L, *lhs / *rhs);
                        return 1;
                    });
                    lua_setfield(L, -2, "__div");
                }

                // Register the unary minus
                if constexpr (has_unary_minus_v<TClass>) {
                    lua_pushcfunction(L, [](lua_State* L) -> int {
                        if (!lua_isuserdata(L, 1))
                            return 0;
                        TClass* obj = (TClass*)lua_touserdata(L, 1);
                        internal::stack_push<TClass>(L, -*obj);
                        return 1;
                    });
                    lua_setfield(L, -2, "__unm");
                }

                // Register the equlality operator
                if constexpr (has_eq_v<TClass>) {
                    lua_pushcfunction(L, [](lua_State* L) -> int {
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
                if constexpr (has_lt_v<TClass>) {
                    lua_pushcfunction(L, [](lua_State* L) -> int {
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
                if constexpr (has_lt_v<TClass>) {
                    lua_pushcfunction(L, [](lua_State* L) -> int {
                        if (!lua_isuserdata(L, 1) || !lua_isuserdata(L, 2))
                            return 0;
                        TClass* lhs = (TClass*)lua_touserdata(L, 1);
                        TClass* rhs = (TClass*)lua_touserdata(L, 2);
                        lua_pushboolean(L, *lhs <= *rhs);
                        return 1;
                    });
                    lua_setfield(L, -2, "__le");
                }

                lua_pop(L, 1);
                return *this;
            }

            // Registers a nonconst member function to lua
            template<typename TRet, typename... TArgs>
            const TypeWrapper& add_method(const char* name, internal::MemberFuncPtr_t<TClass, TRet, TArgs...> methodPtr) const noexcept {
                using StoreType = internal::MemberFuncPtrStore<TClass, TRet, TArgs...>;
                using MethodType = internal::MemberFuncPtr_t<TClass, TRet, TArgs...>;
                luaL_getmetatable(L, TClass::lua_type_name());
                lua_getfield(L, -1, "__index"); // __index field is the type table
                wrap_method<StoreType, MethodType, TClass, TRet, TArgs...>(L, methodPtr);
                lua_setfield(L, -2, name);
                lua_pop(L, 2); // Pop the type table
                return *this;
            }

            // Registers a const member function to lua
            template<typename TRet, typename... TArgs>
            const TypeWrapper& add_method(const char* name, internal::MemberConstFuncPtr_t<TClass, TRet, TArgs...> methodPtr) const noexcept {
                // Everything works the same as the non-const version
                using StoreType = internal::MemberConstFuncPtrStore<TClass, TRet, TArgs...>;
                using MethodType = internal::MemberConstFuncPtr_t<TClass, TRet, TArgs...>;
                luaL_getmetatable(L, TClass::lua_type_name());
                lua_getfield(L, -1, "__index");
                wrap_method<StoreType, MethodType, TClass, TRet, TArgs...>(L, methodPtr);
                lua_setfield(L, -2, name);
                lua_pop(L, 2);
                return *this;
            }

            // Adds a custom definition for one of lua's metamethod
            template<typename TRet, typename... TArgs>
            const TypeWrapper& add_metamethod(const char* methodName, internal::MemberFuncPtr_t<TClass, TRet, TArgs...> methodPtr) const noexcept {
                using StoreType = internal::MemberFuncPtrStore<TClass, TRet, TArgs...>;
                using MethodType = internal::MemberFuncPtr_t<TClass, TRet, TArgs...>;
                luaL_getmetatable(L, TClass::lua_type_name());
                wrap_method<StoreType, MethodType, TClass, TRet, TArgs...>(L, methodPtr);
                lua_setfield(L, -2, methodName);
                lua_pop(L, 1);
                return *this;
            }

            // Adds a custom definition for one of lua's metamethod (const method version)
            template<typename TRet, typename... TArgs>
            const TypeWrapper& add_metamethod(const char* methodName, internal::MemberConstFuncPtr_t<TClass, TRet, TArgs...> methodPtr) const noexcept {
                using StoreType = internal::MemberConstFuncPtrStore<TClass, TRet, TArgs...>;
                using MethodType = internal::MemberConstFuncPtr_t<TClass, TRet, TArgs...>;
                luaL_getmetatable(L, TClass::lua_type_name());
                wrap_method<StoreType, MethodType, TClass, TRet, TArgs...>(L, methodPtr);
                lua_setfield(L, -2, methodName);
                lua_pop(L, 1);
                return *this;
            }

            // Registers a member variable to lua
            template<typename TProp>
            const TypeWrapper& add_member(const char* name, internal::MemberPtr_t<TClass, TProp> memberPtr) const noexcept {
                // Assigning is the same as methods
                using PtrStore_t = internal::MemberPtrStore<TClass, TProp>;
                luaL_getmetatable(L, TClass::lua_type_name());
                lua_getfield(L, -1, "__index");
                auto store = (PtrStore_t*)lua_newuserdatauv(L, sizeof(PtrStore_t), 0);
                store->ptr = memberPtr;
                lua_pushcclosure(L, [](lua_State* L) -> int {
                    TClass* self = (TClass*)lua_touserdata(L, 1);
                    auto memberPtr = ((PtrStore_t*)lua_touserdata(L, lua_upvalueindex(1)))->ptr;
                    // When there are no additional arguments on the stack we want to access the variable
                    if (lua_gettop(L) < 2) {
                        internal::stack_push(L, self->*memberPtr);
                        return 1;
                    } else {
                        // We want to assign to it
                        try {
                            self->*memberPtr = internal::stack_get<TProp>(L, 2);
                        } catch (const lua_w::internal::Error& e) {
                            luaL_typeerror(L, 2, e.type());
                        }
                        return 0;
                    }
                }, 1);
                lua_setfield(L, -2, name);
                lua_pop(L, 2);
                return *this;
            }

            // register a static function to lua
            template<typename TRet, typename... TArgs>
            const TypeWrapper& add_static_method(const char* name, internal::FuncPtr_t<TRet, TArgs...> methodPtr) const noexcept {
                // Works the same as registering a normal function. The only difference is that this function will be called from the type table
                luaL_getmetatable(L, TClass::lua_type_name());
                lua_getfield(L, -1, "__index"); // __index field is the type table
                wrap_function(L, methodPtr);
                lua_setfield(L, -2, name);
                lua_pop(L, 2);
                return *this;
            }
        
            template<class TParentClass>
            const TypeWrapper& add_parent_type() const noexcept {
                static_assert(std::is_base_of_v<TParentClass, TClass>, "'TParentClass' is not a base type for 'TClass'");
                static_assert(has_lua_type_name_v<TParentClass>, "'TParentClass' has to have a 'static const char* lua_type_name() method'");
                #ifndef LUA_W_NO_PTR_SAFETY
                static_assert(std::is_base_of_v<LuaBaseObject, TParentClass>, "'TParentClass' has to derive from 'LuaBaseObject' when 'LUA_W_NO_PTR_SAFETY' is NOT defined");
                #endif

                luaL_getmetatable(L, TClass::lua_type_name());
                lua_getfield(L, -1, "__index"); // __index field is the type table
                get_type_table_metatable(); // Get (or add) the type's metatable
                luaL_getmetatable(L, TParentClass::lua_type_name());
                lua_getfield(L, -1, "__index"); // Get the parent type's type table
                lua_setfield(L, -3, "__index"); // Set the __index field to look in the parent implementation
                lua_pop(L, 4); // Pop the metatable, type table and it's metatable and parent's type table
                return *this;
            }
        };
    }

    // Registers a C++ type in the lua VM
    // Wrapped types are required to have a static method with the signature: 'const char* lua_type_name(void)'
    template<class TClass>
    internal::TypeWrapper<TClass> register_type(lua_State* L) noexcept {
        static_assert(internal::has_lua_type_name_v<TClass>, "Class has to have a static 'static const char* lua_type_name()' method");
        #ifndef LUA_W_NO_PTR_SAFETY
        static_assert(std::is_base_of_v<LuaBaseObject, TClass>, "'TClass' has to derive from 'LuaBaseObject' when 'LUA_W_NO_PTR_SAFETY' is NOT defined");
        #endif
        return internal::TypeWrapper<TClass>(L);
    }

    // Overrides a global 'type' function. It will work the same as the regular 'type' function with the ability to detect custom types
    void register_type_function(lua_State* L) noexcept;
}
#endif // End of LUA_W_INCLUDE_H

#ifdef LUA_W_IMPLEMENTATION
void lua_w::init(lua_State* L) {
    lua_pushlightuserdata(L, (void*)L);
    lua_setfield(L, LUA_REGISTRYINDEX, "LUA_W_MAIN_STATE");    
}

lua_w::internal::Error::Error(const char* type, const char* message) : std::runtime_error(message) {
    typeName = type ? type : "Unspecified";
}

const char* lua_w::internal::Error::type() const {
    return typeName;
}

void lua_w::open_libs(lua_State* L, uint16_t libs) noexcept {
    int popCount = 0;
    if (libs == Libs::all) { luaL_openlibs(L); }

    if (libs & Libs::base)      { luaL_requiref(L, LUA_GNAME, luaopen_base, 1); ++popCount; }
    if (libs & Libs::coroutine) { luaL_requiref(L, LUA_COLIBNAME, luaopen_coroutine, 1); ++popCount; }
    if (libs & Libs::debug)     { luaL_requiref(L, LUA_DBLIBNAME, luaopen_debug, 1); ++popCount; }
    if (libs & Libs::io)        { luaL_requiref(L, LUA_IOLIBNAME, luaopen_io, 1); ++popCount; }
    if (libs & Libs::math)      { luaL_requiref(L, LUA_MATHLIBNAME, luaopen_math, 1); ++popCount; }
    if (libs & Libs::os)        { luaL_requiref(L, LUA_OSLIBNAME, luaopen_os, 1); ++popCount; }
    if (libs & Libs::package)   { luaL_requiref(L, LUA_LOADLIBNAME, luaopen_package, 1); ++popCount; }
    if (libs & Libs::string)    { luaL_requiref(L, LUA_STRLIBNAME, luaopen_string, 1); ++popCount; }
    if (libs & Libs::table)     { luaL_requiref(L, LUA_TABLIBNAME, luaopen_table, 1); ++popCount; }
    if (libs & Libs::utf8)      { luaL_requiref(L, LUA_UTF8LIBNAME, luaopen_utf8, 1); ++popCount; }
    lua_pop(L, popCount);
}

void lua_w::register_type_function(lua_State* L) noexcept {
    lua_pushcfunction(L, [](lua_State* L) -> int {
        // Check if it has a special metafield with the type name
        if (luaL_getmetafield(L, 1, "__name") == LUA_TSTRING)
            return 1;
        lua_pushstring(L, luaL_typename(L, 1));
        return 1;
    });
    lua_setglobal(L, "type");
}
#endif // End of LUA_W_IMPLEMENTATION
