#ifndef MINI_CHROMIUM_SRC_CRBASE_WIN_NATIVE_INTERNAL_H_
#define MINI_CHROMIUM_SRC_CRBASE_WIN_NATIVE_INTERNAL_H_

#include <windows.h>

#include "crbase/compiler_specific.h"

#if defined(MINI_CHROMIUM_ARCH_CPU_X86)
#define NT_STRUCTURE(name) using name = name##32;
#elif defined(MINI_CHROMIUM_ARCH_CPU_X86_64)
#define NT_STRUCTURE(name) using name = name##64;
#endif

#define NT_STRUCTURE_LIST_ENTRY(T, alignbits)                 \
  struct CR_ALIGNAS(alignbits / 8) NT_LIST_ENTRY##alignbits { \
    T Flink;                                                  \
    T Blink;                                                  \
  }
// - NT_LIST_ENTRY32
NT_STRUCTURE_LIST_ENTRY(DWORD, 32);
// - NT_LIST_ENTRY64
NT_STRUCTURE_LIST_ENTRY(DWORD64, 64);
NT_STRUCTURE(NT_LIST_ENTRY);

#define NT_STRUCTURE_UNICODE_STRING(T, alignbits)                 \
  struct CR_ALIGNAS(alignbits / 8) NT_UNICODE_STRING##alignbits { \
    WORD BufferLength;                                            \
    WORD MaximumBufferLength;                                     \
    T Buffer;                                                     \
  }
// - NT_UNICODE_STRING32
NT_STRUCTURE_UNICODE_STRING(DWORD, 32);
// - NT_UNICODE_STRING64
NT_STRUCTURE_UNICODE_STRING(DWORD64, 64);
NT_STRUCTURE(NT_UNICODE_STRING);

#define NT_STRUCTURE_CURDIR(T, USTR, alignbits)           \
  struct CR_ALIGNAS(alignbits / 8) NT_CURDIR##alignbits { \
    USTR DosPath; /* UNICODE_STRING*/                     \
    T Handle;     /* HANDLE*/                             \
  }
// - NT_CURDIR32
NT_STRUCTURE_CURDIR(DWORD, NT_UNICODE_STRING32, 32);
// - NT_CURDIR64
NT_STRUCTURE_CURDIR(DWORD64, NT_UNICODE_STRING64, 64);
NT_STRUCTURE(NT_CURDIR);

#define NT_STRUCTURE_RTL_DRIVE_LETTER_CURDIR(USTR, alignbits)              \
  struct CR_ALIGNAS(alignbits / 8) NT_RTL_DRIVE_LETTER_CURDIR##alignbits { \
    USHORT Flags;                                                          \
    USHORT Length;                                                         \
    ULONG TimeStamp;                                                       \
    USTR DosPath; /* UNICODE_STRING */                                     \
  }
// - NT_RTL_DRIVE_LETTER_CURDIR32
NT_STRUCTURE_RTL_DRIVE_LETTER_CURDIR(NT_UNICODE_STRING32, 32);
// - NT_RTL_DRIVE_LETTER_CURDIR64
NT_STRUCTURE_RTL_DRIVE_LETTER_CURDIR(NT_UNICODE_STRING64, 64);
NT_STRUCTURE(NT_RTL_DRIVE_LETTER_CURDIR);

#define NT_RTL_MAX_DRIVE_LETTERS 32
#define NT_RTL_DRIVE_LETTER_VALID (USHORT)0x0001

