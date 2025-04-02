// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/win/windows_version.h"

#include <windows.h>

#include <memory>
#include <map>

#include "crbase/build_config.h"
#include "crbase/compiler_specific.h"
#include "crbase/logging.h"
#include "crbase/strings/utf_string_conversions.h"
#include "crbase/file_version_info.h"
#include "crbase/files/file_path.h"
#include "crbase/win/registry.h"

#ifndef PROCESSOR_ARCHITECTURE_ARM64
#define PROCESSOR_ARCHITECTURE_ARM64 12
#endif

#ifndef IMAGE_FILE_MACHINE_ARM64
#define IMAGE_FILE_MACHINE_ARM64 0xAA64  // ARM64 Little-Endian
#endif

#define OS_PRODUCT_UNDEFINED                           0x00000000

#define OS_PRODUCT_ULTIMATE                            0x00000001
#define OS_PRODUCT_HOME_BASIC                          0x00000002
#define OS_PRODUCT_HOME_PREMIUM                        0x00000003
#define OS_PRODUCT_ENTERPRISE                          0x00000004
#define OS_PRODUCT_HOME_BASIC_N                        0x00000005
#define OS_PRODUCT_BUSINESS                            0x00000006
#define OS_PRODUCT_STANDARD_SERVER                     0x00000007
#define OS_PRODUCT_DATACENTER_SERVER                   0x00000008
#define OS_PRODUCT_SMALLBUSINESS_SERVER                0x00000009
#define OS_PRODUCT_ENTERPRISE_SERVER                   0x0000000A
#define OS_PRODUCT_STARTER                             0x0000000B
#define OS_PRODUCT_DATACENTER_SERVER_CORE              0x0000000C
#define OS_PRODUCT_STANDARD_SERVER_CORE                0x0000000D
#define OS_PRODUCT_ENTERPRISE_SERVER_CORE              0x0000000E
#define OS_PRODUCT_ENTERPRISE_SERVER_IA64              0x0000000F
#define OS_PRODUCT_BUSINESS_N                          0x00000010
#define OS_PRODUCT_WEB_SERVER                          0x00000011
#define OS_PRODUCT_CLUSTER_SERVER                      0x00000012
#define OS_PRODUCT_HOME_SERVER                         0x00000013
#define OS_PRODUCT_STORAGE_EXPRESS_SERVER              0x00000014
#define OS_PRODUCT_STORAGE_STANDARD_SERVER             0x00000015
#define OS_PRODUCT_STORAGE_WORKGROUP_SERVER            0x00000016
#define OS_PRODUCT_STORAGE_ENTERPRISE_SERVER           0x00000017
#define OS_PRODUCT_SERVER_FOR_SMALLBUSINESS            0x00000018
#define OS_PRODUCT_SMALLBUSINESS_SERVER_PREMIUM        0x00000019
#define OS_PRODUCT_HOME_PREMIUM_N                      0x0000001A
#define OS_PRODUCT_ENTERPRISE_N                        0x0000001B
#define OS_PRODUCT_ULTIMATE_N                          0x0000001C
#define OS_PRODUCT_WEB_SERVER_CORE                     0x0000001D
#define OS_PRODUCT_MEDIUMBUSINESS_SERVER_MANAGEMENT    0x0000001E
#define OS_PRODUCT_MEDIUMBUSINESS_SERVER_SECURITY      0x0000001F
#define OS_PRODUCT_MEDIUMBUSINESS_SERVER_MESSAGING     0x00000020
#define OS_PRODUCT_SERVER_FOUNDATION                   0x00000021
#define OS_PRODUCT_HOME_PREMIUM_SERVER                 0x00000022
#define OS_PRODUCT_SERVER_FOR_SMALLBUSINESS_V          0x00000023
#define OS_PRODUCT_STANDARD_SERVER_V                   0x00000024
#define OS_PRODUCT_DATACENTER_SERVER_V                 0x00000025
#define OS_PRODUCT_ENTERPRISE_SERVER_V                 0x00000026
#define OS_PRODUCT_DATACENTER_SERVER_CORE_V            0x00000027
#define OS_PRODUCT_STANDARD_SERVER_CORE_V              0x00000028
#define OS_PRODUCT_ENTERPRISE_SERVER_CORE_V            0x00000029
#define OS_PRODUCT_HYPERV                              0x0000002A
#define OS_PRODUCT_STORAGE_EXPRESS_SERVER_CORE         0x0000002B
#define OS_PRODUCT_STORAGE_STANDARD_SERVER_CORE        0x0000002C
#define OS_PRODUCT_STORAGE_WORKGROUP_SERVER_CORE       0x0000002D
#define OS_PRODUCT_STORAGE_ENTERPRISE_SERVER_CORE      0x0000002E
#define OS_PRODUCT_STARTER_N                           0x0000002F
#define OS_PRODUCT_PROFESSIONAL                        0x00000030
#define OS_PRODUCT_PROFESSIONAL_N                      0x00000031
#define OS_PRODUCT_SB_SOLUTION_SERVER                  0x00000032
#define OS_PRODUCT_SERVER_FOR_SB_SOLUTIONS             0x00000033
#define OS_PRODUCT_STANDARD_SERVER_SOLUTIONS           0x00000034
#define OS_PRODUCT_STANDARD_SERVER_SOLUTIONS_CORE      0x00000035
#define OS_PRODUCT_SB_SOLUTION_SERVER_EM               0x00000036
#define OS_PRODUCT_SERVER_FOR_SB_SOLUTIONS_EM          0x00000037
#define OS_PRODUCT_SOLUTION_EMBEDDEDSERVER             0x00000038
#define OS_PRODUCT_SOLUTION_EMBEDDEDSERVER_CORE        0x00000039
#define OS_PRODUCT_PROFESSIONAL_EMBEDDED               0x0000003A
#define OS_PRODUCT_ESSENTIALBUSINESS_SERVER_MGMT       0x0000003B
#define OS_PRODUCT_ESSENTIALBUSINESS_SERVER_ADDL       0x0000003C
#define OS_PRODUCT_ESSENTIALBUSINESS_SERVER_MGMTSVC    0x0000003D
#define OS_PRODUCT_ESSENTIALBUSINESS_SERVER_ADDLSVC    0x0000003E
#define OS_PRODUCT_SMALLBUSINESS_SERVER_PREMIUM_CORE   0x0000003F
#define OS_PRODUCT_CLUSTER_SERVER_V                    0x00000040
#define OS_PRODUCT_EMBEDDED                            0x00000041
#define OS_PRODUCT_STARTER_E                           0x00000042
#define OS_PRODUCT_HOME_BASIC_E                        0x00000043
#define OS_PRODUCT_HOME_PREMIUM_E                      0x00000044
#define OS_PRODUCT_PROFESSIONAL_E                      0x00000045
#define OS_PRODUCT_ENTERPRISE_E                        0x00000046
#define OS_PRODUCT_ULTIMATE_E                          0x00000047
#define OS_PRODUCT_ENTERPRISE_EVALUATION               0x00000048
#define OS_PRODUCT_MULTIPOINT_STANDARD_SERVER          0x0000004C
#define OS_PRODUCT_MULTIPOINT_PREMIUM_SERVER           0x0000004D
#define OS_PRODUCT_STANDARD_EVALUATION_SERVER          0x0000004F
#define OS_PRODUCT_DATACENTER_EVALUATION_SERVER        0x00000050
#define OS_PRODUCT_ENTERPRISE_N_EVALUATION             0x00000054
#define OS_PRODUCT_EMBEDDED_AUTOMOTIVE                 0x00000055
#define OS_PRODUCT_EMBEDDED_INDUSTRY_A                 0x00000056
#define OS_PRODUCT_THINPC                              0x00000057
#define OS_PRODUCT_EMBEDDED_A                          0x00000058
#define OS_PRODUCT_EMBEDDED_INDUSTRY                   0x00000059
#define OS_PRODUCT_EMBEDDED_E                          0x0000005A
#define OS_PRODUCT_EMBEDDED_INDUSTRY_E                 0x0000005B
#define OS_PRODUCT_EMBEDDED_INDUSTRY_A_E               0x0000005C
#define OS_PRODUCT_STORAGE_WORKGROUP_EVALUATION_SERVER 0x0000005F
#define OS_PRODUCT_STORAGE_STANDARD_EVALUATION_SERVER  0x00000060
#define OS_PRODUCT_CORE_ARM                            0x00000061
#define OS_PRODUCT_CORE_N                              0x00000062
#define OS_PRODUCT_CORE_COUNTRYSPECIFIC                0x00000063
#define OS_PRODUCT_CORE_SINGLELANGUAGE                 0x00000064
#define OS_PRODUCT_CORE                                0x00000065
#define OS_PRODUCT_PROFESSIONAL_WMC                    0x00000067
#define OS_PRODUCT_EMBEDDED_INDUSTRY_EVAL              0x00000069
#define OS_PRODUCT_EMBEDDED_INDUSTRY_E_EVAL            0x0000006A
#define OS_PRODUCT_EMBEDDED_EVAL                       0x0000006B
#define OS_PRODUCT_EMBEDDED_E_EVAL                     0x0000006C
#define OS_PRODUCT_NANO_SERVER                         0x0000006D
#define OS_PRODUCT_CLOUD_STORAGE_SERVER                0x0000006E
#define OS_PRODUCT_CORE_CONNECTED                      0x0000006F
#define OS_PRODUCT_PROFESSIONAL_STUDENT                0x00000070
#define OS_PRODUCT_CORE_CONNECTED_N                    0x00000071
#define OS_PRODUCT_PROFESSIONAL_STUDENT_N              0x00000072
#define OS_PRODUCT_CORE_CONNECTED_SINGLELANGUAGE       0x00000073
#define OS_PRODUCT_CORE_CONNECTED_COUNTRYSPECIFIC      0x00000074
#define OS_PRODUCT_CONNECTED_CAR                       0x00000075
#define OS_PRODUCT_INDUSTRY_HANDHELD                   0x00000076
#define OS_PRODUCT_PPI_PRO                             0x00000077
#define OS_PRODUCT_ARM64_SERVER                        0x00000078
#define OS_PRODUCT_EDUCATION                           0x00000079
#define OS_PRODUCT_EDUCATION_N                         0x0000007A
#define OS_PRODUCT_IOTUAP                              0x0000007B
#define OS_PRODUCT_CLOUD_HOST_INFRASTRUCTURE_SERVER    0x0000007C
#define OS_PRODUCT_ENTERPRISE_S                        0x0000007D
#define OS_PRODUCT_ENTERPRISE_S_N                      0x0000007E
#define OS_PRODUCT_PROFESSIONAL_S                      0x0000007F
#define OS_PRODUCT_PROFESSIONAL_S_N                    0x00000080
#define OS_PRODUCT_ENTERPRISE_S_EVALUATION             0x00000081
#define OS_PRODUCT_ENTERPRISE_S_N_EVALUATION           0x00000082
#define OS_PRODUCT_HOLOGRAPHIC                         0x00000087
#define OS_PRODUCT_HOLOGRAPHIC_BUSINESS                0x00000088
#define OS_PRODUCT_PRO_SINGLE_LANGUAGE                 0x0000008A
#define OS_PRODUCT_PRO_CHINA                           0x0000008B
#define OS_PRODUCT_ENTERPRISE_SUBSCRIPTION             0x0000008C
#define OS_PRODUCT_ENTERPRISE_SUBSCRIPTION_N           0x0000008D
#define OS_PRODUCT_DATACENTER_NANO_SERVER              0x0000008F
#define OS_PRODUCT_STANDARD_NANO_SERVER                0x00000090
#define OS_PRODUCT_DATACENTER_A_SERVER_CORE            0x00000091
#define OS_PRODUCT_STANDARD_A_SERVER_CORE              0x00000092
#define OS_PRODUCT_DATACENTER_WS_SERVER_CORE           0x00000093
#define OS_PRODUCT_STANDARD_WS_SERVER_CORE             0x00000094
#define OS_PRODUCT_UTILITY_VM                          0x00000095
#define OS_PRODUCT_DATACENTER_EVALUATION_SERVER_CORE   0x0000009F
#define OS_PRODUCT_STANDARD_EVALUATION_SERVER_CORE     0x000000A0
#define OS_PRODUCT_PRO_WORKSTATION                     0x000000A1
#define OS_PRODUCT_PRO_WORKSTATION_N                   0x000000A2
#define OS_PRODUCT_PRO_FOR_EDUCATION                   0x000000A4
#define OS_PRODUCT_PRO_FOR_EDUCATION_N                 0x000000A5
#define OS_PRODUCT_AZURE_SERVER_CORE                   0x000000A8
#define OS_PRODUCT_AZURE_NANO_SERVER                   0x000000A9
#define OS_PRODUCT_ENTERPRISEG                         0x000000AB
#define OS_PRODUCT_ENTERPRISEGN                        0x000000AC
#define OS_PRODUCT_SERVERRDSH                          0x000000AF
#define OS_PRODUCT_CLOUD                               0x000000B2
#define OS_PRODUCT_CLOUDN                              0x000000B3
#define OS_PRODUCT_HUBOS                               0x000000B4
#define OS_PRODUCT_ONECOREUPDATEOS                     0x000000B6
#define OS_PRODUCT_CLOUDE                              0x000000B7
#define OS_PRODUCT_ANDROMEDA                           0x000000B8
#define OS_PRODUCT_IOTOS                               0x000000B9
#define OS_PRODUCT_CLOUDEN                             0x000000BA
#define OS_PRODUCT_IOTEDGEOS                           0x000000BB
#define OS_PRODUCT_IOTENTERPRISE                       0x000000BC
#define OS_PRODUCT_LITE                                0x000000BD
#define OS_PRODUCT_IOTENTERPRISES                      0x000000BF
#define OS_PRODUCT_XBOX_SYSTEMOS                       0x000000C0
#define OS_PRODUCT_XBOX_NATIVEOS                       0x000000C1
#define OS_PRODUCT_XBOX_GAMEOS                         0x000000C2
#define OS_PRODUCT_XBOX_ERAOS                          0x000000C3
#define OS_PRODUCT_XBOX_DURANGOHOSTOS                  0x000000C4
#define OS_PRODUCT_XBOX_SCARLETTHOSTOS                 0x000000C5

