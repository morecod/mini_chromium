// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_WIN_NATIVE_API_WRAPPER_H_
#define MINI_CHROMIUM_SRC_CRBASE_WIN_NATIVE_API_WRAPPER_H_

#include <windows.h>

#include "crbase/build_config.h"

typedef LONG KPRIORITY;

typedef enum _NT_SYSTEM_INFORMATION_CLASS {
  kSystemBasicInformation = 0,
  kSystemProcessorInformation = 1,
  kSystemPerformanceInformation = 2,
  kSystemTimeOfDayInformation = 3,
  kSystemPathInformation = 4,
  kSystemProcessInformation = 5,
  kSystemCallCountInformation = 6,
  kSystemDeviceInformation = 7,
  kSystemProcessorPerformanceInformation = 8,
  kSystemFlagsInformation = 9,
  kSystemCallTimeInformation = 10,
  kSystemModuleInformation = 11,
  kSystemLocksInformation = 12,
  kSystemStackTraceInformation = 13,
  kSystemPagedPoolInformation = 14,
  kSystemNonPagedPoolInformation = 15,
  kSystemHandleInformation = 16,
  kSystemObjectInformation = 17,
  kSystemPageFileInformation = 18,
  kSystemVdmInstemulInformation = 19,
  kSystemVdmBopInformation = 20,
  kSystemFileCacheInformation = 21,
  kSystemPoolTagInformation = 22,
  kSystemInterruptInformation = 23,
  kSystemDpcBehaviorInformation = 24,
  kSystemFullMemoryInformation = 25,
  kSystemLoadGdiDriverInformation = 26,
  kSystemUnloadGdiDriverInformation = 27,
  kSystemTimeAdjustmentInformation = 28,
  kSystemSummaryMemoryInformation = 29,
  kSystemMirrorMemoryInformation = 30,
  kSystemPerformanceTraceInformation = 31,
  kSystemObsolete0 = 32,
  kSystemExceptionInformation = 33,
  kSystemCrashDumpStateInformation = 34,
  kSystemKernelDebuggerInformation = 35,
  kSystemContextSwitchInformation = 36,
  kSystemRegistryQuotaInformation = 37,
  kSystemExtendServiceTableInformation = 38,
  kSystemPrioritySeperation = 39,
  kSystemVerifierAddDriverInformation = 40,
  kSystemVerifierRemoveDriverInformation = 41,
  kSystemProcessorIdleInformation = 42,
  kSystemLegacyDriverInformation = 43,
  kSystemCurrentTimeZoneInformation = 44,
  kSystemLookasideInformation = 45,
  kSystemTimeSlipNotification = 46,
  kSystemSessionCreate = 47,
  kSystemSessionDetach = 48,
  kSystemSessionInformation = 49,
  kSystemRangeStartInformation = 50,
  kSystemVerifierInformation = 51,
  kSystemVerifierThunkExtend = 52,
  kSystemSessionProcessInformation = 53,
  kSystemLoadGdiDriverInSystemSpace = 54,
  kSystemNumaProcessorMap = 55,
  kSystemPrefetcherInformation = 56,
  kSystemExtendedProcessInformation = 57,
  kSystemRecommendedSharedDataAlignment = 58,
  kSystemComPlusPackage = 59,
  kSystemNumaAvailableMemory = 60,
  kSystemProcessorPowerInformation = 61,
  kSystemEmulationBasicInformation = 62,
  kSystemEmulationProcessorInformation = 63,
  kSystemExtendedHandleInformation = 64,
  kSystemLostDelayedWriteInformation = 65,
  kSystemBigPoolInformation = 66,
  kSystemSessionPoolTagInformation = 67,
  kSystemSessionMappedViewInformation = 68,
  kSystemHotpatchInformation = 69,
  kSystemObjectSecurityMode = 70,
  kSystemWatchdogTimerHandler = 71,
  kSystemWatchdogTimerInformation = 72,
  kSystemLogicalProcessorInformation = 73,
  kSystemWow64SharedInformation = 74,
  kSystemRegisterFirmwareTableInformationHandler = 75,
  kSystemFirmwareTableInformation = 76,
  kSystemModuleInformationEx = 77,
  kSystemVerifierTriageInformation = 78,
  kSystemSuperfetchInformation = 79,
  kSystemMemoryListInformation = 80,
  kSystemFileCacheInformationEx = 81,
  kMaxSystemInfoClass = 82  // MaxSystemInfoClass should always be the last enum
} NT_SYSTEM_INFORMATION_CLASS;

