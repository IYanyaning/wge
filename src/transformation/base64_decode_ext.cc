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
#include "base64_decode_ext.h"

namespace Wge {
namespace Transformation {
bool Base64DecodeExt::evaluate(std::string_view data, std::string& result) const {
  return base64_decode_.evaluate(data, result);
}

std::unique_ptr<StreamState, std::function<void(StreamState*)>> Base64DecodeExt::newStream() const {
  return base64_decode_.newStream();
}

StreamResult Base64DecodeExt::evaluateStream(std::string_view input, std::string& output,
                                             StreamState& state, bool end_stream) const {
  return base64_decode_.evaluateStream(input, output, state, end_stream);
}
} // namespace Transformation
} // namespace Wge
