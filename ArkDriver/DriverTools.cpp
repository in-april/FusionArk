#include "DriverTools.h"
#include "CommonTools.h"
#include <aux_klib.h>

NTSTATUS DriverFunc::dispatcher(MinorOrder subOrder, PCMD pCmd, PVOID outBuffer, ULONG outBufferSize, PULONG retLength)
{
	NTSTATUS status = STATUS_SUCCESS;
	switch (subOrder)
	{
	case MinorOrder::SysModList:
	{
		if (outBuffer == NULL) return STATUS_UNSUCCESSFUL;
		status = GetSysModList2(outBuffer, outBufferSize, retLength);
		break;
	}
	case MinorOrder::DriverList:
	{
		if (outBuffer == NULL) return STATUS_UNSUCCESSFUL;
		status = GetDriverList((PWCHAR)pCmd->data, outBuffer, outBufferSize, retLength);
		break;
	}
	default:
		break;
	}
	return status;
}


NTSTATUS DriverFunc::GetSysModList(PVOID buffer, ULONG outBufferSize, PULONG returnLength)
{
	ULONG bufferSize = 0;
	PAUX_MODULE_EXTENDED_INFO moduleInfo = NULL;

	NTSTATUS status = AuxKlibInitialize();
	if (!NT_SUCCESS(status))
	{
		KdPrint(("AuxKlibInitialize failed error : %d\n", status));
		return status;
	}

	// First, get the required buffer size
	status = AuxKlibQueryModuleInformation(&bufferSize, sizeof(AUX_MODULE_EXTENDED_INFO), NULL);
	if (status != STATUS_BUFFER_TOO_SMALL)
	{
		KdPrint(("Failed to get buffer size for module information, error : %d\n", status));
		return status;
	}

	if (bufferSize > outBufferSize)
	{
		return STATUS_INFO_LENGTH_MISMATCH;
	}

	// Allocate memory for module information
	moduleInfo = (PAUX_MODULE_EXTENDED_INFO)ExAllocatePoolWithTag(PagedPool, bufferSize, 0);
	if (!moduleInfo)
	{
		KdPrint(("Failed to allocate memory for module information, error : %d\n", status));
		return status;
	}

	// Get the module information
	status = AuxKlibQueryModuleInformation(&bufferSize, sizeof(AUX_MODULE_EXTENDED_INFO), moduleInfo);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("Failed to query module information\n"));
		ExFreePool(moduleInfo);
		return status;
	}


	RtlCopyMemory(buffer, moduleInfo, bufferSize);

	// Print information about loaded drivers
	//for (ULONG i = 0; i < bufferSize / sizeof(AUX_MODULE_EXTENDED_INFO); i++)
	//{
	//	KdPrint(("Base address: %p, Size: %lu, Path: %s\n",
	//		moduleInfo[i].BasicInfo.ImageBase,
	//		moduleInfo[i].ImageSize,
	//		moduleInfo[i].FullPathName));
	//}

	// Free allocated memory
	ExFreePool(moduleInfo);
	return status;
}

NTSTATUS DriverFunc::GetSysModList2(PVOID buffer, ULONG bufferSize, PULONG returnLength)
{
	NTSTATUS status = STATUS_SUCCESS;
	ULONG bufsize = 0;

	status = ZwQuerySystemInformation(SystemModuleInformation, NULL, 0, &bufsize);
	if (status != STATUS_INFO_LENGTH_MISMATCH) {
		return status;
	}

	PRTL_PROCESS_MODULES modules = (PRTL_PROCESS_MODULES)ExAllocatePoolWithTag(PagedPool, bufsize, 0);
	if (modules == NULL) {
		return STATUS_UNSUCCESSFUL;
	}
	ULONG retLen = 0;
	status = ZwQuerySystemInformation(SystemModuleInformation, modules, bufsize, &retLen);
	if (!NT_SUCCESS(status)) {
		ExFreePool(modules);
		return STATUS_UNSUCCESSFUL;
	}

	ULONG outBufferSize = sizeof(SysModInfo) + sizeof(SysModItem) * (modules->NumberOfModules - 1);
	if (outBufferSize > bufferSize) return STATUS_BUFFER_TOO_SMALL;

	ULONG total = sizeof(SysModInfo) - sizeof(SysModItem);
	PSysModItem pItem = (PSysModItem)((ULONG64)buffer + total);

	int i = 0;
	for (; i < modules->NumberOfModules; i++)
	{
		SysModItem info = { 0 };
		info.base = (ptr64_t)modules->Modules[i].ImageBase;
		info.size = modules->Modules[i].ImageSize;
		info.init_seq = modules->Modules[i].InitOrderIndex;
		info.load_seq = modules->Modules[i].LoadOrderIndex;
		RtlCopyMemory(info.path, modules->Modules[i].FullPathName, strlen((CHAR*)modules->Modules[i].FullPathName));
		RtlCopyMemory(&pItem[i], &info, sizeof(info));
	}
	((PSysModInfo)buffer)->count = i;
	*returnLength = outBufferSize;
	return STATUS_SUCCESS;
}

