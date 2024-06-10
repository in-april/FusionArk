#include "ProcessTools.h"
#include "CommonTools.h"


extern Offset gOffset;

NTSTATUS ProcessFunc::dispatcher(MinorOrder subOrder, PCMD pCmd, PVOID outBuffer, ULONG outBufferSize, PULONG retLength)
{
	NTSTATUS status = STATUS_SUCCESS;
	switch (subOrder)
	{
	case MinorOrder::ProcessList:
	{
		if (outBuffer == NULL) return STATUS_UNSUCCESSFUL;
		status = EnumProcess(outBuffer, outBufferSize, retLength);
		break;
	}
	case MinorOrder::HandleList:
	{
		if (outBuffer == NULL) return STATUS_UNSUCCESSFUL;
		status = EnumHandleInfoByPid(outBuffer, outBufferSize, (HANDLE)pCmd->pid, retLength);
		break;
	}
	case MinorOrder::KillProcess:
	{
		status = KillProcessByPid((HANDLE)pCmd->pid);
		break;
	}
	case MinorOrder::OpenProcess:
	{
		if (outBuffer == NULL) return STATUS_UNSUCCESSFUL;
		status = GetProcessHandleByPid((HANDLE)pCmd->pid, pCmd->access, (HANDLE*)outBuffer);
		break;
	}
	case MinorOrder::ReadVm:
	{
		if (outBuffer == NULL) return STATUS_UNSUCCESSFUL;
		status = ReadVirtualMemory((HANDLE)pCmd->pid, (PVOID)pCmd->address, outBuffer, pCmd->data_size);
		break;
	}
	default:
		break;
	}
	return status;
}

NTSTATUS ProcessFunc::IsValidProcess(PEPROCESS process)
{
	if (PsGetProcessExitStatus(process) != STATUS_PENDING)
	{
		return STATUS_UNSUCCESSFUL;
	}
	return STATUS_SUCCESS;
}

NTSTATUS ProcessFunc::GetProcessInfoByPID(HANDLE pid, PProcessItem pItem)
{
	NTSTATUS  status = STATUS_SUCCESS;
	PEPROCESS EProcess = NULL;

	status = PsLookupProcessByProcessId(pid, &EProcess);
	if (!NT_SUCCESS(status))
	{
		return status;
	}
	__try
	{
		status = IsValidProcess(EProcess);
		if (!NT_SUCCESS(status))
		{
			return status;
		}
		__try
		{
			pItem->pid = (uint64_t)pid;
			pItem->parent_pid = getData<uint64_t>(EProcess, gOffset.eprocess.InheritedFromUniqueProcessId);
			pItem->eprocess = (ptr64_t)EProcess;

			// 路径
			PUNICODE_STRING pFilepathW = NULL;
			ANSI_STRING filepathA = { 0 };
			NTSTATUS st = SeLocateProcessImageName(EProcess, &pFilepathW);
			RtlUnicodeStringToAnsiString(&filepathA, pFilepathW, TRUE);
			memset(pItem->path, 0, sizeof(pItem->path));
			RtlCopyMemory(pItem->path, filepathA.Buffer, filepathA.Length);
			ExFreePool(pFilepathW);
			RtlFreeAnsiString(&filepathA);

			pItem->start_time = getData<uint64_t>(EProcess, gOffset.eprocess.CreateTime);
			uint32_t Flag2 = getData<uint32_t>(EProcess, gOffset.eprocess.Flags2);
			if ((Flag2 >> 11) & 1) // 判断第11位是否为1
			{
				pItem->r3_open = false;
			}
			else
			{
				pItem->r3_open = true;
			}
		}
		__except(EXCEPTION_EXECUTE_HANDLER)
		{
			__leave;
		}
	}
	__finally
	{
		ObDereferenceObject(EProcess);   // 解引用
	}
	return status;
}

NTSTATUS ProcessFunc::OpenProcess(HANDLE pid, HANDLE* hProcess, ACCESS_MASK mask)
{
	CLIENT_ID cid = { 0 };
	cid.UniqueProcess = (HANDLE)pid;
	cid.UniqueThread = (HANDLE)0;
	OBJECT_ATTRIBUTES oa = { 0 };
	InitializeObjectAttributes(&oa, NULL, 0, NULL, NULL);
	return ZwOpenProcess(hProcess, mask, &oa, &cid);
}

