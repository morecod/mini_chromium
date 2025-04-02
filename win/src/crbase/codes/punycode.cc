// Copyright 2016 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "crbase/codes/punycode.h"

#include <vector>

#include "crbase/logging.h"
#include "crbase/strings/string_util.h"

namespace {

const int32_t maxInt = 0x7FFFFFFF;
const int32_t base = 36;
const int32_t damp = 700;
const int32_t initialBias = 72;
const int32_t initialN = 128;
const int32_t skew = 38;
const int32_t tmax = 26;
const int32_t tmin = 1;

// adapt is the bias adaptation function specified in section 6.1.
int32_t adapt(int32_t delta, int32_t numPoints, bool firstTime) {
  if (firstTime) delta /= damp;
  else delta /= 2;

  delta += delta / numPoints;

  uint32_t k = 0;
  while (delta > ((base - tmin)*tmax) / 2) {
    delta /= base - tmin;
    k += base;
  }

  return k + (base - tmin + 1)*delta / (delta + skew);
}

// returns 0 on fails.
bool encodeDigit(int32_t digit, int32_t& out) {
  if (digit >= 0 && digit < 26) {
    out = digit + 'a';
    return true;
  }

  if (digit >= 26 && digit < 36) {
    out = digit + ('0' - 26);
    return true;
  }

  CR_NOTREACHED() << "idna: internal error in punycode encoding";
  return false;
}

bool decodeDigit(uint8_t x, int32_t& out) {
  if (x >= '0' && x <= '9') {
    out = x - ('0' - 26);
    return true;
  }

  if (x >= 'A' && x <= 'Z') {
    out = x - 'A';
    return true;
  }

  if (x >= 'a' && x <= 'z') {
    out = x - 'a';
    return true;
  }

  out = 0;
  return false;
}

// madd computes a + (b * c), returns $false on detecting overflow.
bool madd(int32_t a, int32_t b, int32_t c, int32_t& next) {
  int64_t p = static_cast<int64_t>(b) * static_cast<int64_t>(c);
  if (p > maxInt - static_cast<int64_t>(a)) {
    next = 0;
    return false;
  }

  next = a + static_cast<int32_t>(p);
  return true;
}

// encode encodes a string as specified in section 6.3 and prepends prefix to
// the result.
//
// The "while h < length(input)" line in the specification becomes "for
// remaining != 0" in the Go code, because len(s) in Go is in bytes, not runes.
template<typename Span, typename STR>
bool encode(Span codepoints, STR& out) {
  CR_DCHECK(sizeof(codepoints[0]) >= 2u);
  out.clear();

  int32_t delta = 0, n = initialN, bias = initialBias;
  int32_t b = 0, remaining = 0;
  for (int32_t cp : codepoints) {
    if (cp < 0x80) {
      b++;
      out.push_back(static_cast<char>(cp));
    } else {
      remaining ++;
    }
  }

  int32_t h = b;
  if (b > 0)
    out.push_back('-');

  while (remaining != 0) {
    int32_t m = maxInt;
    for (int32_t cp : codepoints) {
      if (m > cp && cp >= n)
        m = cp;
    }

    if (!madd(delta, m - n, h + 1, delta))
      return false;  // overflow

    n = m;
    for (int32_t cp : codepoints) {
      if (cp < n) {
        delta++;
        if (delta < 0)
          return false;  // overflow
        continue;
      }

      if (cp > n)
        continue;

      int32_t digit;

      int32_t q = delta;
      for (int32_t k = base; ; k += base) {
        int32_t t = k - bias;
        if (k <= bias) {
          t = tmin;
        } else if (k >= bias + tmax) {
          t = tmax;
        }

        if (q < t) break;

        if (!encodeDigit(t + (q - t) % (base - t), digit))
          return false; //
        out.push_back(static_cast<char>(digit));

        q = (q - t) / (base - t);
      }

      if (!encodeDigit(q, digit))
        return false; //
      out.push_back(static_cast<char>(digit));
      bias = adapt(delta, h + 1, h == b);
      delta = 0;
      h++;
      remaining--;
    }
    delta++;
    n++;
  }
  return true;
}

// decode decodes a string as specified in section 6.2.
bool decode(cr::StringPiece input, cr::string16& out) {
  CR_DCHECK(IsStringASCII(input));

  if (input.empty())
    return false;

  size_t de_pos = input.find_last_of('-');
  int32_t pos = cr::StringPiece::npos == de_pos
                    ? -1 : static_cast<int32_t>(de_pos);
  pos++;

  if (pos == 1)
    return false;

  if (pos == static_cast<int32_t>(input.length())) {
    size_t len = input.length() - 1;

    // copy ascii chars to uni chars.
    cr::WriteInto(&out, len + 1);
    for (size_t i = 0; i < len; i++)
      out[i] = input[i];

    return true;
  }

  out.reserve(input.length());

  if (pos != 0) {
    for (auto cp : input.substr(0, pos - 1))
      out.push_back(cp);
  }

  int32_t i = 0, n = initialN, bias = initialBias;
  while (pos < static_cast<int32_t>(input.length())) {
    int32_t oldI = i, w = 1;
    for (int32_t k = base;; k += base) {
      if (pos == static_cast<int32_t>(input.length()))
        return false;

      int32_t digit;
      if (!decodeDigit(input[pos], digit))
        return false;

      pos++;

      if (!madd(i, digit, w, i))
        return false;  // overflow

      int32_t t = k - bias;
      if (k <= bias) {
        t = tmin;
      } else if (k >= bias + tmax) {
        t = tmax;
      }

      if (digit < t) break;

      if (!madd(0, w, base - t, w))
        return false; // overflow
    }

    if (out.length() >= 1024)
      return false;

    int32_t x = static_cast<int32_t>(out.length() + 1);
    bias = adapt(i - oldI, x, oldI == 0);
    n += i / x;
    i %= x;
    if (n < 0 || n > 0x0010FFFF /*utf8.MaxRune*/)
      return false;

    out.insert(out.begin() + i, static_cast<cr::char16>(n));
    i++;
  }

  return true;
}

}  // namespace

namespace cr {

bool EncodePunycode(const StringPiece16& input, std::string &out) {
  return encode(input, out);
}

bool EncodePunycode(const StringPiece16& input, string16 &out) {
  return encode(input, out);
}

bool DecodePunycode(StringPiece input, string16& output) {
  return decode(input, output);
}

}  // namespace cr