namespace cr {
namespace win {

namespace {
typedef BOOL (WINAPI * IsWow64Process2Ptr)(HANDLE, USHORT*, USHORT*);
typedef DWORD(WINAPI* RtlGetVersionPtr)(LPOSVERSIONINFOEXW);
typedef BOOL (WINAPI *GetProductInfoPtr)(DWORD, DWORD, DWORD, DWORD, PDWORD);

typedef struct _PROCESSOR_POWER_INFORMATION {
  ULONG Number;
  ULONG MaxMhz;
  ULONG CurrentMhz;
  ULONG MhzLimit;
  ULONG MaxIdleState;
  ULONG CurrentIdleState;
} PROCESSOR_POWER_INFORMATION, *PPROCESSOR_POWER_INFORMATION;

// The values under the CurrentVersion registry hive are mirrored under
// the corresponding Wow6432 hive.
const wchar_t kRegKeyWindowsNTCurrentVersion[] =
    L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";

// Returns the "UBR" (Windows 10 patch number) and "DisplayVersion" (or
// "ReleaseId" on earlier versions) (Windows 10 release number) from registry.
// "UBR" is an undocumented value and will be 0 if the value was not found.
// "ReleaseId" will be an empty string if neither new nor old values are found.
std::pair<int, std::string> GetVersionData() {
  DWORD ubr = 0;
  std::wstring release_id;
  RegKey key;

  if (key.Open(HKEY_LOCAL_MACHINE, kRegKeyWindowsNTCurrentVersion,
               KEY_QUERY_VALUE) == ERROR_SUCCESS) {
    key.ReadValueDW(L"UBR", &ubr);
    // "DisplayVersion" has been introduced in Windows 10 2009
    // when naming changed to mixed letters and numbers.
    key.ReadValue(L"DisplayVersion", &release_id);
    // Use discontinued "ReleaseId" instead, if the former is unavailable.
    if (release_id.empty())
      key.ReadValue(L"ReleaseId", &release_id);
  }

  return std::make_pair(static_cast<int>(ubr), WideToUTF8(release_id));
}

const SYSTEM_INFO& GetSystemInfoStorage() {
	static const SYSTEM_INFO system_info = [] {
		SYSTEM_INFO info = {};
		::GetNativeSystemInfo(&info);
		return info;
	}();
	return system_info;
}

}  // namespace

// static
OSInfo** OSInfo::GetInstanceStorage() {
	// Note: we don't use the Singleton class because it depends on AtExitManager,
	// and it's convenient for other modules to use this class without it.
	static OSInfo* info = []() {
		OSVERSIONINFOEXW version_info;
    ZeroMemory(&version_info, sizeof(version_info));
    version_info.dwOSVersionInfoSize = sizeof(version_info);

    RtlGetVersionPtr RtlGetVersion = reinterpret_cast<RtlGetVersionPtr>(
        GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));

    if (RtlGetVersion == NULL) {
      // GetVersionEx() is deprecated, and the suggested replacement are
      // the IsWindows*OrGreater() functions in VersionHelpers.h. We can't
      // use that because:
      // - For Windows 10, there's IsWindows10OrGreater(), but nothing more
      //   granular. We need to be able to detect different Windows 10 releases
      //   since they sometimes change behavior in ways that matter.
      // - There is no IsWindows11OrGreater() function yet.
      MSVC_PUSH_DISABLE_WARNING(4996)
      ::GetVersionExW(reinterpret_cast<OSVERSIONINFOW*>(&version_info));
      MSVC_POP_WARNING()
    } else {
      RtlGetVersion(&version_info);
    }

    GetProductInfoPtr get_product_info =
      reinterpret_cast<GetProductInfoPtr>(
        GetProcAddress(::GetModuleHandleW(L"kernel32.dll"), "GetProductInfo"));
		DWORD os_type = 0;

    if (get_product_info)
      get_product_info(version_info.dwMajorVersion, version_info.dwMinorVersion,
                  		 0, 0, &os_type);

		return new OSInfo(version_info, GetSystemInfoStorage(), os_type);
	}();