NTSTATUS ProcessFunc::EnumProcess(PVOID buffer, ULONG bufferSize, PULONG returnLen)
{
	NTSTATUS status = STATUS_SUCCESS;
	ULONG length = 0x10000;
	PVOID pSystemProcessInfo = NULL;
	ULONG retLength;

	__try
	{
		do
		{
			pSystemProcessInfo = (PVOID)ExAllocatePoolWithTag(PagedPool, length, 0);
			if (!pSystemProcessInfo)
			{
				return STATUS_NO_MEMORY;
			}
			memset(pSystemProcessInfo, 0, length);
			// 不确定ReturnLength是否稳定返回需要的长度，所以此处多次尝试
			status = ZwQuerySystemInformation(SystemProcessInformation, pSystemProcessInfo, length, &retLength);
			if (status == STATUS_INFO_LENGTH_MISMATCH)
			{
				ExFreePool(pSystemProcessInfo);
				pSystemProcessInfo = NULL;
				length += 0x2000;
			}
		} while (status == STATUS_INFO_LENGTH_MISMATCH);

		if (!NT_SUCCESS(status))
		{
			return status;
		}

		ULONG count = 0;
		ULONG total = sizeof(ProcessInfo) - sizeof(ProcessItem);
		PProcessItem pItem = (PProcessItem)((ULONG64)buffer + total);

		PSYSTEM_PROCESS_INFORMATION current = (PSYSTEM_PROCESS_INFORMATION)pSystemProcessInfo;
		do
		{
			total += sizeof(ProcessItem);
			if (total > bufferSize)
			{
				return STATUS_BUFFER_TOO_SMALL;
			}

			// 将SYSTEM_PROCESS_INFORMATION结构的信息转换为ProcessItem结构的信息
			HANDLE pid = current->UniqueProcessId;
			GetProcessInfoByPID(pid, pItem);
			count++;

			if (current->NextEntryOffset == 0) break;
			current = (PSYSTEM_PROCESS_INFORMATION)((ULONG64)current + current->NextEntryOffset);
			pItem++;
			
		} while (true);
		((PProcessInfo)buffer)->count = count;
		*returnLen = total;
	}
	__finally
	{
		if (pSystemProcessInfo) ExFreePool(pSystemProcessInfo);
	}
	return status;
}

NTSTATUS ProcessFunc::EnumProcess2(PVOID Buffer, ULONG BufferSize, PULONG ReturnLength)
{
	NTSTATUS status = STATUS_SUCCESS;
	return status;
}

