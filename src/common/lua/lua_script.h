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
#pragma once

#include <expected>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <luajit-2.1/lua.hpp>

#include "../variant.h"
namespace Wge {
class Transaction;
namespace Common {
namespace Lua {
class LuaBytecodeCache {
public:
  static LuaBytecodeCache& instance() {
    static LuaBytecodeCache ins;
    return ins;
  }

  bool preload(const std::string& script_path) {
    if (cache_.contains(script_path))
      return true;

    lua_State* L = luaL_newstate();
    luaL_openlibs(L);

    if (luaL_loadfile(L, script_path.c_str()) != LUA_OK) {
      lua_close(L);
      return false;
    }

    std::vector<char> bytecode;
    if (lua_dump(L, bytecodeWriter, &bytecode) != 0) {
      lua_close(L);
      return false;
    }

    cache_[script_path] = std::move(bytecode);
    lua_close(L);
    return true;
  }

  std::vector<char> get(const std::string& script_path) const {
    auto it = cache_.find(script_path);
    return (it != cache_.end()) ? it->second : std::vector<char>{};
  }

private:
  static int bytecodeWriter(lua_State* L, const void* p, size_t sz, void* ud) {
    std::vector<char>* buf = static_cast<std::vector<char>*>(ud);
    const char* data = static_cast<const char*>(p);
    buf->insert(buf->end(), data, data + sz);
    return 0;
  }

  std::unordered_map<std::string, std::vector<char>> cache_;
};

class LuaScript {
public:
  explicit LuaScript(lua_State* shared_state = nullptr) : state_(shared_state) {}
  ~LuaScript();

  std::expected<bool, std::string> load(const std::string& script_path);
  std::expected<bool, std::string> run(Transaction* t, const Common::Variant& operand);

public:
  static void pushTransactionToLua(lua_State* state, Transaction* t);
  static void pushVariantToLua(lua_State* L, const Common::Variant& v);
  static int txGetVariable(lua_State* L);
  static int txSetVariable(lua_State* L);
  static Transaction* checkTransaction(lua_State* L, int idx);

private:
  lua_State* state_{nullptr};
  int env_ref_{LUA_NOREF};
};

using LuaPtr = std::unique_ptr<LuaScript>;
} // namespace Lua
} // namespace Common
} // namespace Wge
