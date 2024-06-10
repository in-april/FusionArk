#pragma once
#include "Entry.h"
#include "typedef.h"
#include <Aux_klib.h>
#pragma comment(lib, "Aux_Klib.lib")

namespace ProcessFunc
{
	NTSTATUS dispatcher(MinorOrder subOrder, PCMD pCmd, PVOID outBuffer, ULONG outBufferSize, PULONG retLength);
	
	// �ж��Ƿ�Ϊ��Ч����(Ŀǰֻ����˽����˳�״̬)
	NTSTATUS IsValidProcess(PEPROCESS process);
	// ����pid��ȡ������Ϣ
	NTSTATUS GetProcessInfoByPID(HANDLE pid, PProcessItem pItem);

	NTSTATUS OpenProcess(HANDLE pid, HANDLE* handle, ACCESS_MASK mask);

	// ö�ٽ��̣�ͨ��ZwQuerySystemInformation��ȡ��
	NTSTATUS EnumProcess(PVOID buffer, ULONG bufferSize, PULONG returnLength);
	// ö�ٽ���2��ͨ������ö��pid��ȡ��
	NTSTATUS EnumProcess2(PVOID buffer, ULONG bufferSize, PULONG returnLength);  // ��ʱ����

	// ö�ٽ��̵�˽�о����
	NTSTATUS EnumHandleInfoByPid(PVOID buffer, ULONG bufferSize, HANDLE pid, PULONG returnLen);

	// �رս���
	NTSTATUS KillProcessByPid(HANDLE pid);

	// ��ȡ���̾��
	NTSTATUS GetProcessHandleByPid(HANDLE pid, ULONG access, HANDLE *hProcess);

	// ��ȡr3�ڴ�
	NTSTATUS ReadUserMemory(HANDLE pid, PVOID address, PVOID buffer, ULONG bufferSize);

	// ��ȡr0�ڴ�
	NTSTATUS ReadKernelMemory(PVOID address, PVOID buffer, ULONG bufferSize);

	// ��ȡָ�������ַ���ڴ�(pid == 0��ʾ��ȡr0�ڴ�)
	NTSTATUS ReadVirtualMemory(HANDLE pid, PVOID address, PVOID buffer, ULONG bufferSize);

};