	return &info;
}

// static
OSInfo* OSInfo::GetInstance() {
  //// Note: we don't use the Singleton class because it depends on AtExitManager,
  //// and it's convenient for other modules to use this classs without it. This
  //// pattern is copied from gurl.cc.
  //static OSInfo* info = NULL;
  //if (!info) {
  //  OSInfo* new_info = new OSInfo();
  //  if (InterlockedCompareExchangePointer(
  //      reinterpret_cast<PVOID*>(&info), new_info, NULL)) {
  //    delete new_info;
  //  }
  //}
  //return info;
  return *GetInstanceStorage();
}

// static
OSInfo::WindowsArchitecture OSInfo::GetArchitecture() {
  switch (GetSystemInfoStorage().wProcessorArchitecture) {
    case PROCESSOR_ARCHITECTURE_INTEL:
      return X86_ARCHITECTURE;
    case PROCESSOR_ARCHITECTURE_AMD64:
      return X64_ARCHITECTURE;
    case PROCESSOR_ARCHITECTURE_IA64:
      return IA64_ARCHITECTURE;
    case PROCESSOR_ARCHITECTURE_ARM64:
      return ARM64_ARCHITECTURE;
    default:
      return OTHER_ARCHITECTURE;
  }
}

// Returns true if this is an x86/x64 process running on ARM64 through
// emulation.
// static
bool OSInfo::IsRunningEmulatedOnArm64() {
#if defined(MINI_CHROMIUM_ARCH_CPU_ARM64)
  // If we're running native ARM64 then we aren't running emulated.
  return false;
#else
  IsWow64Process2Ptr is_wow64_process2 =
      reinterpret_cast<IsWow64Process2Ptr>(::GetProcAddress(
          ::GetModuleHandleA("kernel32.dll"), "IsWow64Process2"));
  if (!is_wow64_process2) {
    return false;
  }
  USHORT process_machine;
  USHORT native_machine;
  bool retval = !!is_wow64_process2(::GetCurrentProcess(), &process_machine,
                                    &native_machine);
  if (!retval) {
    return false;
  }
  if (native_machine == IMAGE_FILE_MACHINE_ARM64) {
    return true;
  }
  return false;
#endif
}

OSInfo::OSInfo(const _OSVERSIONINFOEXW& version_info,
               const _SYSTEM_INFO& system_info,
               DWORD os_type)
    : version_(Version::PRE_XP),
      wow_process_machine_(WowProcessMachine::kUnknown),
      wow_native_machine_(WowNativeMachine::kUnknown),
      os_type_(os_type) {
  version_number_.major = version_info.dwMajorVersion;
  version_number_.minor = version_info.dwMinorVersion;
  version_number_.build = version_info.dwBuildNumber;
  //std::tie(version_number_.patch, release_id_) = GetVersionData();
  std::pair<int, std::string> version_data = GetVersionData();
  version_number_.patch = version_data.first;
  release_id_ = version_data.second;

  version_ = MajorMinorBuildToVersion(
      version_number_.major, version_number_.minor, version_number_.build);
  InitializeWowStatusValuesForProcess(GetCurrentProcess());
  service_pack_.major = version_info.wServicePackMajor;
  service_pack_.minor = version_info.wServicePackMinor;
  service_pack_str_ = WideToUTF8(version_info.szCSDVersion);

  processors_ = static_cast<int>(system_info.dwNumberOfProcessors);
  allocation_granularity_ = system_info.dwAllocationGranularity;

  if (version_info.dwMajorVersion == 6 || version_info.dwMajorVersion == 10) {
    // Only present on Vista+.
    switch (os_type) {
      case OS_PRODUCT_CLUSTER_SERVER:
      case OS_PRODUCT_DATACENTER_SERVER:
      case OS_PRODUCT_DATACENTER_SERVER_CORE:
      case OS_PRODUCT_ENTERPRISE_SERVER:
      case OS_PRODUCT_ENTERPRISE_SERVER_CORE:
      case OS_PRODUCT_ENTERPRISE_SERVER_IA64:
      case OS_PRODUCT_SMALLBUSINESS_SERVER:
      case OS_PRODUCT_SMALLBUSINESS_SERVER_PREMIUM:
      case OS_PRODUCT_STANDARD_SERVER:
      case OS_PRODUCT_STANDARD_SERVER_CORE:
      case OS_PRODUCT_WEB_SERVER:
        version_type_ = VersionType::SUITE_SERVER;
        break;
      case OS_PRODUCT_PROFESSIONAL:
      case OS_PRODUCT_ULTIMATE:
        version_type_ = VersionType::SUITE_PROFESSIONAL;
        break;
      case OS_PRODUCT_ENTERPRISE:
      case OS_PRODUCT_ENTERPRISE_E:
      case OS_PRODUCT_ENTERPRISE_EVALUATION:
      case OS_PRODUCT_ENTERPRISE_N:
      case OS_PRODUCT_ENTERPRISE_N_EVALUATION:
      case OS_PRODUCT_ENTERPRISE_S:
      case OS_PRODUCT_ENTERPRISE_S_EVALUATION:
      case OS_PRODUCT_ENTERPRISE_S_N:
      case OS_PRODUCT_ENTERPRISE_S_N_EVALUATION:
      case OS_PRODUCT_ENTERPRISE_SUBSCRIPTION:
      case OS_PRODUCT_ENTERPRISE_SUBSCRIPTION_N:
      case OS_PRODUCT_BUSINESS:
      case OS_PRODUCT_BUSINESS_N:
      case OS_PRODUCT_IOTENTERPRISE:
      case OS_PRODUCT_IOTENTERPRISES:
        version_type_ = VersionType::SUITE_ENTERPRISE;
        break;
      case OS_PRODUCT_PRO_FOR_EDUCATION:
      case OS_PRODUCT_PRO_FOR_EDUCATION_N:
        version_type_ = VersionType::SUITE_EDUCATION_PRO;
        break;
      case OS_PRODUCT_EDUCATION:
      case OS_PRODUCT_EDUCATION_N:
        version_type_ = VersionType::SUITE_EDUCATION;
        break;
      case OS_PRODUCT_HOME_BASIC:
      case OS_PRODUCT_HOME_PREMIUM:
      case OS_PRODUCT_STARTER:
      default:
        version_type_ = VersionType::SUITE_HOME;
        break;
    }
  } else if (version_info.dwMajorVersion == 5 &&
             version_info.dwMinorVersion == 2) {
    if (version_info.wProductType == VER_NT_WORKSTATION &&
        system_info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) {
      version_type_ = VersionType::SUITE_PROFESSIONAL;
    } else if (version_info.wSuiteMask & VER_SUITE_WH_SERVER) {
      version_type_ = VersionType::SUITE_HOME;
    } else {
      version_type_ = VersionType::SUITE_SERVER;
    }
  } else if (version_info.dwMajorVersion == 5 &&
             version_info.dwMinorVersion == 1) {
    if (version_info.wSuiteMask & VER_SUITE_PERSONAL)
      version_type_ = VersionType::SUITE_HOME;
    else
      version_type_ = VersionType::SUITE_PROFESSIONAL;
  } else {
    // Windows is pre XP so we don't care but pick a safe default.
    version_type_ = VersionType::SUITE_HOME;
  }
}

OSInfo::~OSInfo() = default;

Version OSInfo::Kernel32Version() {
  static const Version kernel32_version =
      MajorMinorBuildToVersion(Kernel32BaseVersion().components()[0],
                               Kernel32BaseVersion().components()[1],
                               Kernel32BaseVersion().components()[2]);
  return kernel32_version;
}

OSInfo::VersionNumber OSInfo::Kernel32VersionNumber() {
  CR_DCHECK_EQ(Kernel32BaseVersion().components().size(), 4u);
  static const VersionNumber version = {
      /*.major =*/ Kernel32BaseVersion().components()[0],
      /*.minor =*/ Kernel32BaseVersion().components()[1],
      /*.build =*/ Kernel32BaseVersion().components()[2],
      /*.patch =*/ Kernel32BaseVersion().components()[3]};
  return version;
}

// Retrieve a version from kernel32. This is useful because when running in
// compatibility mode for a down-level version of the OS, the file version of
// kernel32 will still be the "real" version.
cr::Version OSInfo::Kernel32BaseVersion() {
  static cr::Version* version([] {
    // Allow the calls to `Kernel32BaseVersion()` to block, as they only happen
    // once (after which the result is cached in `version`), and reading from
    // kernel32.dll is fast in practice because it is used by all processes and
    // therefore likely to be in the OS's file cache.
    ///cr::ScopedAllowBlocking allow_blocking;
    std::unique_ptr<FileVersionInfoWin> file_version_info =
        FileVersionInfoWin::CreateFileVersionInfoWin(
            FilePath(FILE_PATH_LITERAL("kernel32.dll")));
    if (!file_version_info) {
      // crbug.com/912061: on some systems it seems kernel32.dll might be
      // corrupted or not in a state to get version info. In this case try
      // kernelbase.dll as a fallback.
      file_version_info = FileVersionInfoWin::CreateFileVersionInfoWin(
          FilePath(FILE_PATH_LITERAL("kernelbase.dll")));
    }
    CR_CHECK(file_version_info);
    return new cr::Version(file_version_info->GetFileVersion());
  }());
  return *version;
}

bool OSInfo::IsWowDisabled() const {
  return (wow_process_machine_ == WowProcessMachine::kDisabled);
}

bool OSInfo::IsWowX86OnAMD64() const {
  return (wow_process_machine_ == WowProcessMachine::kX86 &&
          wow_native_machine_ == WowNativeMachine::kAMD64);
}

bool OSInfo::IsWowX86OnARM64() const {
  return (wow_process_machine_ == WowProcessMachine::kX86 &&
          wow_native_machine_ == WowNativeMachine::kARM64);
}

bool OSInfo::IsWowAMD64OnARM64() const {
#if defined(MINI_CHROMIUM_ARCH_CPU_X86_64)
  // An AMD64 process running on an ARM64 device results in the incorrect
  // identification of the device architecture (AMD64 is reported). However,
  // IsWow64Process2 will return the correct device type for the native
  // machine, even though the OS doesn't consider an AMD64 process on an ARM64
  // processor a classic Windows-on-Windows setup.
  return (wow_process_machine_ == WowProcessMachine::kDisabled &&
          wow_native_machine_ == WowNativeMachine::kARM64);
#else
  return false;
#endif
}

bool OSInfo::IsWowX86OnOther() const {
  return (wow_process_machine_ == WowProcessMachine::kX86 &&
          wow_native_machine_ == WowNativeMachine::kOther);
}

std::string OSInfo::processor_model_name() {
  if (processor_model_name_.empty()) {
    const wchar_t kProcessorNameString[] =
        L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0";
    RegKey key(HKEY_LOCAL_MACHINE, kProcessorNameString, KEY_READ);
    std::wstring value;
    key.ReadValue(L"ProcessorNameString", &value);
    processor_model_name_ = WideToUTF8(value);
  }
  return processor_model_name_;
}

bool OSInfo::IsWindowsNSku() const {
  switch (os_type_) {
    case OS_PRODUCT_BUSINESS_N:
    case OS_PRODUCT_CORE_N:
    case OS_PRODUCT_CORE_CONNECTED_N:
    case OS_PRODUCT_EDUCATION_N:
    case OS_PRODUCT_ENTERPRISE_N:
    case OS_PRODUCT_ENTERPRISE_S_N:
    case OS_PRODUCT_ENTERPRISE_SUBSCRIPTION_N:
    case OS_PRODUCT_HOME_BASIC_N:
    case OS_PRODUCT_HOME_PREMIUM_N:
    case OS_PRODUCT_PRO_FOR_EDUCATION_N:
    case OS_PRODUCT_PRO_WORKSTATION_N:
    case OS_PRODUCT_PROFESSIONAL_N:
    case OS_PRODUCT_PROFESSIONAL_S_N:
    case OS_PRODUCT_PROFESSIONAL_STUDENT_N:
    case OS_PRODUCT_STARTER_N:
    case OS_PRODUCT_ULTIMATE_N:
      return true;
    default:
      return false;
  }
}

// With the exception of Server 2003, server variants are treated the same as
// the corresponding workstation release.
// static
Version OSInfo::MajorMinorBuildToVersion(uint32_t major,
                                         uint32_t minor,
                                         uint32_t build) {
  if (major == 11) {
    // We know nothing about this version of Windows or even if it exists.
    // Known Windows 11 versions have a major number 10 and are thus handled by
    // the == 10 block below.
    return Version::WIN11;
  }

  if (major == 10) {
    if (build >= 22631) return Version::WIN11_23H2;
    if (build >= 22621) return Version::WIN11_22H2;
    if (build >= 22000) return Version::WIN11;
    if (build >= 20348) return Version::SERVER_2022;
    if (build >= 19045) return Version::WIN10_22H2;
    if (build >= 19044) return Version::WIN10_21H2;
    if (build >= 19043) return Version::WIN10_21H1;
    if (build >= 19042) return Version::WIN10_20H2;
    if (build >= 19041) return Version::WIN10_20H1;
    if (build >= 18363) return Version::WIN10_19H2;
    if (build >= 18362) return Version::WIN10_19H1;
    if (build >= 17763) return Version::WIN10_RS5;
    if (build >= 17134) return Version::WIN10_RS4;
    if (build >= 16299) return Version::WIN10_RS3;
    if (build >= 15063) return Version::WIN10_RS2;
    if (build >= 14393) return Version::WIN10_RS1;
    if (build >= 10586) return Version::WIN10_TH2;
    return Version::WIN10;
  }

  if (major > 6) {
    // Hitting this likely means that it's time for a >11 block above.
    //LOG(DFATAL) << "Unsupported version: " << major << "." << minor << "."
    //            << build;
    //SCOPED_CRASH_KEY_NUMBER("WindowsVersion", "major", major);
    //SCOPED_CRASH_KEY_NUMBER("WindowsVersion", "minor", minor);
    //SCOPED_CRASH_KEY_NUMBER("WindowsVersion", "build", build);
    //base::debug::DumpWithoutCrashing();
    return Version::WIN_LAST;
  }

  if (major == 6) {
    switch (minor) {
      case 0:
        return Version::VISTA;
      case 1:
        return Version::WIN7;
      case 2:
        return Version::WIN8;
      default:
        CR_DCHECK_EQ(minor, 3u);
        return Version::WIN8_1;
    }
  }

  if (major == 5 && minor != 0) {
    // Treat XP Pro x64, Home Server, and Server 2003 R2 as Server 2003.
    return minor == 1 ? Version::XP : Version::SERVER_2003;
  }

  // Win 2000 or older.
  return Version::PRE_XP;
}

OSInfo::WowProcessMachine OSInfo::GetWowProcessMachineArchitecture(
    const int process_machine) {
  switch (process_machine) {
    case IMAGE_FILE_MACHINE_UNKNOWN:
      return OSInfo::WowProcessMachine::kDisabled;
    case IMAGE_FILE_MACHINE_I386:
      return OSInfo::WowProcessMachine::kX86;
    case IMAGE_FILE_MACHINE_ARM:
    case IMAGE_FILE_MACHINE_THUMB:
    case IMAGE_FILE_MACHINE_ARMNT:
      return OSInfo::WowProcessMachine::kARM32;
  }
  return OSInfo::WowProcessMachine::kOther;
}

OSInfo::WowNativeMachine OSInfo::GetWowNativeMachineArchitecture(
    const int native_machine) {
  switch (native_machine) {
    case IMAGE_FILE_MACHINE_ARM64:
      return OSInfo::WowNativeMachine::kARM64;
    case IMAGE_FILE_MACHINE_AMD64:
      return OSInfo::WowNativeMachine::kAMD64;
  }
  return OSInfo::WowNativeMachine::kOther;
}

void OSInfo::InitializeWowStatusValuesFromLegacyApi(HANDLE process_handle) {
  BOOL is_wow64 = FALSE;
  if (!::IsWow64Process(process_handle, &is_wow64))
    return;
  if (is_wow64) {
    wow_process_machine_ = WowProcessMachine::kX86;
    wow_native_machine_ = WowNativeMachine::kAMD64;
  } else {
    wow_process_machine_ = WowProcessMachine::kDisabled;
  }
}

void OSInfo::InitializeWowStatusValuesForProcess(HANDLE process_handle) {
  static const IsWow64Process2Ptr is_wow64_process2 =
      reinterpret_cast<IsWow64Process2Ptr>(::GetProcAddress(
          ::GetModuleHandleW(L"kernel32.dll"), "IsWow64Process2"));
  if (!is_wow64_process2) {
    InitializeWowStatusValuesFromLegacyApi(process_handle);
    return;
  }

  USHORT process_machine = IMAGE_FILE_MACHINE_UNKNOWN;
  USHORT native_machine = IMAGE_FILE_MACHINE_UNKNOWN;
  if (!is_wow64_process2(process_handle, &process_machine, &native_machine)) {
    return;
  }
  wow_process_machine_ = GetWowProcessMachineArchitecture(process_machine);
  wow_native_machine_ = GetWowNativeMachineArchitecture(native_machine);
}

Version GetVersion() {
  return OSInfo::GetInstance()->version();
}

}  // namespace win
}  // namespace cr
