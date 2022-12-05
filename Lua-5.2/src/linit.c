/*
** $Id: linit.c,v 1.32.1.1 2013/04/12 18:48:47 roberto Exp $
** Initialization of libraries for lua.c and other clients
** See Copyright Notice in lua.h
*/


/*
** If you embed Lua in your program and need to open the standard
** libraries, call luaL_openlibs in your program. If you need a
** different set of libraries, copy this file to your project and edit
** it to suit your needs.
*/


#define linit_c
#define LUA_LIB

#include "lua.h"

#include "lualib.h"
#include "lauxlib.h"

#if LUA_LTR
#include "lrotable.h"
#endif


/*
** these libs are loaded by lua.c and are readily available to any Lua
** program
*/
static const luaL_Reg loadedlibs[] = {
#if !LUA_LTR
#ifdef CONFIG_LUA_BASE_LIB
  {"_G", luaopen_base},
#endif
#endif
#ifdef CONFIG_LUA_PACKAGE_LIB
  {LUA_LOADLIBNAME, luaopen_package},
#endif
#if !LUA_LTR
#ifdef CONFIG_LUA_COROUTINE_LIB
  {LUA_COLIBNAME, luaopen_coroutine},
#endif
#ifdef CONFIG_LUA_TABLE_LIB
  {LUA_TABLIBNAME, luaopen_table},
#endif
#endif
#ifdef CONFIG_LUA_IO_LIB
  {LUA_IOLIBNAME, luaopen_io},
#endif
#if !LUA_LTR
  #ifdef CONFIG_LUA_SYSTEM_LIB
  {LUA_OSLIBNAME, luaopen_os},
#endif
#ifdef CONFIG_LUA_STRING_LIB
  {LUA_STRLIBNAME, luaopen_string},
#endif
#ifdef CONFIG_LUA_BIT_LIB
  {LUA_BITLIBNAME, luaopen_bit32},
#endif
#ifdef CONFIG_LUA_MATH_LIB
  {LUA_MATHLIBNAME, luaopen_math},
#endif
#ifdef CONFIG_LUA_DEBUG_LIB
  {LUA_DBLIBNAME, luaopen_debug},
#endif
#endif
  {NULL, NULL}
};

#if LUA_LTR && !defined(CONFIG_EXTERN_ROTABLE)
const luaR_table lua_rotable[] =
{
#ifdef CONFIG_LUA_BASE_LIB
  {"__baselib", base_funcs, NULL},
#endif
#ifdef CONFIG_LUA_STRING_LIB
  {LUA_STRLIBNAME, strlib, NULL},
#endif
#ifdef CONFIG_LUA_MATH_LIB
  {LUA_MATHLIBNAME, mathlib, mathlib_vals},
#endif
#ifdef CONFIG_LUA_BIT_LIB
  {LUA_BITLIBNAME, bitlib, NULL},
#endif
#ifdef CONFIG_LUA_SYSTEM_LIB
  {LUA_OSLIBNAME, syslib, NULL},
#endif
#ifdef CONFIG_LUA_TABLE_LIB
  {LUA_TABLIBNAME, tab_funcs, NULL},
#endif
#ifdef CONFIG_LUA_COROUTINE_LIB
  {LUA_COLIBNAME, co_funcs, NULL},
#endif
#ifdef CONFIG_LUA_DEBUG_LIB
  {LUA_DBLIBNAME, dblib, NULL},
#endif
  {NULL, NULL, NULL}
};
#endif

/*
** these libs are preloaded and must be required before used
*/
static const luaL_Reg preloadedlibs[] = {
  {NULL, NULL}
};


LUALIB_API void luaL_openlibs (lua_State *L) {
  const luaL_Reg *lib;
  /* call open functions from 'loadedlibs' and set results to global table */
  for (lib = loadedlibs; lib->func; lib++) {
    luaL_requiref(L, lib->name, lib->func, 1);
    lua_pop(L, 1);  /* remove lib */
  }
  /* add open functions from 'preloadedlibs' into 'package.preload' table */
  luaL_getsubtable(L, LUA_REGISTRYINDEX, "_PRELOAD");
  for (lib = preloadedlibs; lib->func; lib++) {
    lua_pushcfunction(L, lib->func);
    lua_setfield(L, -2, lib->name);
  }
  lua_pop(L, 1);  /* remove _PRELOAD table */

#if LUA_LTR
#ifdef CONFIG_LUA_BASE_LIB
  lua_pushliteral(L, LUA_VERSION);
  lua_setglobal(L, "_VERSION");
#endif
#endif
}

