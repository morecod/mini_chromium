#include "crbase/win/native_api_wrapper.h"

#include "crbase/codes/base64.h"
#include "crbase/build_config.h"
#include "crbase/win/native_internal.h"
#include "crbase/win/windows_version.h"

namespace {

HMODULE GetNativeModule() {
  static HMODULE ntdll_ = nullptr;
  if (ntdll_ == nullptr) {
    const char dll[] = "ntdll.dll";
    ntdll_ = GetModuleHandleA(dll);
  }
  return ntdll_;
}

template<typename FuncType>
FuncType GetNativeFunction(const char* name) {
  std::string api_name;
  if (!cr::Base64Decode(name, &api_name))
    return nullptr;

  FuncType fun = reinterpret_cast<FuncType>(
      GetProcAddress(GetNativeModule(), api_name.data()));
  /// memset(&api_name[0], 0, api_name.length());
  return fun;
}

}  // namespace

NTSTATUS NTAPI NtQuerySystemInformation(
    /* [in]            */ NT_SYSTEM_INFORMATION_CLASS SystemInformationClass,
    /* [in, out]       */ PVOID SystemInformation,
    /* [in]            */ ULONG SystemInformationLength,
    /* [out, optional] */ PULONG ReturnLength) {
  typedef NTSTATUS(NTAPI * FunctionType)(
      NT_SYSTEM_INFORMATION_CLASS, PVOID, ULONG, PULONG);
  const char api[] = "TnRRdWVyeVN5c3RlbUluZm9ybWF0aW9u";  // "NtQuerySystemInformation";

  static FunctionType func = nullptr;
  if (func == nullptr)
    func = GetNativeFunction<FunctionType>(api);

  if (func == nullptr) {
    return E_FAIL;
  }
  return func(SystemInformationClass, SystemInformation,
              SystemInformationLength, ReturnLength);
}

NTSTATUS NTAPI NtQueryProcessInformation(
    /* [in]            */ HANDLE ProcessHandle,
    /* [in]            */ NT_PROCESS_INFORMATION_CLASS ProcessInformationClass,
    /* [out]           */ PVOID ProcessInformation,
    /* [in]            */ ULONG ProcessInformationLength,
    /* [out, optional] */ PULONG ReturnLength) {
  typedef NTSTATUS(NTAPI * FunctionType)(
      HANDLE, NT_PROCESS_INFORMATION_CLASS, PVOID, ULONG, PULONG);
  const char api[] = "TnRRdWVyeUluZm9ybWF0aW9uUHJvY2Vzcw==";  // "NtQueryInformationProcess";

  static FunctionType func = nullptr;
  if (func == nullptr)
    func = GetNativeFunction<FunctionType>(api);

  if (func == nullptr)
    return E_FAIL;

  return func(ProcessHandle, ProcessInformationClass, ProcessInformation,
              ProcessInformationLength, ReturnLength);
}

NTSTATUS NTAPI NtQueryProcessInformationWow64(
    /* [in]            */ HANDLE ProcessHandle,
    /* [in]            */ NT_PROCESS_INFORMATION_CLASS ProcessInformationClass,
    /* [out]           */ PVOID ProcessInformation,
    /* [in]            */ ULONG ProcessInformationLength,
    /* [out, optional] */ PULONG ReturnLength) {
  typedef NTSTATUS(NTAPI * FunctionType)(
      HANDLE, NT_PROCESS_INFORMATION_CLASS, PVOID, ULONG, PULONG);
  const char api[] = "TnRXb3c2NFF1ZXJ5SW5mb3JtYXRpb25Qcm9jZXNzNjQ=";  // "NtWow64QueryInformationProcess64";

  static FunctionType func = nullptr;
  if (func == nullptr)
    func = GetNativeFunction<FunctionType>(api);

  if (func == nullptr)
    return E_FAIL;

  return func(ProcessHandle, ProcessInformationClass, ProcessInformation,
              ProcessInformationLength, ReturnLength);
}

NTSTATUS NTAPI NtReadVirtualMemory(/* [in]  */ HANDLE   ProcessHandle,
                                   /* [in]  */ PVOID  BaseAddress,
                                   /* [out] */ PVOID    Buffer,
                                   /* [in]  */ SIZE_T  Size,
                                   /* [out] */ PSIZE_T NumberOfBytesRead) {
  typedef NTSTATUS(NTAPI * FunctionType)(HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);

  const char api[] = "TnRSZWFkVmlydHVhbE1lbW9yeQ==";  // "NtReadVirtualMemory";
  static FunctionType func = nullptr;
  if (func == nullptr) 
    func = GetNativeFunction<FunctionType>(api);

  if (func == nullptr)
    return E_FAIL;

  return func(ProcessHandle, BaseAddress, Buffer, Size, NumberOfBytesRead);
}

NTSTATUS NTAPI NtReadVirtualMemoryWow64(/* [in]  */ HANDLE   ProcessHandle,
                                        /* [in]  */ ULONG64  BaseAddress,
                                        /* [out] */ PVOID    Buffer,
                                        /* [in]  */ ULONG64  Size,
                                        /* [out] */ PULONG64 NumberOfBytesRead) {
  typedef NTSTATUS(NTAPI * FunctionType)(
      HANDLE, ULONG64, PVOID, ULONG64, PULONG64);

  const char api[] = "TnRXb3c2NFJlYWRWaXJ0dWFsTWVtb3J5NjQ=";  // "NtWow64ReadVirtualMemory64";
  static FunctionType func = nullptr;
  if (func == nullptr) func = GetNativeFunction<FunctionType>(api);

  if (func == nullptr) return E_FAIL;

  return func(ProcessHandle, BaseAddress, Buffer, Size, NumberOfBytesRead);
}
