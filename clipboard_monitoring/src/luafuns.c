#include "luafuns.h"

#include <gmodule.h>

struct LuaFuns luafuns = {};

void load_luafuns()
{
	GModule* module = g_module_open( NULL, G_MODULE_BIND_LAZY );

#define X( NAME, VAR ) g_module_symbol( module, NAME, ( gpointer* )&VAR );
	X( "lua_isnumber", lua_isnumber )
	X( "lua_createtable", lua_createtable )
	X( "lua_pushboolean", lua_pushboolean )
	X( "lua_pushcclosure", lua_pushcclosure )
	X( "lua_pushinteger", lua_pushinteger )
	X( "lua_pushlstring", lua_pushlstring )
	X( "lua_pushstring", lua_pushstring )
	X( "lua_pushnil", lua_pushnil )
	X( "lua_rawseti", lua_rawseti )
	X( "lua_settable", lua_settable )
	X( "lua_settop", lua_settop )
	X( "lua_tointeger", lua_tointeger )
	X( "lua_tolstring", lua_tolstring )
	X( "lua_type", lua_type )
	X( "lua_getfield", lua_getfield )
	X( "luaL_loadfile", luaL_loadfile )
	X( "luaL_loadstring", luaL_loadstring )
	X( "lua_pcall", lua_pcall )
	X( "lua_close", lua_close )
	X( "luaL_checkinteger", luaL_checkinteger )
	X( "lua_typename", lua_typename )
	X( "lua_gettop", lua_gettop )
	X( "lua_pushvalue", lua_pushvalue )
	X( "lua_remove", lua_remove )
	X( "lua_insert", lua_insert )
	X( "lua_replace", lua_replace )
	X( "lua_checkstack", lua_checkstack )
	X( "lua_next", lua_next )
	X( "lua_toboolean", lua_toboolean )
#undef X

	g_module_close( module );
}