NTSTATUS ProcessFunc::EnumHandleInfoByPid(PVOID buffer, ULONG bufferSize, HANDLE pid, PULONG returnLen)
{
	NTSTATUS status = STATUS_SUCCESS;
	ULONG length = 0x10000;
	PVOID pHandleTableInfo = NULL;
	ULONG retLength;
	HANDLE hProcess = NULL;
	PEPROCESS EProcess = NULL;

	__try
	{
		do
		{
			pHandleTableInfo = (PVOID)ExAllocatePoolWithTag(PagedPool, length, 0);
			if (!pHandleTableInfo)
			{
				return STATUS_NO_MEMORY;
			}
			memset(pHandleTableInfo, 0, length);
			// 不确定ReturnLength是否稳定返回需要的长度，所以此处多次尝试
			status = ZwQuerySystemInformation(SystemHandleInformation, pHandleTableInfo, length, &retLength);
			if (status == STATUS_INFO_LENGTH_MISMATCH)
			{
				ExFreePool(pHandleTableInfo);
				pHandleTableInfo = NULL;
				length += 0x10000;
			}
		} while (status == STATUS_INFO_LENGTH_MISMATCH);

		if (!NT_SUCCESS(status))
		{
			return status;
		}

		ULONG64 handleCount = ((SYSTEM_HANDLE_INFORMATION*)pHandleTableInfo)->NumberOfHandles;
		SYSTEM_HANDLE_TABLE_ENTRY_INFO* handles = (SYSTEM_HANDLE_TABLE_ENTRY_INFO*)((SYSTEM_HANDLE_INFORMATION*)pHandleTableInfo)->Handles;
		
		
		status = PsLookupProcessByProcessId(pid, &EProcess);
		if (!NT_SUCCESS(status))
		{
			return status;
		}
		status = PsAcquireProcessExitSynchronization(EProcess);
		if (!NT_SUCCESS(status))
		{
			return status;
		}

		ULONG count = 0;
		ULONG total = sizeof(HandleInfo) - sizeof(HandleItem);
		PHandleItem pItem = (PHandleItem)((ULONG64)buffer + total);

		for (ULONG64 i = 0; i < handleCount; i++)
		{
			ULONG64 processid = handles[i].UniqueProcessId;
			if (pid == (HANDLE)processid)
			{
				total += sizeof(HandleItem);
				if (total > bufferSize)
				{
					return STATUS_BUFFER_TOO_SMALL; // r3分配的输出缓冲区太小
				}

				HANDLE handle = (HANDLE)handles[i].HandleValue;
				ULONG grantedAccess = handles[i].GrantedAccess;
				// HANDLE hDupHandle = NULL;
				OBJECT_BASIC_INFORMATION basicInfo = { 0 };
				// 用于临时存储查询出来的名称，2k应该差不多了
				CHAR tempBuffer[2048] = { 0 };

				do {
					// 拷贝句柄有个问题，就是不能拷贝ETW的句柄
					/*status = ZwDuplicateObject(hProcess, handle, NtCurrentProcess(), &hDupHandle, PROCESS_ALL_ACCESS, 0, DUPLICATE_SAME_ACCESS);
					if (!NT_SUCCESS(status)) {
						KdPrint(("ZwDuplicateObject %p Fail, status : %d", handle, status));
						break;
					}*/
					KAPC_STATE apcState;
					KeStackAttachProcess(EProcess, &apcState);
					status = ZwQueryObject(handle, ObjectBasicInformation, &basicInfo, sizeof(OBJECT_BASIC_INFORMATION), NULL);
					KeUnstackDetachProcess(&apcState);
					if (!NT_SUCCESS(status)) {
						KdPrint(("ZwQueryObject %p basic Fail, status : %d", handle, status));
						break;
					}
					// 获取句柄名称
					ULONG retLen = 0;
					KeStackAttachProcess(EProcess, &apcState);
					status = ZwQueryObject(handle, (OBJECT_INFORMATION_CLASS)1, tempBuffer, sizeof(tempBuffer), &retLen);
					KeUnstackDetachProcess(&apcState);
					if (!NT_SUCCESS(status)) {
						KdPrint(("ZwQueryObject %p obj name Fail, status : %d", handle, status));
						break;
					}
					POBJECT_NAME_INFORMATION nameInfo = (POBJECT_NAME_INFORMATION)tempBuffer;
					ANSI_STRING obj_name = { 0 };
					RtlUnicodeStringToAnsiString(&obj_name, &nameInfo->Name, TRUE);
					RtlCopyMemory(pItem[count].obj_name, obj_name.Buffer, obj_name.Length);
					RtlFreeAnsiString(&obj_name);


					RtlZeroMemory(tempBuffer, sizeof(tempBuffer));
					KeStackAttachProcess(EProcess, &apcState);
					status = ZwQueryObject(handle, ObjectTypeInformation, tempBuffer, sizeof(tempBuffer), &retLen); // ObjectTypeInformation
					KeUnstackDetachProcess(&apcState);
					if (!NT_SUCCESS(status)) {
						KdPrint(("ZwQueryObject %p handle type name Fail, status : %d", handle, status));
						break;
					}
					// 真实结构为OBJECT_TYPE_INFORMATION，但因为未定义且其他字段用不上，所以使用NAME的结构
					POBJECT_NAME_INFORMATION typeInfo = (POBJECT_NAME_INFORMATION)tempBuffer;;
					ANSI_STRING type_name = { 0 };
					RtlUnicodeStringToAnsiString(&type_name, &typeInfo->Name, TRUE);
					RtlCopyMemory(pItem[count].type_name, type_name.Buffer, type_name.Length);
					RtlFreeAnsiString(&type_name);

					
				} while (false);

				// if (hDupHandle) ZwClose(hDupHandle);
				pItem[count].pid = (uint64_t)pid;
				pItem[count].value = (uint64_t)handle;
				pItem[count].obj_address = (ptr64_t)handles[i].Object;
				pItem[count].handle_count = basicInfo.HandleCount;
				pItem[count].obj_count = basicInfo.ReferenceCount;
				pItem[count].access = grantedAccess;
				count++;
			}
		}

		((PHandleInfo)buffer)->count = count;
		*returnLen = total;
	}
	__finally
	{
		// if (hProcess) ZwClose(hProcess);
		if (pHandleTableInfo) ExFreePool(pHandleTableInfo);
		if (EProcess)
		{
			PsReleaseProcessExitSynchronization(EProcess);
			ObDereferenceObject(EProcess);   // 解引用
		}
	}
	return STATUS_SUCCESS;
	
}

