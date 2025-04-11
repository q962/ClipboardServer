#pragma once

#include "stdafx.h"

struct LuaFuns {
	int ( *lua_isnumber )( lua_State* L, int index );
	void ( *lua_createtable )( lua_State* L, int narr, int nrec );
	void ( *lua_pushboolean )( lua_State* L, int b );
	void ( *lua_pushcclosure )( lua_State* L, lua_CFunction fn, int n );
	void ( *lua_pushinteger )( lua_State* L, lua_Integer n );
	void ( *lua_pushlstring )( lua_State* L, const char* s, size_t len );
	void ( *lua_pushstring )( lua_State* L, const char* s );
	void ( *lua_pushnil )( lua_State* L );
	void ( *lua_rawseti )( lua_State* L, int index, int n );
	void ( *lua_settable )( lua_State* L, int index );
	void ( *lua_settop )( lua_State* L, int index );
	lua_Integer ( *lua_tointeger )( lua_State* L, int index );
	const char* ( *lua_tolstring )( lua_State* L, int index, size_t* len );
	int ( *lua_type )( lua_State* L, int index );
	void ( *lua_getfield )( lua_State* L, int index, const char* k );
	int ( *luaL_loadfile )( lua_State* L, const char* filename );
	int ( *luaL_loadstring )( lua_State* L, const char* s );
	int ( *lua_pcall )( lua_State* L, int nargs, int nresults, int errfunc );
	void ( *lua_close )( lua_State* L );
	lua_Integer ( *luaL_checkinteger )( lua_State* L, int narg );
	const char* ( *lua_typename )( lua_State* L, int tp );
	int ( *lua_gettop )( lua_State* L );
	void ( *lua_pushvalue )( lua_State* L, int index );
	void ( *lua_remove )( lua_State* L, int index );
	void ( *lua_insert )( lua_State* L, int index );
	void ( *lua_replace )( lua_State* L, int index );
	int ( *lua_checkstack )( lua_State* L, int extra );
	int ( *lua_next )( lua_State* L, int index );
	int ( *lua_toboolean )( lua_State* L, int index );
};
extern struct LuaFuns luafuns;

#define lua_isnumber luafuns.lua_isnumber
#define lua_createtable luafuns.lua_createtable
#define lua_pushboolean luafuns.lua_pushboolean
#define lua_pushcclosure luafuns.lua_pushcclosure
#define lua_pushinteger luafuns.lua_pushinteger
#define lua_pushlstring luafuns.lua_pushlstring
#define lua_pushstring luafuns.lua_pushstring
#define lua_pushnil luafuns.lua_pushnil
#define lua_rawseti luafuns.lua_rawseti
#define lua_settable luafuns.lua_settable
#define lua_settop luafuns.lua_settop
#define lua_tointeger luafuns.lua_tointeger
#define lua_tolstring luafuns.lua_tolstring
#define lua_type luafuns.lua_type
#define lua_getfield luafuns.lua_getfield
#define luaL_loadfile luafuns.luaL_loadfile
#define luaL_loadstring luafuns.luaL_loadstring
#define lua_pcall luafuns.lua_pcall
#define lua_close luafuns.lua_close
#define luaL_checkinteger luafuns.luaL_checkinteger
#define lua_typename luafuns.lua_typename
#define lua_gettop luafuns.lua_gettop
#define lua_pushvalue luafuns.lua_pushvalue
#define lua_remove luafuns.lua_remove
#define lua_insert luafuns.lua_insert
#define lua_replace luafuns.lua_replace
#define lua_checkstack luafuns.lua_checkstack
#define lua_next luafuns.lua_next
#define lua_toboolean luafuns.lua_toboolean

void load_luafuns();