typedef enum _NT_SYSTEM_FIRMWARE_TABLE_ACTION {
  KSystemFirmwareTable_Enumerate,
  KSystemFirmwareTable_Get
} NT_SYSTEM_FIRMWARE_TABLE_ACTION;

typedef struct _NT_SYSTEM_FIRMWARE_TABLE_INFORMATION {
  ULONG ProviderSignature;
  NT_SYSTEM_FIRMWARE_TABLE_ACTION Action;
  ULONG TableID;
  ULONG TableBufferLength;
  UCHAR TableBuffer[1];
} NT_SYSTEM_FIRMWARE_TABLE_INFORMATION, *PNT_SYSTEM_FIRMWARE_TABLE_INFORMATION;

NTSTATUS NTAPI NtQuerySystemInformation(
  /* [in]            */ NT_SYSTEM_INFORMATION_CLASS SystemInformationClass,
  /* [in, out]       */ PVOID                       SystemInformation,
  /* [in]            */ ULONG                       SystemInformationLength,
  /* [out, optional] */ PULONG                      ReturnLength
);

typedef enum _NT_PROCESS_INFORMATION_CLASS {
  kProcessBasicInformation,  // 0, q: PROCESS_BASIC_INFORMATION, PROCESS_EXTENDED_BASIC_INFORMATION
  kProcessQuotaLimits,       // qs: QUOTA_LIMITS, QUOTA_LIMITS_EX
  kProcessIoCounters,        // q: IO_COUNTERS
  kProcessVmCounters,        // q: VM_COUNTERS, VM_COUNTERS_EX, VM_COUNTERS_EX2
  kProcessTimes,             // q: KERNEL_USER_TIMES
  kProcessBasePriority,      // s: KPRIORITY
  kProcessRaisePriority,     // s: ULONG
  kProcessDebugPort,         // q: HANDLE
  kProcessExceptionPort,     // s: HANDLE
  kProcessAccessToken,       // s: PROCESS_ACCESS_TOKEN
  kProcessLdtInformation,    // 10, qs: PROCESS_LDT_INFORMATION
  kProcessLdtSize,           // s: PROCESS_LDT_SIZE
  kProcessDefaultHardErrorMode,  // qs: ULONG
  kProcessIoPortHandlers,        // (kernel-mode only)
  kProcessPooledUsageAndLimits,  // q: POOLED_USAGE_AND_LIMITS
  kProcessWorkingSetWatch,       // q: PROCESS_WS_WATCH_INFORMATION[]; s: void
  kProcessUserModeIOPL,
  kProcessEnableAlignmentFaultFixup,  // s: BOOLEAN
  kProcessPriorityClass,              // qs: PROCESS_PRIORITY_CLASS
  kProcessWx86Information,
  kProcessHandleCount,            // 20, q: ULONG, PROCESS_HANDLE_INFORMATION
  kProcessAffinityMask,           // s: KAFFINITY
  kProcessPriorityBoost,          // qs: ULONG
  kProcessDeviceMap,              // qs: PROCESS_DEVICEMAP_INFORMATION, PROCESS_DEVICEMAP_INFORMATION_EX
  kProcessSessionInformation,     // q: PROCESS_SESSION_INFORMATION
  kProcessForegroundInformation,  // s: PROCESS_FOREGROUND_BACKGROUND
  kProcessWow64Information,       // q: ULONG_PTR
  kProcessImageFileName,          // q: UNICODE_STRING
  kProcessLUIDDeviceMapsEnabled,  // q: ULONG
  kProcessBreakOnTermination,     // qs: ULONG
  kProcessDebugObjectHandle,      // 30, q: HANDLE
  kProcessDebugFlags,             // qs: ULONG
  kProcessHandleTracing,  // q: PROCESS_HANDLE_TRACING_QUERY; s: size 0 disables,  otherwise enables  -- XP SP1 end
  kProcessIoPriority,     // qs: ULONG -- visata start
  kProcessExecuteFlags,   // qs: ULONG
  kProcessResourceManagement,
  kProcessCookie,            // q: ULONG
  kProcessImageInformation,  // q: SECTION_IMAGE_INFORMATION
  kProcessCycleTime,         // q: PROCESS_CYCLE_TIME_INFORMATION // since VISTA
  kProcessPagePriority,      // q: ULONG
  kProcessInstrumentationCallback,  // 40
  kProcessThreadStackAllocation,    // s: PROCESS_STACK_ALLOCATION_INFORMATION, PROCESS_STACK_ALLOCATION_INFORMATION_EX
  kProcessWorkingSetWatchEx,        // q: PROCESS_WS_WATCH_INFORMATION_EX[]
  kProcessImageFileNameWin32,       // q: UNICODE_STRING
  kProcessImageFileMapping,         // q: HANDLE (input)
  kProcessAffinityUpdateMode,       // qs: PROCESS_AFFINITY_UPDATE_MODE
  kProcessMemoryAllocationMode,     // qs: PROCESS_MEMORY_ALLOCATION_MODE
  kProcessGroupInformation,         // q: USHORT[]
  kProcessTokenVirtualizationEnabled,  // s: ULONG
  kProcessConsoleHostProcess,          // q: ULONG_PTR
  kProcessWindowInformation,           // 50, q: PROCESS_WINDOW_INFORMATION
  kProcessHandleInformation,  // q: PROCESS_HANDLE_SNAPSHOT_INFORMATION since WIN8
  kProcessMitigationPolicy,   // s: PROCESS_MITIGATION_POLICY_INFORMATION
  kProcessDynamicFunctionTableInformation,
  kProcessHandleCheckingMode,
  kProcessKeepAliveCount,     // q: PROCESS_KEEPALIVE_COUNT_INFORMATION
  kProcessRevokeFileHandles,  // s: PROCESS_REVOKE_FILE_HANDLES_INFORMATION
  kProcessWorkingSetControl,  // s: PROCESS_WORKING_SET_CONTROL
  kProcessHandleTable,        // since WINBLUE
  kProcessCheckStackExtentsMode,
  kProcessCommandLineInformation,  // 60, q: UNICODE_STRING
  kProcessProtectionInformation,   // q: PS_PROTECTION
  kProcessMemoryExhaustion,  // PROCESS_MEMORY_EXHAUSTION_INFO // since THRESHOLD
  kProcessFaultInformation,  // PROCESS_FAULT_INFORMATION
  kProcessTelemetryIdInformation,    // PROCESS_TELEMETRY_ID_INFORMATION
  kProcessCommitReleaseInformation,  // PROCESS_COMMIT_RELEASE_INFORMATION
  kProcessDefaultCpuSetsInformation,
  kProcessAllowedCpuSetsInformation,
  kProcessReserved1Information,
  kProcessReserved2Information,
  kProcessSubsystemProcess,      // 70
  kProcessJobMemoryInformation,  // PROCESS_JOB_MEMORY_INFO
  kMaxProcessInfoClass
} NT_PROCESS_INFORMATION_CLASS, *PNT_PROCESS_INFORMATION_CLASS;