#define NT_STRUCTURE_RTL_USER_PROCESS_PARAMETERS(                              \
    T, USTR, CURDIR, RTL_DRIVE_LETTER_CURDIR, alignbits)                       \
  struct CR_ALIGNAS(alignbits / 8) NT_RTL_USER_PROCESS_PARAMETERS##alignbits { \
    ULONG MaximumLength;                                                       \
    ULONG Length;                                                              \
    ULONG Flags;                                                               \
    ULONG DebugFlags;                                                          \
    T ConsoleHandle; /* HANDLE */                                              \
    ULONG ConsoleFlags;                                                        \
    T StandardInput;         /* HANDLE */                                      \
    T StandardOutput;        /* HANDLE */                                      \
    T StandardError;         /* HANDLE */                                      \
    CURDIR CurrentDirectory; /* CURDIR */                                      \
    USTR DllPath;            /* UNICODE_STRING */                              \
    USTR ImagePathName;      /* UNICODE_STRING */                              \
    USTR CommandLine;        /* UNICODE_STRING */                              \
    T Environment;           /* PVOID */                                       \
    ULONG StartingX;                                                           \
    ULONG StartingY;                                                           \
    ULONG CountX;                                                              \
    ULONG CountY;                                                              \
    ULONG CountCharsX;                                                         \
    ULONG CountCharsY;                                                         \
    ULONG FillAttribute;                                                       \
    ULONG WindowFlags;                                                         \
    ULONG ShowWindowFlags;                                                     \
    USTR WindowTitle; /* UNICODE_STRING */                                     \
    USTR DesktopInfo; /* UNICODE_STRING */                                     \
    USTR ShellInfo;   /* UNICODE_STRING */                                     \
    USTR RuntimeData; /* UNICODE_STRING */                                     \
    RTL_DRIVE_LETTER_CURDIR CurrentDirectories[NT_RTL_MAX_DRIVE_LETTERS];      \
    ULONG EnvironmentSize;                                                     \
    ULONG EnvironmentVersion;                                                  \
  }

// - NT_RTL_USER_PROCESS_PARAMETERS32
NT_STRUCTURE_RTL_USER_PROCESS_PARAMETERS(DWORD, NT_UNICODE_STRING32,
                                         NT_CURDIR32,
                                         NT_RTL_DRIVE_LETTER_CURDIR32, 32);
// - NT_RTL_USER_PROCESS_PARAMETERS64
NT_STRUCTURE_RTL_USER_PROCESS_PARAMETERS(DWORD64, NT_UNICODE_STRING64,
                                         NT_CURDIR64,
                                         NT_RTL_DRIVE_LETTER_CURDIR64, 64);
NT_STRUCTURE(NT_RTL_USER_PROCESS_PARAMETERS);

#define NT_GDI_HANDLE_BUFFER_SIZE32 34
#define NT_GDI_HANDLE_BUFFER_SIZE64 60

#if defined(MINI_CHROMIUM_ARCH_CPU_X86)
#define NT_GDI_HANDLE_BUFFER_SIZE NT_GDI_HANDLE_BUFFER_SIZE32
#elif defined(MINI_CHROMIUM_ARCH_CPU_X86_64)
#define NT_GDI_HANDLE_BUFFER_SIZE NT_GDI_HANDLE_BUFFER_SIZE64
#endif

typedef ULONG NT_GDI_HANDLE_BUFFER32[NT_GDI_HANDLE_BUFFER_SIZE32];
typedef ULONG NT_GDI_HANDLE_BUFFER64[NT_GDI_HANDLE_BUFFER_SIZE64];
typedef ULONG NT_GDI_HANDLE_BUFFER[NT_GDI_HANDLE_BUFFER_SIZE];

