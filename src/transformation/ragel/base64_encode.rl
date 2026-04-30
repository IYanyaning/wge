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
#include <string_view>

#include "src/transformation/stream_util.h"

// clang-format off
%%{
  machine base64_encode;

  action encode_chunk {
    const unsigned char* s = (const unsigned char*)ts;
    unsigned char b0 = s[0], b1 = s[1], b2 = s[2];
    *r++ = alphabet[(b0 >> 2) & 0x3F];
    *r++ = alphabet[((b0 << 4) | (b1 >> 4)) & 0x3F];
    *r++ = alphabet[((b1 << 2) | (b2 >> 6)) & 0x3F];
    *r++ = alphabet[b2 & 0x3F];
  }

  action encode_remaining {
    const unsigned char* s = (const unsigned char*)ts;
    size_t remaining = te - ts;
    if (remaining == 1) {
      unsigned char b0 = s[0];
      *r++ = alphabet[(b0 >> 2) & 0x3F];
      *r++ = alphabet[(b0 << 4) & 0x3F];
      *r++ = '=';
      *r++ = '=';
    } else if (remaining == 2) {
      unsigned char b0 = s[0], b1 = s[1];
      *r++ = alphabet[(b0 >> 2) & 0x3F];
      *r++ = alphabet[((b0 << 4) | (b1 >> 4)) & 0x3F];
      *r++ = alphabet[(b1 << 2) & 0x3F];
      *r++ = '=';
    }
  }

  main := |*
    any{3} => encode_chunk;
    any{1,2} => encode_remaining;
  *|;
}%%

%% write data;
// clang-format on

static bool base64Encode(std::string_view input, std::string& result) {
  result.clear();

  if (input.empty()) {
    return false;
  }
  const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  result.resize(((input.size() + 2) / 3) * 4);
  char* r = (char*)result.data();

  const char* p = input.data();
  const char* pe = p + input.size();
  const char* eof = pe;
  const char *ts, *te;
  int cs, act;

  // clang-format off
  %% write init;
  %% write exec;
  // clang-format on

  result.resize(r - result.data());
  return true;
}

// clang-format off
%%{
  machine base64_encode_stream;

  action encode_chunk {
    const unsigned char* s = (const unsigned char*)ts;
    unsigned char b0 = s[0], b1 = s[1], b2 = s[2];
    result += alphabet[(b0 >> 2) & 0x3F];
    result += alphabet[((b0 << 4) | (b1 >> 4)) & 0x3F];
    result += alphabet[((b1 << 2) | (b2 >> 6)) & 0x3F];
    result += alphabet[b2 & 0x3F];
  }

  action encode_remaining {
    const unsigned char* s = (const unsigned char*)ts;
    size_t remaining = te - ts;
    if (remaining == 1) {
      unsigned char b0 = s[0];
      result += alphabet[(b0 >> 2) & 0x3F];
      result += alphabet[(b0 << 4) & 0x3F];
      result += '=';
      result += '=';
    } else if (remaining == 2) {
      unsigned char b0 = s[0], b1 = s[1];
      result += alphabet[(b0 >> 2) & 0x3F];
      result += alphabet[((b0 << 4) | (b1 >> 4)) & 0x3F];
      result += alphabet[(b1 << 2) & 0x3F];
      result += '=';
    }
  }

  main := |*
    any{3} => encode_chunk;
    any{1,2} => encode_remaining;
  *|;
}%%

%% write data;
// clang-format on

static std::unique_ptr<Wge::Transformation::StreamState,
                       std::function<void(Wge::Transformation::StreamState*)>>
base64EncodeNewStream() {
  return std::make_unique<Wge::Transformation::StreamState>();
}

static Wge::Transformation::StreamResult base64EncodeStream(std::string_view input,
                                                            std::string& result,
                                                            Wge::Transformation::StreamState& state,
                                                            bool end_stream) {
  using namespace Wge::Transformation;
  const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  // The stream is not valid
  if (state.state_.test(static_cast<size_t>(StreamState::State::INVALID)))
    [[unlikely]] { return StreamResult::INVALID_INPUT; }

  // The stream is complete, no more data to process
  if (state.state_.test(static_cast<size_t>(StreamState::State::COMPLETE)))
    [[unlikely]] { return StreamResult::SUCCESS; }

  // In the stream mode, we can't operate the raw pointer of the result directly simular to the
  // block mode since we can't guarantee reserve enough space in the result string. Instead, we
  // will use the string's append method to add the transformed data. Although this is less
  // efficient than using a raw pointer, it is necessary to ensure the safety of the stream
  // processing.
  result.reserve(result.size() + ((input.size() + 2) / 3) * 4);

  const char* p = input.data();
  const char* ps = p;
  const char* pe = p + input.size();
  const char* eof = end_stream ? pe : nullptr;
  const char *ts, *te;
  int cs, act;

  // clang-format off
  %% write init;
  recoverStreamState(state, input, ps, pe, eof, p, cs, act, ts, te, end_stream);
  %% write exec;
  // clang-format on

  return saveStreamState(state, cs, act, ps, pe, ts, te, end_stream);
}