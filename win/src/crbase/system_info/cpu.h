// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_SYSTEM_INFO_CPU_H_
#define MINI_CHROMIUM_SRC_CRBASE_SYSTEM_INFO_CPU_H_

#include <string>

#include "crbase/build_config.h"
#include "crbase/base_export.h"

namespace cr {

// Query information about the processor.
class CRBASE_EXPORT CPU {
 public:
  // Constructor
  CPU();

  enum IntelMicroArchitecture {
    PENTIUM = 0,
    SSE = 1,
    SSE2 = 2,
    SSE3 = 3,
    SSSE3 = 4,
    SSE41 = 5,
    SSE42 = 6,
    AVX = 7,
    AVX2 = 8,
    FMA3 = 9,
    AVX_VNNI = 10,
    AVX512F = 11,
    AVX512BW = 12,
    AVX512_VNNI = 13,
    MAX_INTEL_MICRO_ARCHITECTURE = 14
  };

  // Accessors for CPU information.
  const std::string& vendor_name() const { return cpu_vendor_; }
  int signature() const { return signature_; }
  int stepping() const { return stepping_; }
  int model() const { return model_; }
  int family() const { return family_; }
  int type() const { return type_; }
  int extended_model() const { return ext_model_; }
  int extended_family() const { return ext_family_; }
#if defined(MINI_CHROMIUM_ARCH_CPU_X86_FAMILY)
  bool has_mmx() const { return has_mmx_; }
  bool has_sse() const { return has_sse_; }
  bool has_sse2() const { return has_sse2_; }
  bool has_sse3() const { return has_sse3_; }
  bool has_ssse3() const { return has_ssse3_; }
  bool has_sse41() const { return has_sse41_; }
  bool has_sse42() const { return has_sse42_; }
  bool has_popcnt() const { return has_popcnt_; }
  bool has_avx() const { return has_avx_; }
  bool has_fma3() const { return has_fma3_; }
  bool has_avx2() const { return has_avx2_; }
  bool has_avx_vnni() const { return has_avx_vnni_; }
  bool has_avx512_f() const { return has_avx512_f_; }
  bool has_avx512_bw() const { return has_avx512_bw_; }
  bool has_avx512_vnni() const { return has_avx512_vnni_; }
  bool support_virtualization() const { return support_virtualization_; }
#endif
  bool has_aesni() const { return has_aesni_; }
  bool has_non_stop_time_stamp_counter() const {
    return has_non_stop_time_stamp_counter_;
  }
  bool is_running_in_vm() const { return is_running_in_vm_; }

#if defined(MINI_CHROMIUM_ARCH_CPU_ARM_FAMILY)
  // The cpuinfo values for ARM cores are from the MIDR_EL1 register, a
  // bitfield whose format is described in the core-specific manuals. E.g.,
  // ARM Cortex-A57:
  // https://developer.arm.com/documentation/ddi0488/h/system-control/aarch64-register-descriptions/main-id-register--el1.
  uint8_t implementer() const { return implementer_; }
  uint32_t part_number() const { return part_number_; }
#endif

  // Armv8.5-A extensions for control flow and memory safety.
#if defined(MINI_CHROMIUM_ARCH_CPU_ARM_FAMILY)
  bool has_mte() const { return has_mte_; }
  bool has_bti() const { return has_bti_; }
#else
  bool has_mte() const { return false; }
  bool has_bti() const { return false; }
#endif

#if defined(MINI_CHROMIUM_ARCH_CPU_X86_FAMILY)
  // Memory protection key support for user-mode pages
  bool has_pku() const { return has_pku_; }
#else
  bool has_pku() const { return false; }
#endif

 #if defined(MINI_CHROMIUM_ARCH_CPU_X86_FAMILY)
  IntelMicroArchitecture GetIntelMicroArchitecture() const;
#endif

  const std::string& cpu_brand() const { return cpu_brand_; }

 private:
  // Query the processor for CPUID information.
  void Initialize();

  std::string cpu_vendor_;
  std::string cpu_brand_;

  int signature_;  // raw form of type, family, model, and stepping
  int type_;  // process type
  int family_;  // family of the processor
  int model_;  // model of processor
  int stepping_;  // processor revision number
  int ext_model_;
  int ext_family_;
#if defined(MINI_CHROMIUM_ARCH_CPU_ARM_FAMILY)
  uint32_t part_number_;  // ARM MIDR part number
  uint8_t implementer_;   // ARM MIDR implementer identifier
#endif
#if defined(MINI_CHROMIUM_ARCH_CPU_X86_FAMILY)
  bool has_mmx_;
  bool has_sse_;
  bool has_sse2_;
  bool has_sse3_;
  bool has_ssse3_;
  bool has_sse41_;
  bool has_sse42_;
  bool has_popcnt_;
  bool has_avx_;
  bool has_fma3_;
  bool has_avx2_;
  bool has_avx_vnni_;
  bool has_avx512_f_;
  bool has_avx512_bw_;
  bool has_avx512_vnni_;
  bool has_pku_;
  bool support_virtualization_;
#endif

  bool has_aesni_;

#if defined(MINI_CHROMIUM_ARCH_CPU_ARM_FAMILY)
  bool has_mte_;  // Armv8.5-A MTE (Memory Taggging Extension)
  bool has_bti_;  // Armv8.5-A BTI (Branch Target Identification)
#endif

  bool has_non_stop_time_stamp_counter_;
  bool is_running_in_vm_;
};

}  // namespace cr

#endif  // MINI_CHROMIUM_SRC_CRBASE_SYSTEM_INFO_CPU_H_