NTSTATUS ProcessFunc::KillProcessByPid(HANDLE pid)
{
	NTSTATUS status;          // 状态码

	// 根据进程 ID 获取进程结构体
	PEPROCESS process = NULL;
	status = PsLookupProcessByProcessId((HANDLE)pid, &process);
	if (!NT_SUCCESS(status))
		return status;

	if (process == PsGetCurrentProcess()) return STATUS_UNSUCCESSFUL;// 关闭自己会卡死
	// 打开进程对象
	HANDLE processHandle = NULL;
	
	// 如果为内核模式，则无需获取具体权限即可调用ZwTerminateProcess关闭进程
	status = ObOpenObjectByPointer(process, OBJ_KERNEL_HANDLE, NULL, NULL, *PsProcessType, KernelMode, &processHandle);
	ObfDereferenceObject(process);  // 释放进程结构体
	if (!NT_SUCCESS(status))
		return status;

	// 终止进程
	status = ZwTerminateProcess(processHandle, 0);
	ZwClose(processHandle);

	// 返回终止状态
	return status;
}

NTSTATUS ProcessFunc::GetProcessHandleByPid(HANDLE pid, ULONG access, HANDLE *hProcess)
{
	NTSTATUS status;          // 状态码

	// 根据进程 ID 获取进程结构体
	PEPROCESS process = NULL;
	status = PsLookupProcessByProcessId((HANDLE)pid, &process);
	if (!NT_SUCCESS(status))
		return status;

	// 打开进程对象
	HANDLE processHandle = NULL;
	KPROCESSOR_MODE PreviousMode = ExGetPreviousMode();

	ULONG attr = 0;
	if (PreviousMode == KernelMode)
	{
		attr = OBJ_KERNEL_HANDLE;
	}
	status = ObOpenObjectByPointer(process, attr, NULL, access, *PsProcessType, KernelMode, &processHandle);
	ObfDereferenceObject(process);  // 释放进程结构体
	if (!NT_SUCCESS(status))
		return status;

	*hProcess = processHandle;
	return status;
}

NTSTATUS ProcessFunc::ReadUserMemory(HANDLE pid, PVOID address, PVOID buffer, ULONG bufferSize)
{
	PEPROCESS process = NULL;
	KAPC_STATE kApcState = { 0 };
	
	
	LOGICAL Probing;
	
	NTSTATUS status = PsLookupProcessByProcessId(pid, &process);

	if (!NT_SUCCESS(status))
	{
		return status;
	}

	__try
	{
		// 验证进程有效性
		status = IsValidProcess(process);
		if (!NT_SUCCESS(status))
		{
			return status;
		}

		// 加锁，防止进程退出
		status = PsAcquireProcessExitSynchronization(process);
		if (!NT_SUCCESS(status))
		{
			return status;
		}

		PVOID tempBuf = ExAllocatePoolWithTag(NonPagedPool, bufferSize, 0);
		if (tempBuf)
		{
			KAPC_STATE apcState;
			KeStackAttachProcess(process, &apcState);
			__try
			{
				RtlCopyMemory(tempBuf, address, bufferSize);
			}
			__except(EXCEPTION_EXECUTE_HANDLER)
			{
				status = STATUS_UNSUCCESSFUL;
			}
			KeUnstackDetachProcess(&apcState);

			RtlCopyMemory(buffer, tempBuf, bufferSize);
			ExFreePool(tempBuf);
		}
	}
	__finally
	{
		if (process)
		{
			PsReleaseProcessExitSynchronization(process);
			ObDereferenceObject(process);   // 解引用
		}
		
	}
	
	return STATUS_SUCCESS;
}

NTSTATUS ProcessFunc::ReadKernelMemory(PVOID address, PVOID buffer, ULONG bufferSize)
{
	return STATUS_SUCCESS;
}

NTSTATUS ProcessFunc::ReadVirtualMemory(HANDLE pid, PVOID address, PVOID buffer, ULONG bufferSize)
{
	if (pid)
	{
		return ReadUserMemory(pid, address, buffer, bufferSize);
	}
	else
	{
		return ReadKernelMemory(address, buffer, bufferSize);
	}
	
}

