#pragma once

#define PFN_TO_PAGE(pfn) (pfn << PAGE_SHIFT)
#define PAGE_TO_PFN(pfn) (pfn >> PAGE_SHIFT)

#pragma warning(push)
#pragma warning(disable : 4201) // nonstandard extension used: nameless struct/union

#pragma pack(push, 1)
typedef union CR3_
{
	ULONG64 Value;
	struct
	{
		ULONG64 Ignored1 : 3;
		ULONG64 WriteThrough : 1;
		ULONG64 CacheDisable : 1;
		ULONG64 Ignored2 : 7;
		ULONG64 Pml4 : 40;
		ULONG64 Reserved : 12;
	};
} PTE_CR3;

typedef union VIRT_ADDR_
{
	ULONG64 Value;
	void* Pointer;
	struct
	{
		ULONG64 Offset : 12;
		ULONG64 PtIndex : 9;
		ULONG64 PdIndex : 9;
		ULONG64 PdptIndex : 9;
		ULONG64 Pml4Index : 9;
		ULONG64 Reserved : 16;
	};
} VIRTUAL_ADDRESS;

typedef union PML4E_
{
	ULONG64 Value;
	struct
	{
		ULONG64 Present : 1;
		ULONG64 Rw : 1;
		ULONG64 User : 1;
		ULONG64 WriteThrough : 1;
		ULONG64 CacheDisable : 1;
		ULONG64 Accessed : 1;
		ULONG64 Ignored1 : 1;
		ULONG64 Reserved1 : 1;
		ULONG64 Ignored2 : 4;
		ULONG64 Pdpt : 40;
		ULONG64 Ignored3 : 11;
		ULONG64 Xd : 1;
	};
} PML4E;

typedef union PDPTE_
{
	ULONG64 Value;
	struct
	{
		ULONG64 Present : 1;
		ULONG64 Rw : 1;
		ULONG64 User : 1;
		ULONG64 WriteThrough : 1;
		ULONG64 CacheDisable : 1;
		ULONG64 Accessed : 1;
		ULONG64 Dirty : 1;
		ULONG64 PageSize : 1;
		ULONG64 Ignored2 : 4;
		ULONG64 Pd : 40;
		ULONG64 Ignored3 : 11;
		ULONG64 Xd : 1;
	};
} PDPTE;

typedef union PDE_
{
	ULONG64 Value;
	struct
	{
		ULONG64 Present : 1;
		ULONG64 Rw : 1;
		ULONG64 User : 1;
		ULONG64 WriteThrough : 1;
		ULONG64 CacheDisable : 1;
		ULONG64 Accessed : 1;
		ULONG64 Dirty : 1;
		ULONG64 PageSize : 1;
		ULONG64 Ignored2 : 4;
		ULONG64 Pt : 40;
		ULONG64 Ignored3 : 11;
		ULONG64 Xd : 1;
	};
} PDE;

typedef union PTE_
{
	ULONG64 Value;
	VIRTUAL_ADDRESS VirtualAddress;
	struct
	{
		ULONG64 Present : 1;
		ULONG64 Rw : 1;
		ULONG64 User : 1;
		ULONG64 WriteThrough : 1;
		ULONG64 CacheDisable : 1;
		ULONG64 Accessed : 1;
		ULONG64 Dirty : 1;
		ULONG64 Pat : 1;
		ULONG64 Global : 1;
		ULONG64 Ignored1 : 3;
		ULONG64 PageFrame : 40;
		ULONG64 Ignored3 : 11;
		ULONG64 Xd : 1;
	};
} PTE;
#pragma pack(pop)

#pragma warning(pop)

typedef struct _RTL_PROCESS_MODULE_INFORMATION
{
	HANDLE Section;
	PVOID MappedBase;
	PVOID ImageBase;
	ULONG ImageSize;
	ULONG Flags;
	USHORT LoadOrderIndex;
	USHORT InitOrderIndex;
	USHORT LoadCount;
	USHORT OffsetToFileName;
	UCHAR FullPathName[256];
} RTL_PROCESS_MODULE_INFORMATION, * PRTL_PROCESS_MODULE_INFORMATION;

typedef struct _RTL_PROCESS_MODULES
{
	ULONG NumberOfModules;
	RTL_PROCESS_MODULE_INFORMATION Modules[1];
} RTL_PROCESS_MODULES, * PRTL_PROCESS_MODULES;

typedef struct _POOL_TRACKER_BIG_PAGES
{
	volatile ULONGLONG Va;
	ULONG Key;
	ULONG Pattern : 8;
	ULONG PoolType : 12;
	ULONG SlushSize : 12;
	ULONGLONG NumberOfBytes;
} POOL_TRACKER_BIG_PAGES, * PPOOL_TRACKER_BIG_PAGES;

typedef struct _LDR_DATA_TABLE_ENTRY
{
	LIST_ENTRY InLoadOrderLinks;
	LIST_ENTRY InMemoryOrderLinks;
	LIST_ENTRY InInitializationOrderLinks;
	PVOID DllBase;
	PVOID EntryPoint;
	ULONG SizeOfImage;
	UNICODE_STRING FullDllName;
	UNICODE_STRING BaseDllName;
	ULONG Flags;
	USHORT LoadCount;
	USHORT TlsIndex;
	LIST_ENTRY HashLinks;
	ULONG TimeDateStamp;
} LDR_DATA_TABLE_ENTRY, * PLDR_DATA_TABLE_ENTRY;

typedef struct _PEB_LDR_DATA
{
	ULONG Length;
	UCHAR Initialized;
	PVOID SsHandle;
	LIST_ENTRY InLoadOrderModuleList;
	LIST_ENTRY InMemoryOrderModuleList;
	LIST_ENTRY InInitializationOrderModuleList;
} PEB_LDR_DATA, * PPEB_LDR_DATA;

typedef struct _PEB
{
	UCHAR InheritedAddressSpace;
	UCHAR ReadImageFileExecOptions;
	UCHAR BeingDebugged;
	UCHAR BitField;
	PVOID Mutant;
	PVOID ImageBaseAddress;
	PPEB_LDR_DATA Ldr;
	PVOID ProcessParameters;
	PVOID SubSystemData;
	PVOID ProcessHeap;
	PVOID FastPebLock;
	PVOID AtlThunkSListPtr;
	PVOID IFEOKey;
	PVOID CrossProcessFlags;
	PVOID KernelCallbackTable;
	ULONG SystemReserved;
	ULONG AtlThunkSListPtr32;
	PVOID ApiSetMap;
} PEB, * PPEB;

EXTERN_C NTKERNELAPI PPEB PsGetProcessPeb(PEPROCESS process);

#define Log(x, ...) DbgPrintEx(0, 0, "[DPM] " x "\n", __VA_ARGS__)