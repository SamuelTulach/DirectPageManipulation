#include "General.h"

#pragma warning(disable : 4996) // ExAllocatePool is deprecated

NTSTATUS Memory::Init()
{
	PHYSICAL_ADDRESS maxAddress;
	maxAddress.QuadPart = MAXULONG64;

	MainVirtualAddress = MmAllocateContiguousMemory(PAGE_SIZE, maxAddress);
	if (!MainVirtualAddress)
		return STATUS_INSUFFICIENT_RESOURCES;

	VIRTUAL_ADDRESS virtualAddress;
	virtualAddress.Pointer = MainVirtualAddress;

	PTE_CR3 cr3;
	cr3.Value = __readcr3();

	PML4E* pml4 = static_cast<PML4E*>(Utils::PhysicalToVirtual(PFN_TO_PAGE(cr3.Pml4)));
	PML4E* pml4e = (pml4 + virtualAddress.Pml4Index);
	if (!pml4e->Present)
		return STATUS_NOT_FOUND;

	PDPTE* pdpt = static_cast<PDPTE*>(Utils::PhysicalToVirtual(PFN_TO_PAGE(pml4e->Pdpt)));
	PDPTE* pdpte = pdpte = (pdpt + virtualAddress.PdptIndex);
	if (!pdpte->Present)
		return STATUS_NOT_FOUND;

	// sanity check 1GB page
	if (pdpte->PageSize)
		return STATUS_INVALID_PARAMETER;

	PDE* pd = static_cast<PDE*>(Utils::PhysicalToVirtual(PFN_TO_PAGE(pdpte->Pd)));
	PDE* pde = pde = (pd + virtualAddress.PdIndex);
	if (!pde->Present)
		return STATUS_NOT_FOUND;

	// sanity check 2MB page
	if (pde->PageSize)
		return STATUS_INVALID_PARAMETER;

	PTE* pt = static_cast<PTE*>(Utils::PhysicalToVirtual(PFN_TO_PAGE(pde->Pt)));
	PTE* pte = (pt + virtualAddress.PtIndex);
	if (!pte->Present)
		return STATUS_NOT_FOUND;

	MainPageEntry = pte;

	return STATUS_SUCCESS;
}

PVOID Memory::OverwritePage(ULONG64 physicalAddress)
{
	// page boundary checks are done by Read/WriteProcessMemory
	// and page entries are not spread over different pages
	ULONG pageOffset = physicalAddress % PAGE_SIZE;
	ULONG64 pageStartPhysical = physicalAddress - pageOffset;
	MainPageEntry->PageFrame = PAGE_TO_PFN(pageStartPhysical);
	__invlpg(MainVirtualAddress);
	return (PVOID)((ULONG64)MainVirtualAddress + pageOffset);
}

NTSTATUS Memory::ReadPhysicalAddress(ULONG64 targetAddress, PVOID buffer, SIZE_T size)
{
	PVOID virtualAddress = OverwritePage(targetAddress);
	memcpy(buffer, virtualAddress, size);
	return STATUS_SUCCESS;
}

NTSTATUS Memory::WritePhysicalAddress(ULONG64 targetAddress, PVOID buffer, SIZE_T size)
{
	PVOID virtualAddress = OverwritePage(targetAddress);
	memcpy(virtualAddress, buffer, size);
	return STATUS_SUCCESS;
}

#define PAGE_OFFSET_SIZE 12
static const ULONG64 PMASK = (~0xfull << 8) & 0xfffffffffull;
ULONG64 Memory::TranslateLinearAddress(ULONG64 directoryTableBase, ULONG64 virtualAddress)
{
	directoryTableBase &= ~0xf;

	ULONG64 pageOffset = virtualAddress & ~(~0ul << PAGE_OFFSET_SIZE);
	ULONG64 pte = ((virtualAddress >> 12) & (0x1ffll));
	ULONG64 pt = ((virtualAddress >> 21) & (0x1ffll));
	ULONG64 pd = ((virtualAddress >> 30) & (0x1ffll));
	ULONG64 pdp = ((virtualAddress >> 39) & (0x1ffll));

	ULONG64 pdpe = 0;
	ReadPhysicalAddress(directoryTableBase + 8 * pdp, &pdpe, sizeof(pdpe));
	if (~pdpe & 1)
		return 0;

	ULONG64 pde = 0;
	ReadPhysicalAddress((pdpe & PMASK) + 8 * pd, &pde, sizeof(pde));
	if (~pde & 1)
		return 0;

	// 1GB large page, use pde's 12-34 bits
	if (pde & 0x80)
		return (pde & (~0ull << 42 >> 12)) + (virtualAddress & ~(~0ull << 30));

	ULONG64 pteAddr = 0;
	ReadPhysicalAddress((pde & PMASK) + 8 * pt, &pteAddr, sizeof(pteAddr));
	if (~pteAddr & 1)
		return 0;

	// 2MB large page
	if (pteAddr & 0x80)
		return (pteAddr & PMASK) + (virtualAddress & ~(~0ull << 21));

	virtualAddress = 0;
	ReadPhysicalAddress((pteAddr & PMASK) + 8 * pte, &virtualAddress, sizeof(virtualAddress));
	virtualAddress &= PMASK;

	if (!virtualAddress)
		return 0;

	return virtualAddress + pageOffset;
}

