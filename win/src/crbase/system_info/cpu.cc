// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/system_info/cpu.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <intrin.h>
#include <immintrin.h>  // For _xgetbv()

#include <algorithm>

#include "crbase/macros.h"
#include "crbase/strings/string_piece.h"

namespace cr {

namespace {

#if defined(MINI_CHROMIUM_ARCH_CPU_X86_FAMILY)
struct X86ModelInfo {
  int family;
  int model;
  int ext_family;
  int ext_model;
};

X86ModelInfo ComputeX86FamilyAndModel(const std::string& vendor,
                                      int signature) {
  X86ModelInfo results;
  results.family = (signature >> 8) & 0xf;
  results.model = (signature >> 4) & 0xf;
  results.ext_family = 0;
  results.ext_model = 0;

  // The "Intel 64 and IA-32 Architectures Developer's Manual: Vol. 2A"
  // specifies the Extended Model is defined only when the Base Family is
  // 06h or 0Fh.
  // The "AMD CPUID Specification" specifies that the Extended Model is
  // defined only when Base Family is 0Fh.
  // Both manuals define the display model as
  // {ExtendedModel[3:0],BaseModel[3:0]} in that case.
  if (results.family == 0xf ||
      (results.family == 0x6 && vendor == "GenuineIntel")) {
    results.ext_model = (signature >> 16) & 0xf;
    results.model += results.ext_model << 4;
  }
  // Both the "Intel 64 and IA-32 Architectures Developer's Manual: Vol. 2A"
  // and the "AMD CPUID Specification" specify that the Extended Family is
  // defined only when the Base Family is 0Fh.
  // Both manuals define the display family as {0000b,BaseFamily[3:0]} +
  // ExtendedFamily[7:0] in that case.
  if (results.family == 0xf) {
    results.ext_family = (signature >> 20) & 0xff;
    results.family += results.ext_family;
  }

  return results;
}

bool IsCPUSupportVirtualization(const std::string& vendor, int features) {
  // Is intel? Check bit5
  if (vendor == "GenuineIntel")
    return !!(features & 0x20);

  // Is AMD? check bit2
  if (vendor == "AuthenticAMD")
    return !!(features & 0x04);

  // Not intel or AMD, this returns false but you may want to throw
  return false;
}

#endif  // defined(MINI_CHROMIUM_ARCH_CPU_X86_FAMILY)


#if defined(MINI_CHROMIUM_ARCH_CPU_X86_FAMILY)
#if !defined(MINI_CHROMIUM_COMPILER_MSVC)

#if defined(__pic__) && defined(__i386__)

// Requests extended feature information via |ecx|.
void __cpuidex(int cpu_info[4], int eax, int ecx) {
  __asm__ volatile(
      "mov %%ebx, %%edi\n"
      "cpuid\n"
      "xchg %%edi, %%ebx\n"
      : "=a"(cpu_info[0]), "=D"(cpu_info[1]), "=c"(cpu_info[2]),
        "=d"(cpu_info[3])
      : "a"(eax), "c"(ecx));
}

void __cpuid(int cpu_info[4], int info_type) {
  __cpuidex(cpu_info, info_type, /*ecx=*/0);
}

#else

// Requests extended feature information via |ecx|.
void __cpuidex(int cpu_info[4], int eax, int ecx) {
  __asm__ volatile("cpuid\n"
                   : "=a"(cpu_info[0]), "=b"(cpu_info[1]), "=c"(cpu_info[2]),
                     "=d"(cpu_info[3])
                   : "a"(eax), "c"(ecx));
}

void __cpuid(int cpu_info[4], int info_type) {
  __cpuidex(cpu_info, info_type, /*ecx=*/0);
}

#endif
#endif  // !defined(MINI_CHROMIUM_COMPILER_MSVC)

// xgetbv returns the value of an Intel Extended Control Register (XCR).
// Currently only XCR0 is defined by Intel so |xcr| should always be zero.
uint64_t xgetbv(uint32_t xcr) {
#if defined(MINI_CHROMIUM_COMPILER_MSVC)
  return _xgetbv(xcr);
#else
  uint32_t eax, edx;

  __asm__ volatile (
    "xgetbv" : "=a"(eax), "=d"(edx) : "c"(xcr));
  return (static_cast<uint64_t>(edx) << 32) | eax;
#endif  // defined(MINI_CHROMIUM_COMPILER_MSVC)
}

#endif  // MINI_CHROMIUM_ARCH_CPU_X86_FAMILY

}  // namespace

CPU::CPU()
  : cpu_vendor_("unknown"),
    cpu_brand_(),
    signature_(0),
    type_(0),
    family_(0),
    model_(0),
    stepping_(0),
    ext_model_(0),
    ext_family_(0),
#if defined(MINI_CHROMIUM_ARCH_CPU_X86_FAMILY)
    has_mmx_(false),
    has_sse_(false),
    has_sse2_(false),
    has_sse3_(false),
    has_ssse3_(false),
    has_sse41_(false),
    has_sse42_(false),
    has_popcnt_(false),
    has_avx_(false),
    has_fma3_(false),
    has_avx2_(false),
    has_avx_vnni_(false),
    has_avx512_f_(false),
    has_avx512_bw_(false),
    has_avx512_vnni_(false),
    has_pku_(false),
    support_virtualization_(false),
#endif
    has_aesni_(false),
#if defined(MINI_CHROMIUM_ARCH_CPU_ARM_FAMILY)
    implementer_(0),
    part_number_(0),
    has_mte_(false),
    has_bti_(false),
#endif
    has_non_stop_time_stamp_counter_(false),
    is_running_in_vm_(false) {
  Initialize();
}


void CPU::Initialize() {
#if defined(MINI_CHROMIUM_ARCH_CPU_X86_FAMILY)
  int cpu_info[4] = {-1};
  char cpu_string[48];

  // __cpuid with an InfoType argument of 0 returns the number of
  // valid Ids in CPUInfo[0] and the CPU identification string in
  // the other three array elements. The CPU identification string is
  // not in linear order. The code below arranges the information
  // in a human readable form. The human readable order is CPUInfo[1] |
  // CPUInfo[3] | CPUInfo[2]. CPUInfo[2] and CPUInfo[3] are swapped
  // before using memcpy to copy these three array elements to cpu_string.
  __cpuid(cpu_info, 0);
  int num_ids = cpu_info[0];
  std::swap(cpu_info[2], cpu_info[3]);
  memcpy(cpu_string, &cpu_info[1], 3 * sizeof(cpu_info[1]));
  cpu_vendor_.assign(cpu_string, 3 * sizeof(cpu_info[1]));

  // Interpret CPU feature information.
  if (num_ids > 0) {
    int cpu_info7[4] = {0};
    int cpu_einfo7[4] = {0};
    __cpuid(cpu_info, 1);
    if (num_ids >= 7) {
      __cpuid(cpu_info7, 7);
      if (cpu_info7[0] >= 1) {
        __cpuidex(cpu_einfo7, 7, 1);
      }
    }
    signature_ = cpu_info[0];
    stepping_ = cpu_info[0] & 0xf;
    type_ = (cpu_info[0] >> 12) & 0x3;
    support_virtualization_ =
        IsCPUSupportVirtualization(cpu_vendor_, cpu_info[2]);
    X86ModelInfo results = ComputeX86FamilyAndModel(cpu_vendor_, signature_);
    family_ = results.family;
    model_ = results.model;
    ext_family_ = results.ext_family;
    ext_model_ = results.ext_model;
    has_mmx_ =   (cpu_info[3] & 0x00800000) != 0;
    has_sse_ =   (cpu_info[3] & 0x02000000) != 0;
    has_sse2_ =  (cpu_info[3] & 0x04000000) != 0;
    has_sse3_ =  (cpu_info[2] & 0x00000001) != 0;
    has_ssse3_ = (cpu_info[2] & 0x00000200) != 0;
    has_sse41_ = (cpu_info[2] & 0x00080000) != 0;
    has_sse42_ = (cpu_info[2] & 0x00100000) != 0;
    has_popcnt_ = (cpu_info[2] & 0x00800000) != 0;

    // "Hypervisor Present Bit: Bit 31 of ECX of CPUID leaf 0x1."
    // See https://lwn.net/Articles/301888/
    // This is checking for any hypervisor. Hypervisors may choose not to
    // announce themselves. Hypervisors trap CPUID and sometimes return
    // different results to underlying hardware.
    is_running_in_vm_ = (static_cast<uint32_t>(cpu_info[2]) & 0x80000000) != 0;

    // AVX instructions will generate an illegal instruction exception unless
    //   a) they are supported by the CPU,
    //   b) XSAVE is supported by the CPU and
    //   c) XSAVE is enabled by the kernel.
    // See http://software.intel.com/en-us/blogs/2011/04/14/is-avx-enabled
    //
    // In addition, we have observed some crashes with the xgetbv instruction
    // even after following Intel's example code. (See crbug.com/375968.)
    // Because of that, we also test the XSAVE bit because its description in
    // the CPUID documentation suggests that it signals xgetbv support.
    has_avx_ =
        (cpu_info[2] & 0x10000000) != 0 &&
        (cpu_info[2] & 0x04000000) != 0 /* XSAVE */ &&
        (cpu_info[2] & 0x08000000) != 0 /* OSXSAVE */ &&
        (xgetbv(0) & 6) == 6 /* XSAVE enabled by kernel */;
    has_aesni_ = (cpu_info[2] & 0x02000000) != 0;
    has_fma3_ = (cpu_info[2] & 0x00001000) != 0;
    if (has_avx_) {
      has_avx2_ = (cpu_info7[1] & 0x00000020) != 0;
      has_avx_vnni_ = (cpu_einfo7[0] & 0x00000010) != 0;
      // Check AVX-512 state, bits 5-7.
      if ((xgetbv(0) & 0xe0) == 0xe0) {
        has_avx512_f_ = (cpu_info7[1] & 0x00010000) != 0;
        has_avx512_bw_ = (cpu_info7[1] & 0x40000000) != 0;
        has_avx512_vnni_ = (cpu_info7[2] & 0x00000800) != 0;
      }
    }
    has_pku_ = (cpu_info7[2] & 0x00000010) != 0;
  }

  // Get the brand string of the cpu.
  __cpuid(cpu_info, 0x80000000);
  const int parameter_end = 0x80000004;
  int max_parameter = cpu_info[0];

  if (cpu_info[0] >= parameter_end) {
    char* cpu_string_ptr = cpu_string;

    for (int parameter = 0x80000002; parameter <= parameter_end &&
         cpu_string_ptr < &cpu_string[sizeof(cpu_string)]; parameter++) {
      __cpuid(cpu_info, parameter);
      memcpy(cpu_string_ptr, cpu_info, sizeof(cpu_info));
      cpu_string_ptr += sizeof(cpu_info);
    }
    cpu_brand_.assign(cpu_string, cpu_string_ptr - cpu_string);
  }

  const int parameter_containing_non_stop_time_stamp_counter = 0x80000007;
  if (max_parameter >= parameter_containing_non_stop_time_stamp_counter) {
    __cpuid(cpu_info, parameter_containing_non_stop_time_stamp_counter);
    has_non_stop_time_stamp_counter_ = (cpu_info[3] & (1 << 8)) != 0;
  }

  if (!has_non_stop_time_stamp_counter_ && is_running_in_vm_) {
    int cpu_info_hv[4] = {};
    __cpuid(cpu_info_hv, 0x40000000);
    if (cpu_info_hv[1] == 0x7263694D &&  // Micr
        cpu_info_hv[2] == 0x666F736F &&  // osof
        cpu_info_hv[3] == 0x76482074) {  // t Hv
      // If CPUID says we have a variant TSC and a hypervisor has identified
      // itself and the hypervisor says it is Microsoft Hyper-V, then treat
      // TSC as invariant.
      //
      // Microsoft Hyper-V hypervisor reports variant TSC as there are some
      // scenarios (eg. VM live migration) where the TSC is variant, but for
      // our purposes we can treat it as invariant.
      has_non_stop_time_stamp_counter_ = true;
    }
  }
#elif defined(MINI_CHROMIUM_ARCH_CPU_ARM_FAMILY)
  // Windows makes high-resolution thread timing information available in
  // user-space.
  has_non_stop_time_stamp_counter_ = true;
#endif
}

#if defined(MINI_CHROMIUM_ARCH_CPU_X86_FAMILY)
CPU::IntelMicroArchitecture CPU::GetIntelMicroArchitecture() const {
  if (has_avx512_vnni()) return AVX512_VNNI;
  if (has_avx512_bw()) return AVX512BW;
  if (has_avx512_f()) return AVX512F;
  if (has_avx_vnni()) return AVX_VNNI;
  if (has_avx2()) return AVX2;
  if (has_fma3()) return FMA3;
  if (has_avx()) return AVX;
  if (has_sse42()) return SSE42;
  if (has_sse41()) return SSE41;
  if (has_ssse3()) return SSSE3;
  if (has_sse3()) return SSE3;
  if (has_sse2()) return SSE2;
  if (has_sse()) return SSE;
  return PENTIUM;
}
#endif

}  // namespace cr