NTSTATUS NTAPI NtQueryProcessInformation(
  /* [in]            */ HANDLE                    ProcessHandle,
  /* [in]            */ NT_PROCESS_INFORMATION_CLASS ProcessInformationClass,
  /* [out]           */ PVOID                     ProcessInformation,
  /* [in]            */ ULONG                     ProcessInformationLength,
  /* [out, optional] */ PULONG                    ReturnLength
);

NTSTATUS NTAPI NtQueryProcessInformationWow64(
  /* [in]            */ HANDLE                    ProcessHandle,
  /* [in]            */ NT_PROCESS_INFORMATION_CLASS ProcessInformationClass,
  /* [out]           */ PVOID                     ProcessInformation,
  /* [in]            */ ULONG                     ProcessInformationLength,
  /* [out, optional] */ PULONG                    ReturnLength
);

NTSTATUS NTAPI NtReadVirtualMemory(
  /* [in]  */ HANDLE   ProcessHandle,
  /* [in]  */ PVOID  BaseAddress,
  /* [out] */ PVOID    Buffer,
  /* [in]  */ SIZE_T  Size,
  /* [out] */ PSIZE_T NumberOfBytesRead);

///
// NOTE:only used in wow64 process.
///
NTSTATUS NTAPI NtReadVirtualMemoryWow64(
  /* [in]  */ HANDLE   ProcessHandle,
  /* [in]  */ ULONG64  BaseAddress,
  /* [out] */ PVOID    Buffer,
  /* [in]  */ ULONG64  Size,
  /* [out] */ PULONG64 NumberOfBytesRead);


#endif  // MINI_CHROMIUM_SRC_CRBASE_WIN_NATIVE_API_WRAPPER_H_