#define NT_STRUCTURE_PEB(T, GDI_HANDLE_BUFFER, alignbits)   \
  struct CR_ALIGNAS(alignbits / 8) NT_PEB##alignbits {      \
    BOOLEAN InheritedAddressSpace;                          \
    BOOLEAN ReadImageFileExecOptions;                       \
    BOOLEAN BeingDebugged;                                  \
    union {                                                 \
      BOOLEAN BitField;                                     \
      struct {                                              \
        BOOLEAN ImageUsesLargePages : 1;                    \
        BOOLEAN IsProtectedProcess : 1;                     \
        BOOLEAN IsLegacyProcess : 1;                        \
        BOOLEAN IsImageDynamicallyRelocated : 1;            \
        BOOLEAN SkipPatchingUser32Forwarders : 1;           \
        BOOLEAN SpareBits : 3;                              \
      };                                                    \
    };                                                      \
    T Mutant;            /* HANDLE */                       \
    T ImageBaseAddress;  /* PVOID */                        \
    T Ldr;               /* PPEB_LDR_DATA */                \
    T ProcessParameters; /* PRTL_USER_PROCESS_PARAMETERS */ \
    T SubSystemData;     /* PVOID */                        \
    T ProcessHeap;       /* PVOID */                        \
    T FastPebLock;       /* PRTL_CRITICAL_SECTION */        \
    T AtlThunkSListPtr;  /* PVOID */                        \
    T IFEOKey;           /* PVOID */                        \
    union {                                                 \
      ULONG CrossProcessFlags;                              \
      struct {                                              \
        ULONG ProcessInJob : 1;                             \
        ULONG ProcessInitializing : 1;                      \
        ULONG ProcessUsingVEH : 1;                          \
        ULONG ProcessUsingVCH : 1;                          \
        ULONG ProcessUsingFTH : 1;                          \
        ULONG ReservedBits0 : 27;                           \
      };                                                    \
      ULONG EnvironmentUpdateCount;                         \
    };                                                      \
    union {                                                 \
      T KernelCallbackTable; /* PVOID */                    \
      T UserSharedInfoPtr;   /* PVOID */                    \
    };                                                      \
    ULONG SystemReserved[1];                                \
    ULONG AtlThunkSListPtr32;                               \
    T ApiSetMap; /* PVOID */                                \
    ULONG TlsExpansionCounter;                              \
    T TlsBitmap; /* PVOID */                                \
    ULONG TlsBitmapBits[2];                                 \
    T ReadOnlySharedMemoryBase; /* PVOID */                 \
    T HotpatchInformation;      /* PVOID */                 \
    T ReadOnlyStaticServerData; /* PPVOID */                \
    T AnsiCodePageData;         /* PVOID */                 \
    T OemCodePageData;          /* PVOID */                 \
    T UnicodeCaseTableData;     /* PVOID */                 \
    ULONG NumberOfProcessors;                               \
    ULONG NtGlobalFlag;                                     \
    ULONG CriticalSectionTimeout[2];                        \
    T HeapSegmentReserve;             /* SIZE_T */          \
    T HeapSegmentCommit;              /* SIZE_T */          \
    T HeapDeCommitTotalFreeThreshold; /* SIZE_T */          \
    T HeapDeCommitFreeBlockThreshold; /* SIZE_T */          \
    ULONG NumberOfHeaps;                                    \
    ULONG MaximumNumberOfHeaps;                             \
    T ProcessHeaps;         /* PPVOID */                    \
    T GdiSharedHandleTable; /* PVOID */                     \
    T ProcessStarterHelper; /* PVOID */                     \
    ULONG GdiDCAttributeList;                               \
    T LoaderLock; /* PRTL_CRITICAL_SECTION */               \
    ULONG OSMajorVersion;                                   \
    ULONG OSMinorVersion;                                   \
    USHORT OSBuildNumber;                                   \
    USHORT OSCSDVersion;                                    \
    ULONG OSPlatformId;                                     \
    ULONG ImageSubsystem;                                   \
    ULONG ImageSubsystemMajorVersion;                       \
    ULONG ImageSubsystemMinorVersion;                       \
    T ImageProcessAffinityMask; /* ULONG_PTR */             \
    GDI_HANDLE_BUFFER GdiHandleBuffer;                      \
    T PostProcessInitRoutine; /* PVOID */                   \
    T TlsExpansionBitmap;     /* PVOID */                   \
    ULONG TlsExpansionBitmapBits[32];                       \
    ULONG SessionId;                                        \
  }

// - NT_PEB32
NT_STRUCTURE_PEB(DWORD, NT_GDI_HANDLE_BUFFER32, 32);
// - NT_PEB64
NT_STRUCTURE_PEB(DWORD64, NT_GDI_HANDLE_BUFFER64, 64);
NT_STRUCTURE(NT_PEB)

#define NT_STRUCTURE_PROCESS_BASIC_INFORMATION(T, alignbits)                 \
  struct CR_ALIGNAS(alignbits / 8) NT_PROCESS_BASIC_INFORMATION##alignbits { \
    NTSTATUS ExitStatus;                                                     \
    T PebBaseAddress; /* PVOID */                                            \
    T AffinityMask;                                                          \
    LONG BasePriority;                                                       \
    T UniqueProcessId;                                                       \
    T InheritedFromUniqueProcessId;                                          \
  }
// - NT_PROCESS_BASIC_INFORMATION32
NT_STRUCTURE_PROCESS_BASIC_INFORMATION(DWORD, 32);
// - NT_PROCESS_BASIC_INFORMATION64
NT_STRUCTURE_PROCESS_BASIC_INFORMATION(DWORD64, 64);
NT_STRUCTURE(NT_PROCESS_BASIC_INFORMATION)

#undef NT_STRUCTURE

#endif  // !MINI_CHROMIUM_SRC_CRBASE_WIN_NATIVE_INTERNAL_H_
