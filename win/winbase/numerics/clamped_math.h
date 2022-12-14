// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINLIB_WINBASE_NUMERICS_CLAMPED_MATH_H_
#define WINLIB_WINBASE_NUMERICS_CLAMPED_MATH_H_

#include <stddef.h>

#include <limits>
#include <type_traits>

#include "winbase\numerics\clamped_math_impl.h"

namespace winbase {
namespace internal {

template <typename T>
class ClampedNumeric {
  static_assert(std::is_arithmetic<T>::value,
                "ClampedNumeric<T>: T must be a numeric type.");

 public:
  using type = T;

  constexpr ClampedNumeric() : value_(0) {}

  // Copy constructor.
  template <typename Src>
  constexpr ClampedNumeric(const ClampedNumeric<Src>& rhs)
      : value_(saturated_cast<T>(rhs.value_)) {}

  template <typename Src>
  friend class ClampedNumeric;

  // This is not an explicit constructor because we implicitly upgrade regular
  // numerics to ClampedNumerics to make them easier to use.
  template <typename Src>
  constexpr ClampedNumeric(Src value)  // NOLINT(runtime/explicit)
      : value_(saturated_cast<T>(value)) {
    static_assert(std::is_arithmetic<Src>::value, "Argument must be numeric.");
  }

  // This is not an explicit constructor because we want a seamless conversion
  // from StrictNumeric types.
  template <typename Src>
  constexpr ClampedNumeric(
      StrictNumeric<Src> value)  // NOLINT(runtime/explicit)
      : value_(saturated_cast<T>(static_cast<Src>(value))) {}

  // Returns a ClampedNumeric of the specified type, cast from the current
  // ClampedNumeric, and saturated to the destination type.
  template <typename Dst>
  constexpr ClampedNumeric<typename UnderlyingType<Dst>::type> Cast() const {
    return *this;
  }

  // Prototypes for the supported arithmetic operator overloads.
  template <typename Src>
  constexpr ClampedNumeric& operator+=(const Src rhs);
  template <typename Src>
  constexpr ClampedNumeric& operator-=(const Src rhs);
  template <typename Src>
  constexpr ClampedNumeric& operator*=(const Src rhs);
  template <typename Src>
  constexpr ClampedNumeric& operator/=(const Src rhs);
  template <typename Src>
  constexpr ClampedNumeric& operator%=(const Src rhs);
  template <typename Src>
  constexpr ClampedNumeric& operator<<=(const Src rhs);
  template <typename Src>
  constexpr ClampedNumeric& operator>>=(const Src rhs);
  template <typename Src>
  constexpr ClampedNumeric& operator&=(const Src rhs);
  template <typename Src>
  constexpr ClampedNumeric& operator|=(const Src rhs);
  template <typename Src>
  constexpr ClampedNumeric& operator^=(const Src rhs);

  constexpr ClampedNumeric operator-() const {
    // The negation of two's complement int min is int min, so that's the
    // only overflow case where we will saturate.
    return ClampedNumeric<T>(SaturatedNegWrapper(value_));
  }

  constexpr ClampedNumeric operator~() const {
    return ClampedNumeric<decltype(InvertWrapper(T()))>(InvertWrapper(value_));
  }

  constexpr ClampedNumeric Abs() const {
    // The negation of two's complement int min is int min, so that's the
    // only overflow case where we will saturate.
    return ClampedNumeric<T>(SaturatedAbsWrapper(value_));
  }

  template <typename U>
  constexpr ClampedNumeric<typename MathWrapper<ClampedMaxOp, T, U>::type> Max(
      const U rhs) const {
    using result_type = typename MathWrapper<ClampedMaxOp, T, U>::type;
    return ClampedNumeric<result_type>(
        ClampedMaxOp<T, U>::Do(value_, Wrapper<U>::value(rhs)));
  }

  template <typename U>
  constexpr ClampedNumeric<typename MathWrapper<ClampedMinOp, T, U>::type> Min(
      const U rhs) const {
    using result_type = typename MathWrapper<ClampedMinOp, T, U>::type;
    return ClampedNumeric<result_type>(
        ClampedMinOp<T, U>::Do(value_, Wrapper<U>::value(rhs)));
  }

  // This function is available only for integral types. It returns an unsigned
  // integer of the same width as the source type, containing the absolute value
  // of the source, and properly handling signed min.
  constexpr ClampedNumeric<typename UnsignedOrFloatForSize<T>::type>
  UnsignedAbs() const {
    return ClampedNumeric<typename UnsignedOrFloatForSize<T>::type>(
        SafeUnsignedAbs(value_));
  }

  constexpr ClampedNumeric& operator++() {
    *this += 1;
    return *this;
  }

  constexpr ClampedNumeric operator++(int) {
    ClampedNumeric value = *this;
    *this += 1;
    return value;
  }

  constexpr ClampedNumeric& operator--() {
    *this -= 1;
    return *this;
  }

  constexpr ClampedNumeric operator--(int) {
    ClampedNumeric value = *this;
    *this -= 1;
    return value;
  }

