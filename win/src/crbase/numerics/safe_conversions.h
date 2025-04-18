// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_NUMERICS_SAFE_CONVERSIONS_H_
#define MINI_CHROMIUM_SRC_CRBASE_NUMERICS_SAFE_CONVERSIONS_H_

#include <stddef.h>

#include <limits>
#include <type_traits>

#include "crbase/logging.h"
#include "crbase/numerics/safe_conversions_impl.h"

namespace cr {

// Convenience function that returns true if the supplied value is in range
// for the destination type.
template <typename Dst, typename Src>
inline bool IsValueInRangeForNumericType(Src value) {
  return internal::DstRangeRelationToSrcRange<Dst>(value) ==
         internal::RANGE_VALID;
}

// Convenience function for determining if a numeric value is negative without
// throwing compiler warnings on: unsigned(value) < 0.
template <typename T>
typename std::enable_if<std::numeric_limits<T>::is_signed, bool>::type
IsValueNegative(T value) {
  static_assert(std::numeric_limits<T>::is_specialized,
                "Argument must be numeric.");
  return value < 0;
}

template <typename T>
typename std::enable_if<!std::numeric_limits<T>::is_signed, bool>::type
    IsValueNegative(T) {
  static_assert(std::numeric_limits<T>::is_specialized,
                "Argument must be numeric.");
  return false;
}

// checked_cast<> is analogous to static_cast<> for numeric types,
// except that it CHECKs that the specified numeric conversion will not
// overflow or underflow. NaN source will always trigger a CHECK.
template <typename Dst, typename Src>
inline Dst checked_cast(Src value) {
  CR_CHECK(IsValueInRangeForNumericType<Dst>(value));
  return static_cast<Dst>(value);
}

// HandleNaN will cause this class to CHECK(false).
struct SaturatedCastNaNBehaviorCheck {
  template <typename T>
  static T HandleNaN() {
    CR_CHECK(false);
    return T();
  }
};

// HandleNaN will return 0 in this case.
struct SaturatedCastNaNBehaviorReturnZero {
  template <typename T>
  static T HandleNaN() {
    return T();
  }
};

// saturated_cast<> is analogous to static_cast<> for numeric types, except
// that the specified numeric conversion will saturate rather than overflow or
// underflow. NaN assignment to an integral will defer the behavior to a
// specified class. By default, it will return 0.
template <typename Dst,
          class NaNHandler = SaturatedCastNaNBehaviorReturnZero,
          typename Src>
inline Dst saturated_cast(Src value) {
  // Optimization for floating point values, which already saturate.
  if (std::numeric_limits<Dst>::is_iec559)
    return static_cast<Dst>(value);

  switch (internal::DstRangeRelationToSrcRange<Dst>(value)) {
    case internal::RANGE_VALID:
      return static_cast<Dst>(value);

    case internal::RANGE_UNDERFLOW:
      return std::numeric_limits<Dst>::min();

    case internal::RANGE_OVERFLOW:
      return std::numeric_limits<Dst>::max();

    // Should fail only on attempting to assign NaN to a saturated integer.
    case internal::RANGE_INVALID:
      return NaNHandler::template HandleNaN<Dst>();
  }

  CR_NOTREACHED();
  return static_cast<Dst>(value);
}

// strict_cast<> is analogous to static_cast<> for numeric types, except that
// it will cause a compile failure if the destination type is not large enough
// to contain any value in the source type. It performs no runtime checking.
template <typename Dst, typename Src>
inline Dst strict_cast(Src value) {
  static_assert(std::numeric_limits<Src>::is_specialized,
                "Argument must be numeric.");
  static_assert(std::numeric_limits<Dst>::is_specialized,
                "Result must be numeric.");
  static_assert((internal::StaticDstRangeRelationToSrcRange<Dst, Src>::value ==
                 internal::NUMERIC_RANGE_CONTAINED),
                "The numeric conversion is out of range for this type. You "
                "should probably use one of the following conversion "
                "mechanisms on the value you want to pass:\n"
                "- cr::checked_cast\n"
                "- cr::saturated_cast\n"
                "- cr::CheckedNumeric");

  return static_cast<Dst>(value);
}

// StrictNumeric implements compile time range checking between numeric types by
// wrapping assignment operations in a strict_cast. This class is intended to be
// used for function arguments and return types, to ensure the destination type
// can always contain the source type. This is essentially the same as enforcing
// -Wconversion in gcc and C4302 warnings on MSVC, but it can be applied
// incrementally at API boundaries, making it easier to convert code so that it
// compiles cleanly with truncation warnings enabled.
// This template should introduce no runtime overhead, but it also provides no
// runtime checking of any of the associated mathematical operations. Use
// CheckedNumeric for runtime range checks of tha actual value being assigned.
template <typename T>
class StrictNumeric {
 public:
  typedef T type;

  StrictNumeric() : value_(0) {}

  // Copy constructor.
  template <typename Src>
  StrictNumeric(const StrictNumeric<Src>& rhs)
      : value_(strict_cast<T>(rhs.value_)) {}

  // This is not an explicit constructor because we implicitly upgrade regular
  // numerics to StrictNumerics to make them easier to use.
  template <typename Src>
  StrictNumeric(Src value)
      : value_(strict_cast<T>(value)) {}

  // The numeric cast operator basically handles all the magic.
  template <typename Dst>
  operator Dst() const {
    return strict_cast<Dst>(value_);
  }

 private:
  T value_;
};

// Explicitly make a shorter size_t typedef for convenience.
typedef StrictNumeric<size_t> SizeT;

}  // namespace cr

#endif  // MINI_CHROMIUM_SRC_CRBASE_NUMERICS_SAFE_CONVERSIONS_H_