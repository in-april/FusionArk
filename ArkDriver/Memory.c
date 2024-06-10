#include "Memory.h"

ULONG64 g_PML4Base = 0;
ULONG64 g_PDPTBase = 0;
ULONG64 g_PDBase = 0;
ULONG64 g_PTBase = 0;

LONG64 InitPageBase()
{
	PHYSICAL_ADDRESS pCr3 = { 0 };
	pCr3.QuadPart = __readcr3();
	PULONG64 pCmpArr = MmGetVirtualForPhysical(pCr3);

	int count = 0;
	while ((*pCmpArr & 0xFFFFFFFFF000) != pCr3.QuadPart)
	{
		if (++count >= 512)
		{
			return -1;
		}
		pCmpArr++;
	}
	g_PML4Base = (ULONG64)pCmpArr & 0xFFFFFFFFFFFFF000;
	if (g_PML4Base != 0)
	{
		g_PDPTBase = (g_PML4Base >> 21) << 21;
		g_PDBase = (g_PML4Base >> 30) << 30;
		g_PTBase = (g_PML4Base >> 39) << 39;
	}
	if (!g_PML4Base || !g_PDPTBase || !g_PDBase || !g_PTBase)
	{
		return -1;
	}
	return 0;
}

ULONG64 GetPTE(ULONG64 VirtualAddress)
{
	return ((VirtualAddress >> 9) & 0x7FFFFFFFF8) + g_PTBase;
}

ULONG64 GetPDE(ULONG64 VirtualAddress)
{
	return ((VirtualAddress >> 18) & 0x3FFFFFF8) + g_PDBase;
}

ULONG64 GetPDPTE(ULONG64 VirtualAddress)
{
	return ((VirtualAddress >> 27) & 0x1FFFF8) + g_PDPTBase;
}

ULONG64 GetPML4E(ULONG64 VirtualAddress)
{
	return ((VirtualAddress >> 39) & 0x1FF) + g_PML4Base;
}

BOOLEAN SetExecutePage(ULONG64 VirtualAddress, ULONG size)
{
	ULONG64 endAddress = (VirtualAddress + size) & (~0xFFF);
	ULONG64 startAddress = VirtualAddress &  (~0xFFF);
	int count = 0;
	while (endAddress >= startAddress)
	{

		PHardwarePte pde = GetPTE(startAddress);
		
		if (MmIsAddressValid(pde) && pde->valid)
		{
			pde->no_execute = 0;
			pde->write = 1;
		}


		PHardwarePte pte = GetPDE(startAddress);

		if (MmIsAddressValid(pte) && pte->valid)
		{
			pte->no_execute = 0;
			pte->write = 1;
		}

		DbgPrintEx(77, 0, "[db]:pde %p pte %p %d\r\n", pde, pte, count++);

		startAddress += PAGE_SIZE;
	}

	return TRUE;
}

PVOID AllocateMemory(HANDLE pid, SIZE_T size)
{
	PEPROCESS Process = NULL;
	KAPC_STATE kApcState = { 0 };
	PVOID BaseAddress = 0;
	NTSTATUS status = PsLookupProcessByProcessId(pid, &Process);

	if (!NT_SUCCESS(status))
	{
		return NULL;
	}

	if (PsGetProcessExitStatus(Process) != STATUS_PENDING)
	{
		ObDereferenceObject(Process);
		return NULL;
	}


	KeStackAttachProcess(Process, &kApcState);

	// 后续优化，申请只读内存，然后访问虚拟地址，使其挂上物理页
	status = ZwAllocateVirtualMemory(NtCurrentProcess(), &BaseAddress, 0, &size, MEM_COMMIT, PAGE_READWRITE);

	if (NT_SUCCESS(status))
	{
		memset(BaseAddress, 0, size); // 保证有物理页
		SetExecutePage(BaseAddress, size);
	}

	KeUnstackDetachProcess(&kApcState);

	return BaseAddress;

}

NTSTATUS FreeMemory(HANDLE pid, PVOID BaseAddress, SIZE_T size)
{
	PEPROCESS Process = NULL;
	KAPC_STATE kApcState = { 0 };
	NTSTATUS status = PsLookupProcessByProcessId(pid, &Process);

	if (!NT_SUCCESS(status))
	{
		return STATUS_NOT_FOUND;
	}

	if (PsGetProcessExitStatus(Process) != STATUS_PENDING)
	{
		ObDereferenceObject(Process);
		return STATUS_UNSUCCESSFUL;
	}


	KeStackAttachProcess(Process, &kApcState);

	if (BaseAddress)
	{
		status = ZwFreeVirtualMemory(NtCurrentProcess(), &BaseAddress, &size, MEM_RELEASE);
	}

	KeUnstackDetachProcess(&kApcState);

	return status;
}