  // These perform the actual math operations on the ClampedNumerics.
  // Binary arithmetic operations.
  template <template <typename, typename, typename> class M,
            typename L,
            typename R>
  static constexpr ClampedNumeric MathOp(const L lhs, const R rhs) {
    using Math = typename MathWrapper<M, L, R>::math;
    return ClampedNumeric<T>(
        Math::template Do<T>(Wrapper<L>::value(lhs), Wrapper<R>::value(rhs)));
  }

  // Assignment arithmetic operations.
  template <template <typename, typename, typename> class M, typename R>
  constexpr inline ClampedNumeric& MathOp(const R rhs) {
    using Math = typename MathWrapper<M, T, R>::math;
    *this =
        ClampedNumeric<T>(Math::template Do<T>(value_, Wrapper<R>::value(rhs)));
    return *this;
  }

  template <typename Dst>
  constexpr operator Dst() const {
    return saturated_cast<typename ArithmeticOrUnderlyingEnum<Dst>::type>(
        value_);
  }

  // This method extracts the raw integer value without saturating it to the
  // destination type as the conversion operator does. This is useful when
  // e.g. assigning to an auto type or passing as a deduced template parameter.
  constexpr T RawValue() const { return value_; }

 private:
  T value_;

  // These wrappers allow us to handle state the same way for both
  // ClampedNumeric and POD arithmetic types.
  template <typename Src>
  struct Wrapper {
    static constexpr Src value(Src value) {
      return static_cast<typename UnderlyingType<Src>::type>(value);
    }
  };
};

// Convience wrapper to return a new ClampedNumeric from the provided arithmetic
// or ClampedNumericType.
template <typename T>
constexpr ClampedNumeric<typename UnderlyingType<T>::type> MakeClampedNum(
    const T value) {
  return value;
}

// Overload the ostream output operator to make logging work nicely.
template <typename T>
std::ostream& operator<<(std::ostream& os, const ClampedNumeric<T>& value) {
  os << static_cast<T>(value);
  return os;
}

// These implement the variadic wrapper for the math operations.
template <template <typename, typename, typename> class M,
          typename L,
          typename R>
constexpr ClampedNumeric<typename MathWrapper<M, L, R>::type> ClampMathOp(
    const L lhs,
    const R rhs) {
  using Math = typename MathWrapper<M, L, R>::math;
  return ClampedNumeric<typename Math::result_type>::template MathOp<M>(lhs,
                                                                        rhs);
}

// General purpose wrapper template for arithmetic operations.
template <template <typename, typename, typename> class M,
          typename L,
          typename R,
          typename... Args>
constexpr ClampedNumeric<typename ResultType<M, L, R, Args...>::type>
ClampMathOp(const L lhs, const R rhs, const Args... args) {
  return ClampMathOp<M>(ClampMathOp<M>(lhs, rhs), args...);
}

WINBASE_NUMERIC_ARITHMETIC_OPERATORS(Clamped, Clamp, Add, +, +=)
WINBASE_NUMERIC_ARITHMETIC_OPERATORS(Clamped, Clamp, Sub, -, -=)
WINBASE_NUMERIC_ARITHMETIC_OPERATORS(Clamped, Clamp, Mul, *, *=)
WINBASE_NUMERIC_ARITHMETIC_OPERATORS(Clamped, Clamp, Div, /, /=)
WINBASE_NUMERIC_ARITHMETIC_OPERATORS(Clamped, Clamp, Mod, %, %=)
WINBASE_NUMERIC_ARITHMETIC_OPERATORS(Clamped, Clamp, Lsh, <<, <<=)
WINBASE_NUMERIC_ARITHMETIC_OPERATORS(Clamped, Clamp, Rsh, >>, >>=)
WINBASE_NUMERIC_ARITHMETIC_OPERATORS(Clamped, Clamp, And, &, &=)
WINBASE_NUMERIC_ARITHMETIC_OPERATORS(Clamped, Clamp, Or, |, |=)
WINBASE_NUMERIC_ARITHMETIC_OPERATORS(Clamped, Clamp, Xor, ^, ^=)
WINBASE_NUMERIC_ARITHMETIC_VARIADIC(Clamped, Clamp, Max)
WINBASE_NUMERIC_ARITHMETIC_VARIADIC(Clamped, Clamp, Min)
WINBASE_NUMERIC_COMPARISON_OPERATORS(Clamped, IsLess, <);
WINBASE_NUMERIC_COMPARISON_OPERATORS(Clamped, IsLessOrEqual, <=);
WINBASE_NUMERIC_COMPARISON_OPERATORS(Clamped, IsGreater, >);
WINBASE_NUMERIC_COMPARISON_OPERATORS(Clamped, IsGreaterOrEqual, >=);
WINBASE_NUMERIC_COMPARISON_OPERATORS(Clamped, IsEqual, ==);
WINBASE_NUMERIC_COMPARISON_OPERATORS(Clamped, IsNotEqual, !=);

}  // namespace internal

using internal::ClampedNumeric;
using internal::MakeClampedNum;
using internal::ClampMax;
using internal::ClampMin;
using internal::ClampAdd;
using internal::ClampSub;
using internal::ClampMul;
using internal::ClampDiv;
using internal::ClampMod;
using internal::ClampLsh;
using internal::ClampRsh;
using internal::ClampAnd;
using internal::ClampOr;
using internal::ClampXor;

}  // namespace winbase

#endif  // WINLIB_WINBASE_NUMERICS_CLAMPED_MATH_H_