// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is separate from base/win/win_util.h to avoid pulling windows.h
// into too many translation units.

#ifndef WINLIB_WINBASE_WIN_WIN_CLIENT_METRICS_H_
#define WINLIB_WINBASE_WIN_WIN_CLIENT_METRICS_H_

#include <windows.h>

namespace winbase {
namespace win {

// This is the same as NONCLIENTMETRICS except that the
// unused member |iPaddedBorderWidth| has been removed.
struct NONCLIENTMETRICS_XP {
  UINT cbSize;
  int iBorderWidth;
  int iScrollWidth;
  int iScrollHeight;
  int iCaptionWidth;
  int iCaptionHeight;
  LOGFONTW lfCaptionFont;
  int iSmCaptionWidth;
  int iSmCaptionHeight;
  LOGFONTW lfSmCaptionFont;
  int iMenuWidth;
  int iMenuHeight;
  LOGFONTW lfMenuFont;
  LOGFONTW lfStatusFont;
  LOGFONTW lfMessageFont;
};

WINBASE_EXPORT void GetNonClientMetrics(NONCLIENTMETRICS_XP* metrics);

}  // namespace win
}  // namespace winbase

#endif  // WINLIB_WINBASE_WIN_WIN_CLIENT_METRICS_H_