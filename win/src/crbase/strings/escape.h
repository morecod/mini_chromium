// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_STRINGS_ESCAPE_H_
#define MINI_CHROMIUM_SRC_CRBASE_STRINGS_ESCAPE_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "crbase/strings/string16.h"
#include "crbase/strings/utf_offset_string_conversions.h"

namespace crbase {

// Escaping --------------------------------------------------------------------

// Escapes characters in text suitable for use as a query parameter value.
// We %XX everything except alphanumerics and -_.!~*'()
// Spaces change to "+" unless you pass usePlus=false.
// This is basically the same as encodeURIComponent in javascript.
CRBASE_EXPORT std::string EscapeQueryParamValue(StringPiece text,
                                                bool use_plus);

// Escapes a partial or complete file/pathname.  This includes:
// non-printable, non-7bit, and (including space)  "#%:<>?[\]^`{|}
// For the base::string16 version, we attempt a conversion to |codepage| before
// encoding the string.  If this conversion fails, we return false.
CRBASE_EXPORT std::string EscapePath(StringPiece path);

// Escapes application/x-www-form-urlencoded content.  This includes:
// non-printable, non-7bit, and (including space)  ?>=<;+'&%$#"![\]^`{|}
// Space is escaped as + (if use_plus is true) and other special characters
// as %XX (hex).
CRBASE_EXPORT std::string EscapeUrlEncodedData(StringPiece path,
                                               bool use_plus);

// Escapes all non-ASCII input.
CRBASE_EXPORT std::string EscapeNonASCII(StringPiece input);

// Escapes characters in text suitable for use as an external protocol handler
// command.
// We %XX everything except alphanumerics and -_.!~*'() and the restricted
// chracters (;/?:@&=+$,#[]) and a valid percent escape sequence (%XX).
CRBASE_EXPORT std::string EscapeExternalHandlerValue(StringPiece text);

// Appends the given character to the output string, escaping the character if
// the character would be interpretted as an HTML delimiter.
CRBASE_EXPORT void AppendEscapedCharForHTML(char c, std::string* output);

// Escapes chars that might cause this text to be interpretted as HTML tags.
CRBASE_EXPORT std::string EscapeForHTML(StringPiece text);
CRBASE_EXPORT string16 EscapeForHTML(StringPiece16 text);

// Unescaping ------------------------------------------------------------------

class UnescapeRule {
 public:
  // A combination of the following flags that is passed to the unescaping
  // functions.
  typedef uint32_t Type;

  enum {
    // Don't unescape anything at all.
    NONE = 0,

    // Don't unescape anything special, but all normal unescaping will happen.
    // This is a placeholder and can't be combined with other flags (since it's
    // just the absence of them). All other unescape rules imply "normal" in
    // addition to their special meaning. Things like escaped letters, digits,
    // and most symbols will get unescaped with this mode.
    NORMAL = 1,

    // Convert %20 to spaces. In some places where we're showing URLs, we may
    // want this. In places where the URL may be copied and pasted out, then
    // you wouldn't want this since it might not be interpreted in one piece
    // by other applications.
    SPACES = 2,

    // Unescapes various characters that will change the meaning of URLs,
    // including '%', '+', '&', '/', '#'. If we unescaped these characters, the
    // resulting URL won't be the same as the source one. This flag is used when
    // generating final output like filenames for URLs where we won't be
    // interpreting as a URL and want to do as much unescaping as possible.
    URL_SPECIAL_CHARS = 4,

    // Unescapes characters that can be used in spoofing attempts (such as LOCK)
    // and control characters (such as BiDi control characters and %01).  This
    // INCLUDES NULLs.  This is used for rare cases such as data: URL decoding
    // where the result is binary data.
    //
    // DO NOT use SPOOFING_AND_CONTROL_CHARS if the URL is going to be displayed
    // in the UI for security reasons.
    SPOOFING_AND_CONTROL_CHARS = 8,

    // URL queries use "+" for space. This flag controls that replacement.
    REPLACE_PLUS_WITH_SPACE = 16,
  };
};

// Unescapes |escaped_text| and returns the result.
// Unescaping consists of looking for the exact pattern "%XX", where each X is
// a hex digit, and converting to the character with the numerical value of
// those digits. Thus "i%20=%203%3b" unescapes to "i = 3;".
//
// Watch out: this doesn't necessarily result in the correct final result,
// because the encoding may be unknown. For example, the input might be ASCII,
// which, after unescaping, is supposed to be interpreted as UTF-8, and then
// converted into full UTF-16 chars. This function won't tell you if any
// conversions need to take place, it only unescapes.
CRBASE_EXPORT std::string UnescapeURLComponent(StringPiece escaped_text,
                                               UnescapeRule::Type rules);
CRBASE_EXPORT crbase::string16 UnescapeURLComponent(
    const StringPiece16 escaped_text,
    UnescapeRule::Type rules);

// Unescapes the given substring as a URL, and then tries to interpret the
// result as being encoded as UTF-8. If the result is convertable into UTF-8, it
// will be returned as converted. If it is not, the original escaped string will
// be converted into a base::string16 and returned.  |adjustments| provides
// information on how the original string was adjusted to get the string
// returned.
CRBASE_EXPORT string16 UnescapeAndDecodeUTF8URLComponent(
    StringPiece text,
    UnescapeRule::Type rules);
CRBASE_EXPORT string16 UnescapeAndDecodeUTF8URLComponentWithAdjustments(
    StringPiece text,
    UnescapeRule::Type rules,
    OffsetAdjuster::Adjustments* adjustments);

// Unescapes the following ampersand character codes from |text|:
// &lt; &gt; &amp; &quot; &#39;
CRBASE_EXPORT string16 UnescapeForHTML(StringPiece16 text);

}  // namespace crbase

#endif  // MINI_CHROMIUM_SRC_CRBASE_STRINGS_ESCAPE_H_