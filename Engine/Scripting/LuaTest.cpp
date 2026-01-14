#include "LuaTest.h"

extern "C"
{
    #include <lua.h>
    #include <lauxlib.h>
    #include <lualib.h>
}


#include <iostream>

void RunLuaSmokeTest()
{
    lua_State* L = luaL_newstate();

    if (!L)
    {
        std::cout << "[LuaTest] Failed to create Lua state\n";
        return;
    }

    luaL_openlibs(L);

    if (luaL_dostring(L, "print('Lua smoke test OK')") != LUA_OK)
    {
        const char* err = lua_tostring(L, -1);
        std::cout << "[LuaTest] Error: " << err << "\n";
    }

    lua_close(L);
}