ULONG64 Memory::GetProcessDirectoryBase(PEPROCESS inputProcess)
{
	UCHAR* process = reinterpret_cast<UCHAR*>(inputProcess);
	ULONG64 dirbase = *reinterpret_cast<ULONG64*>(process + 0x28);
	if (!dirbase)
	{
		ULONG64 userDirbase = *reinterpret_cast<ULONG64*>(process + 0x388);
		return userDirbase;
	}
	return dirbase;
}

NTSTATUS Memory::ReadProcessMemory(PEPROCESS process, ULONG64 address, PVOID buffer, SIZE_T size)
{
	if (!address)
		return STATUS_INVALID_PARAMETER;

	NTSTATUS status = STATUS_UNSUCCESSFUL;
	ULONG64 processDirbase = GetProcessDirectoryBase(process);
	SIZE_T currentOffset = 0;
	SIZE_T totalSize = size;
	while (totalSize)
	{
		ULONG64 currentPhysicalAddress = TranslateLinearAddress(processDirbase, address + currentOffset);
		if (!currentPhysicalAddress)
			return STATUS_NOT_FOUND;

		ULONG64 readSize = min(PAGE_SIZE - (currentPhysicalAddress & 0xFFF), totalSize);

		status = ReadPhysicalAddress(currentPhysicalAddress, reinterpret_cast<PVOID>(reinterpret_cast<ULONG64>(buffer) + currentOffset), readSize);

		totalSize -= readSize;
		currentOffset += readSize;

		if (!NT_SUCCESS(status))
			break;

		if (!readSize)
			break;
	}

	return status;
}

NTSTATUS Memory::WriteProcessMemory(PEPROCESS process, ULONG64 address, PVOID buffer, SIZE_T size)
{
	if (!address)
		return STATUS_INVALID_PARAMETER;

	NTSTATUS status = STATUS_UNSUCCESSFUL;
	ULONG64 processDirbase = GetProcessDirectoryBase(process);
	SIZE_T currentOffset = 0;
	SIZE_T totalSize = size;
	while (totalSize)
	{
		ULONG64 currentPhysicalAddress = TranslateLinearAddress(processDirbase, address + currentOffset);
		if (!currentPhysicalAddress)
			return STATUS_NOT_FOUND;

		ULONG64 writeSize = min(PAGE_SIZE - (currentPhysicalAddress & 0xFFF), totalSize);

		status = WritePhysicalAddress(currentPhysicalAddress, reinterpret_cast<PVOID>(reinterpret_cast<ULONG64>(buffer) + currentOffset), writeSize);

		totalSize -= writeSize;
		currentOffset += writeSize;

		if (!NT_SUCCESS(status))
			break;

		if (!writeSize)
			break;
	}

	return status;
}

NTSTATUS Memory::CopyProcessMemory(PEPROCESS sourceProcess, PVOID sourceAddress, PEPROCESS targetProcess, PVOID targetAddress, SIZE_T bufferSize)
{
	PVOID temporaryBuffer = ExAllocatePool(NonPagedPoolNx, bufferSize);
	if (!temporaryBuffer)
		return STATUS_INSUFFICIENT_RESOURCES;

	NTSTATUS status = ReadProcessMemory(sourceProcess, reinterpret_cast<ULONG64>(sourceAddress), temporaryBuffer, bufferSize);
	if (!NT_SUCCESS(status))
		goto Exit;

	status = WriteProcessMemory(targetProcess, reinterpret_cast<ULONG64>(targetAddress), temporaryBuffer, bufferSize);

Exit:
	ExFreePool(temporaryBuffer);
	return status;
}