NTSTATUS DriverFunc::GetDriverList(PWCHAR dirName, PVOID buffer, ULONG bufferSize, PULONG returnLength)
{
	UNICODE_STRING nsDirName;
	RtlInitUnicodeString(&nsDirName, dirName);
	OBJECT_ATTRIBUTES oa = {};
	InitializeObjectAttributes(&oa,
		&nsDirName,
		OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
		nullptr,
		nullptr);
	HANDLE DirectoryHandle = nullptr;
	NTSTATUS status = ZwOpenDirectoryObject(&DirectoryHandle, DIRECTORY_QUERY, &oa);
	if (!NT_SUCCESS(status))
	{
		return status;
	}

	ULONG count = 0;
	ULONG total = sizeof(SysModInfo) - sizeof(SysModItem);
	PSysModItem pItem = (PSysModItem)((ULONG64)buffer + total);

	ULONG QueryContext = 0;
	UNICODE_STRING DriverTypeStr;
	RtlInitUnicodeString(&DriverTypeStr, L"Driver");
	do
	{
		total += sizeof(SysModItem);
		if (total > bufferSize)
		{
			return STATUS_BUFFER_TOO_SMALL; // r3分配的输出缓冲区太小
		}

		UCHAR Buffer[1024] = { 0 };
		POBJECT_DIRECTORY_INFORMATION DirInfo = (POBJECT_DIRECTORY_INFORMATION)Buffer;
		status = ZwQueryDirectoryObject(
			DirectoryHandle,
			DirInfo,
			sizeof(Buffer),
			TRUE,
			FALSE,
			&QueryContext,
			NULL);

		if (!NT_SUCCESS(status)) continue;

		if (RtlCompareUnicodeString(&DirInfo->TypeName, &DriverTypeStr, TRUE) == 0)
		{
			UNICODE_STRING FullDriverName;
			WCHAR wcsfullname[MAX_PATH_] = { 0 };
			RtlInitEmptyUnicodeString(&FullDriverName, wcsfullname, sizeof(wcsfullname));
			RtlCopyUnicodeString(&FullDriverName, &nsDirName);
			RtlAppendUnicodeToString(&FullDriverName, L"\\");
			RtlAppendUnicodeStringToString(&FullDriverName, &DirInfo->Name);
			{
				PDRIVER_OBJECT DriverPtr = NULL;
				status = ObReferenceObjectByName(&FullDriverName, OBJ_CASE_INSENSITIVE, NULL, GENERIC_READ, *IoDriverObjectType, KernelMode, NULL, (PVOID*)&DriverPtr);
				if (NT_SUCCESS(status))
				{
					SysModItem info = { 0 };
					info.driver_object = (ptr64_t)DriverPtr;

					GetDriverInfoByPtr(DriverPtr, &info);
					ANSI_STRING FullDriverNameA = { 0 };
					RtlUnicodeStringToAnsiString(&FullDriverNameA, &FullDriverName, TRUE);
					RtlCopyMemory(info.driver_name, FullDriverNameA.Buffer, FullDriverNameA.Length);
					RtlFreeAnsiString(&FullDriverNameA);

					RtlCopyMemory(pItem, &info, sizeof(SysModItem));

					
					ObDereferenceObject(DriverPtr);
					count++;
					pItem++;
				}
			}
		}

	} while (NT_SUCCESS(status));

	((PSysModInfo)buffer)->count = count;
	*returnLength = total;

	ZwClose(DirectoryHandle);
	return STATUS_SUCCESS;
}

NTSTATUS DriverFunc::GetDriverInfoByPtr(PDRIVER_OBJECT pdrv, SysModItem* info)
{
	info->base = getData<uint64_t>(pdrv, gOffset.drvobj.DriverStart);
	return STATUS_SUCCESS;
}
