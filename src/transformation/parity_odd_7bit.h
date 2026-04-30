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

#include <string>

#include "transform_base.h"

namespace Wge {
namespace Transformation {
class ParityOdd7Bit final : public TransformBase {
  DECLARE_TRANSFORM_NAME(parityOdd7Bit);

public:
  bool evaluate(std::string_view data, std::string& result) const override {
    if (data.empty()) {
      result.clear();
      return false;
    }

    result.resize(data.size());

    const unsigned char* in = reinterpret_cast<const unsigned char*>(data.data());
    unsigned char* out = reinterpret_cast<unsigned char*>(result.data());

    for (size_t i = 0; i < data.size(); ++i) {
      unsigned char c = in[i];
      unsigned char x = c & 0x7F;

      x ^= x >> 4;
      x &= 0x0F;
      unsigned char parity = (0x6996 >> x) & 1;

      out[i] = (c & 0x7F) | ((parity ^ 1) << 7);
    }

    return true;
  }

  std::unique_ptr<StreamState, std::function<void(StreamState*)>> newStream() const {
    return std::make_unique<Wge::Transformation::StreamState>();
  }

  StreamResult evaluateStream(std::string_view input, std::string& output, StreamState& /*state*/,
                              bool end_stream) const {

    if (input.empty()) {
      if (end_stream) {
        return StreamResult::SUCCESS;
      }
      return StreamResult::NEED_MORE_DATA;
    }

    size_t old_size = output.size();
    output.resize(old_size + input.size());

    const unsigned char* in = reinterpret_cast<const unsigned char*>(input.data());
    unsigned char* out = reinterpret_cast<unsigned char*>(output.data()) + old_size;

    for (size_t i = 0; i < input.size(); ++i) {
      unsigned char c = in[i];
      unsigned char x = c & 0x7F;

      x ^= x >> 4;
      x &= 0x0F;
      unsigned char parity = (0x6996 >> x) & 1;

      out[i] = (c & 0x7F) | ((parity ^ 1) << 7);
    }

    return end_stream ? StreamResult::SUCCESS : StreamResult::NEED_MORE_DATA;
  }
};
} // namespace Transformation
} // namespace Wge
