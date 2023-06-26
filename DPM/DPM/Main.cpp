#include "General.h"

#define OFFSET_ActiveProcessLinks 0x448
PEPROCESS FindProcess(const wchar_t* executableName, PVOID* mainModuleBaseAddress)
{
	PEPROCESS currentProcess = IoGetCurrentProcess();
	PLIST_ENTRY list = reinterpret_cast<PLIST_ENTRY>(reinterpret_cast<PCHAR>(currentProcess) + OFFSET_ActiveProcessLinks);

	for (; list->Flink != reinterpret_cast<PLIST_ENTRY>(reinterpret_cast<PCHAR>(currentProcess) + OFFSET_ActiveProcessLinks); list = list->Flink)
	{
		PEPROCESS targetProcess = reinterpret_cast<PEPROCESS>(reinterpret_cast<PCHAR>(list->Flink) - OFFSET_ActiveProcessLinks);
		PPEB pebAddress = PsGetProcessPeb(targetProcess);
		if (!pebAddress)
			continue;

		PEB pebLocal = { 0 };
		Memory::CopyProcessMemory(targetProcess, pebAddress, currentProcess, &pebLocal, sizeof(PEB));

		PEB_LDR_DATA loaderData = { 0 };
		Memory::CopyProcessMemory(targetProcess, pebLocal.Ldr, currentProcess, &loaderData, sizeof(PEB_LDR_DATA));

		PLIST_ENTRY currentListEntry = loaderData.InMemoryOrderModuleList.Flink;
		while (true)
		{
			LIST_ENTRY listEntryLocal = { 0 };
			Memory::CopyProcessMemory(targetProcess, currentListEntry, currentProcess, &listEntryLocal, sizeof(LIST_ENTRY));
			if (loaderData.InMemoryOrderModuleList.Flink == listEntryLocal.Flink || !listEntryLocal.Flink)
				break;

			PLDR_DATA_TABLE_ENTRY entryAddress = CONTAINING_RECORD(currentListEntry, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);

			LDR_DATA_TABLE_ENTRY entryLocal = { 0 };
			Memory::CopyProcessMemory(targetProcess, entryAddress, currentProcess, &entryLocal, sizeof(LDR_DATA_TABLE_ENTRY));

			UNICODE_STRING modulePathStr = { 0 };
			wchar_t moduleName[256];
			Memory::CopyProcessMemory(targetProcess, entryLocal.BaseDllName.Buffer, currentProcess, moduleName, min(entryLocal.BaseDllName.Length, 256));

			modulePathStr.Buffer = moduleName;
			modulePathStr.Length = min(entryLocal.BaseDllName.Length, 256);
			modulePathStr.MaximumLength = min(entryLocal.BaseDllName.MaximumLength, 256);

			UNICODE_STRING moduleNameStr = { 0 };
			RtlInitUnicodeString(&moduleNameStr, executableName);

			LONG compare = RtlCompareUnicodeString(&modulePathStr, &moduleNameStr, TRUE);
			if (compare == 0)
			{
				*mainModuleBaseAddress = entryLocal.DllBase;
				return targetProcess;
			}

			currentListEntry = listEntryLocal.Flink;
		}
	}

	return nullptr;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driverObject, PUNICODE_STRING registryPath)
{
	UNREFERENCED_PARAMETER(driverObject);
	UNREFERENCED_PARAMETER(registryPath);

	Log("Entry called from 0x%p in context of process with PID %u", _ReturnAddress(), PsGetCurrentProcessId());

	NTSTATUS status = Memory::Init();
	if (!NT_SUCCESS(status))
		return status;

	PVOID moduleBase = NULL;
	PEPROCESS targetProcess = FindProcess(L"ProcessHacker.exe", &moduleBase);
	if (!targetProcess || !moduleBase)
		return STATUS_NOT_FOUND;

	HANDLE processId = PsGetProcessId(targetProcess);
	Log("ProcessHacker.exe has PID of %u with base at 0x%p", processId, moduleBase);

	ULONG64 header = 0;
	Memory::CopyProcessMemory(targetProcess, moduleBase, IoGetCurrentProcess(), &header, sizeof(ULONG64));

	Log("Main module header value is 0x%p", header);

	return STATUS_SUCCESS;
}