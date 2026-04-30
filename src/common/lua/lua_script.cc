/**
 * Copyright (c) 2024-2026 Stone Rhino and contributors.
 *
 * MIT License (http://opensource.org/licenses/MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
 * associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
 * NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "lua_script.h"

#include <cstdio>
#include <string_view>

#include "../../transaction.h"
namespace Wge {
namespace Common {
namespace Lua {
namespace {
#if LUA_VERSION_NUM >= 502
inline void pushGlobalTable(lua_State* L) { lua_pushglobaltable(L); }
inline bool setChunkEnv(lua_State* L, int chunk_index, int env_index) {
  lua_pushvalue(L, env_index);
  const char* upvalue_name = lua_setupvalue(L, chunk_index, 1);
  return upvalue_name && std::string_view(upvalue_name) == "_ENV";
}
#else
inline void pushGlobalTable(lua_State* L) { lua_pushvalue(L, LUA_GLOBALSINDEX); }
inline bool setChunkEnv(lua_State* L, int chunk_index, int env_index) {
  lua_pushvalue(L, env_index);
  lua_setfenv(L, chunk_index);
  return true;
}
#endif
} // namespace

LuaScript::~LuaScript() {
  if (state_ && env_ref_ != LUA_NOREF && env_ref_ != LUA_REFNIL) {
    luaL_unref(state_, LUA_REGISTRYINDEX, env_ref_);
    env_ref_ = LUA_NOREF;
  }
}

std::expected<bool, std::string> LuaScript::load(const std::string& script_path) {
  if (!state_) {
    return std::unexpected("lua state not initialized");
  }

  if (env_ref_ != LUA_NOREF && env_ref_ != LUA_REFNIL) {
    luaL_unref(state_, LUA_REGISTRYINDEX, env_ref_);
    env_ref_ = LUA_NOREF;
  }

  int stack_top = lua_gettop(state_);

  auto bc = LuaBytecodeCache::instance().get(script_path);
  if (bc.empty()) {
    lua_settop(state_, stack_top);
    return std::unexpected("bytecode not found: " + script_path);
  }

  if (luaL_loadbuffer(state_, bc.data(), bc.size(), script_path.c_str()) != LUA_OK) {
    std::string err = lua_tostring(state_, -1);
    lua_pop(state_, 1);
    lua_settop(state_, stack_top);
    return std::unexpected(err);
  }

  const int chunk_index = lua_gettop(state_);

  // Create a dedicated environment for this script: setmetatable(env, {__index = _G})
  lua_newtable(state_);
  int env_index = lua_gettop(state_);
  lua_newtable(state_);
  pushGlobalTable(state_);
  lua_setfield(state_, -2, "__index");
  lua_setmetatable(state_, env_index);

  // Bind env to chunk's environment
  if (!setChunkEnv(state_, chunk_index, env_index)) {
    lua_pop(state_, 2); // env + chunk
    lua_settop(state_, stack_top);
    return std::unexpected("failed to set environment for chunk");
  }

  lua_pushvalue(state_, env_index);
  env_ref_ = luaL_ref(state_, LUA_REGISTRYINDEX); // pops env

  // Remove the env table from the stack, keep only the chunk for execution
  lua_remove(state_, env_index);

  // Run the chunk
  if (lua_pcall(state_, 0, 0, 0) != LUA_OK) {
    std::string err = lua_tostring(state_, -1);
    lua_pop(state_, 1);
    luaL_unref(state_, LUA_REGISTRYINDEX, env_ref_);
    env_ref_ = LUA_NOREF;
    lua_settop(state_, stack_top);
    return std::unexpected(err);
  }

  lua_settop(state_, stack_top);
  return std::expected<bool, std::string>(true);
}

std::expected<bool, std::string> LuaScript::run(Transaction* t, const Common::Variant& operand) {
  if (!state_ || env_ref_ == LUA_NOREF) {
    return std::unexpected("lua state not initialized");
  }

  int stack_top = lua_gettop(state_);

  lua_rawgeti(state_, LUA_REGISTRYINDEX, env_ref_);
  if (!lua_istable(state_, -1)) {
    lua_settop(state_, stack_top);
    return std::unexpected("lua env is broken");
  }

  lua_getfield(state_, -1, "main");
  if (!lua_isfunction(state_, -1)) {
    lua_settop(state_, stack_top);
    return std::unexpected("lua entry function isn't main");
  }

  // Remove env table, keep function on stack
  lua_remove(state_, -2);

  if (IS_STRING_VIEW_VARIANT(operand)) {
    std::string_view operand_str = std::get<std::string_view>(operand);
    lua_pushlstring(state_, operand_str.data(), operand_str.size());
  } else if (IS_INT_VARIANT(operand)) {
    int64_t operand_int = std::get<int64_t>(operand);
    lua_pushinteger(state_, operand_int);
  } else {
    lua_pushnil(state_);
  }

  pushTransactionToLua(state_, t);

  if (lua_pcall(state_, 2, 1, 0) != LUA_OK) {
    std::string err = lua_tostring(state_, -1);
    lua_pop(state_, 1);
    lua_settop(state_, stack_top);
    return std::unexpected(err);
  }
  bool res = lua_toboolean(state_, -1);
  lua_pop(state_, 1);

  lua_settop(state_, stack_top);
  return std::expected<bool, std::string>(res);
}

Transaction* LuaScript::checkTransaction(lua_State* L, int idx) {
  void* ud = luaL_checkudata(L, idx, "Wge.Transaction");
  if (!ud) {
    luaL_argerror(L, idx, "Wge.Transaction expected");
    return nullptr;
  }
  Transaction** pt = static_cast<Transaction**>(ud);
  return *pt;
}

void LuaScript::pushVariantToLua(lua_State* L, const Common::Variant& v) {
  if (std::holds_alternative<std::monostate>(v)) {
    lua_pushnil(L);
    return;
  }
  if (std::holds_alternative<int64_t>(v)) {
    lua_pushinteger(L, std::get<int64_t>(v));
    return;
  }
  if (std::holds_alternative<std::string_view>(v)) {
    auto sv = std::get<std::string_view>(v);
    lua_pushlstring(L, sv.data(), sv.size());
    return;
  }
  lua_pushnil(L);
}

int LuaScript::txGetVariable(lua_State* L) {
  Transaction* t = checkTransaction(L, 1);
  if (!t) {
    lua_pushnil(L);
    return 1;
  }

  if (lua_type(L, 2) != LUA_TSTRING) {
    lua_pushnil(L);
    return 1;
  }

  size_t ns_len;
  const char* ns = lua_tolstring(L, 2, &ns_len);

  if (lua_type(L, 3) == LUA_TNUMBER) {
    lua_Integer idx = lua_tointeger(L, 3);
    const Common::Variant& v = t->getVariable(ns, static_cast<size_t>(idx));
    pushVariantToLua(L, v);
    return 1;
  } else if (lua_type(L, 3) == LUA_TSTRING) {
    size_t len;
    const char* name = lua_tolstring(L, 3, &len);
    std::string s(name, len);
    const Common::Variant& v = t->getVariable(ns, s);
    pushVariantToLua(L, v);
    return 1;
  }

  lua_pushnil(L);
  return 1;
}

int LuaScript::txSetVariable(lua_State* L) {
  Transaction* t = checkTransaction(L, 1);
  if (!t) {
    lua_pushboolean(L, 0);
    return 1;
  }

  if (lua_type(L, 2) != LUA_TSTRING) {
    lua_pushboolean(L, 0);
    return 1;
  }

  size_t ns_len;
  const char* ns = lua_tolstring(L, 2, &ns_len);

  Common::Variant v;
  int ttype = lua_type(L, 4);
  if (ttype == LUA_TNUMBER) {
    lua_Integer iv = lua_tointeger(L, 4);
    v = static_cast<int64_t>(iv);
  } else if (ttype == LUA_TSTRING) {
    size_t len;
    const char* value = lua_tolstring(L, 4, &len);
    auto sv = t->internString({value, len});
    v = sv;
  } else {
    lua_pushboolean(L, 0);
    return 1;
  }

  if (lua_type(L, 3) == LUA_TNUMBER) {
    lua_Integer idx = lua_tointeger(L, 3);
    t->setVariable(ns, static_cast<size_t>(idx), v);
    lua_pushboolean(L, 1);
    return 1;
  } else if (lua_type(L, 3) == LUA_TSTRING) {
    size_t len;
    const char* name = lua_tolstring(L, 3, &len);
    t->setVariable(ns, {name, len}, v);
    lua_pushboolean(L, 1);
    return 1;
  }

  lua_pushboolean(L, 0);
  return 1;
}

// Metatable registration and push
void LuaScript::pushTransactionToLua(lua_State* state, Transaction* t) {
  // create userdata
  Transaction** ud = static_cast<Transaction**>(lua_newuserdata(state, sizeof(Transaction*)));
  *ud = t;

  // create or reuse metatable
  if (luaL_newmetatable(state, "Wge.Transaction")) {
    // metatable.__index = methods table
    lua_newtable(state);
    lua_pushcfunction(state, txGetVariable);
    lua_setfield(state, -2, "getVariable");
    lua_pushcfunction(state, txSetVariable);
    lua_setfield(state, -2, "setVariable");

    lua_setfield(state, -2, "__index");
  }

  // set metatable for userdata
  lua_setmetatable(state, -2);
}
} // namespace Lua
} // namespace Common
} // namespace Wge