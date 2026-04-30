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
#include <unordered_map>

#include "lua_script.h"
namespace Wge {
namespace Common {
namespace Lua {
class LuaRuntime {
public:
  struct ThreadLocalState {
    lua_State* state = nullptr;

    ThreadLocalState() {
      state = luaL_newstate();
      luaL_openlibs(state);
    }

    ~ThreadLocalState() {
      if (state)
        lua_close(state);
    }
  };

  static LuaRuntime* getInstance() {
    static LuaRuntime instance;
    return &instance;
  }

  std::expected<LuaScript*, std::string> getLua(const std::string& script_path) {
    auto [it, inserted] = tls_luas_.try_emplace(script_path, nullptr);

    if (!inserted) {
      return it->second.get();
    }

    auto lua = std::make_unique<LuaScript>(tls_state_.state);
    if (auto err = lua->load(script_path); !err.has_value()) {
      tls_luas_.erase(it);
      return std::unexpected(std::move(err.error()));
    }

    it->second = std::move(lua);
    return it->second.get();
  }

private:
  LuaRuntime() = default;
  LuaRuntime(const LuaRuntime&) = delete;
  LuaRuntime& operator=(const LuaRuntime&) = delete;

private:
  static thread_local std::unordered_map<std::string, LuaPtr> tls_luas_;
  static thread_local ThreadLocalState tls_state_;
};
} // namespace Lua
} // namespace Common
} // namespace Wge