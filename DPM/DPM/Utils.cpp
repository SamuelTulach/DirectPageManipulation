#include "General.h"

PVOID Utils::PhysicalToVirtual(ULONG64 address)
{
	PHYSICAL_ADDRESS physical;
	physical.QuadPart = address;
	return MmGetVirtualForPhysical(physical);
}