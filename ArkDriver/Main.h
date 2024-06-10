#pragma once
#include <ntifs.h>
#include <ntddk.h>

typedef struct _KEYBOARD_INPUT_DATA {
	USHORT UnitId;
	USHORT MakeCode;
	USHORT Flags;
	USHORT Reserved;
	ULONG ExtraInformation;
} KEYBOARD_INPUT_DATA, * PKEYBOARD_INPUT_DATA;

// �ں�api��ͨ��EPROCESS ��ȡ������
PUCHAR PsGetProcessImageFileName(IN PEPROCESS pProcess);

// ö�ٽ���(ͨ���������)
VOID EnumProcess();

// ���̱���ö��
VOID EnumProcess2();

// ö��ָ�����̵������߳�
VOID EnumThreads(HANDLE pid);

// ǿɱ����
VOID TerminateProcessByPid(HANDLE pid);

// ���̱���
VOID RegisterCallback();
VOID UnregisterCallback();

// APC
typedef enum _KAPC_ENVIRONMENT {
	OriginalApcEnvironment,
	AttachedApcEnvironment,
	CurrentApcEnvironment,
	InsertApcEnvironment
} KAPC_ENVIRONMENT;



typedef
VOID
(*PKRUNDOWN_ROUTINE) (
	IN struct _KAPC* Apc
	);

typedef
VOID
(*PKNORMAL_ROUTINE) (
	IN PVOID NormalContext,
	IN PVOID SystemArgument1,
	IN PVOID SystemArgument2
	);

typedef
VOID
(*PKKERNEL_ROUTINE) (
	IN struct _KAPC* Apc,
	IN OUT PKNORMAL_ROUTINE* NormalRoutine,
	IN OUT PVOID* NormalContext,
	IN OUT PVOID* SystemArgument1,
	IN OUT PVOID* SystemArgument2
	);

VOID
KeInitializeApc(
	__out PRKAPC Apc,
	__in PRKTHREAD Thread,
	__in KAPC_ENVIRONMENT Environment,
	__in PKKERNEL_ROUTINE KernelRoutine,
	__in_opt PKRUNDOWN_ROUTINE RundownRoutine,
	__in_opt PKNORMAL_ROUTINE NormalRoutine,
	__in_opt KPROCESSOR_MODE ApcMode,
	__in_opt PVOID NormalContext
);

BOOLEAN
KeInsertQueueApc(
	__inout PRKAPC Apc,
	__in_opt PVOID SystemArgument1,
	__in_opt PVOID SystemArgument2,
	__in KPRIORITY Increment
);

VOID ApcTest();

PETHREAD GetThreadById(HANDLE pid)
{
	PETHREAD thread = NULL;
	NTSTATUS st = PsLookupThreadByThreadId(pid, &thread);
	if (NT_SUCCESS(st))
	{
		ObDereferenceObject(thread);
	}
	return thread;
}

// DPC
VOID DpcTest();

// ��������
VOID TestWorkItem();

 // �ڴ��ַ��Ϣ��ѯ��pInfo����Ϊ�ں˵�ַ
NTSTATUS QueryMemory(ULONG64 Pid, ULONG64 VirtualAddress, PMEMORY_BASIC_INFORMATION pInfo);

// ��ȡ��ģ���ַ
ULONG64 QueryExeMoudle(ULONG64 Pid);

// δ�ĵ�������
ULONG64 PsGetProcessSectionBaseAddress(PEPROCESS process);

