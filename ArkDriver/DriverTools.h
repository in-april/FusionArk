#pragma once
#include "Entry.h"
#include "typedef.h"

extern Offset gOffset;

namespace DriverFunc
{
	NTSTATUS dispatcher(MinorOrder subOrder, PCMD pCmd, PVOID outBuffer, ULONG outBufferSize, PULONG retLength);

	// ö�������ں�ģ��
	NTSTATUS GetSysModList(PVOID buffer, ULONG bufferSize, PULONG returnLength);
	NTSTATUS GetSysModList2(PVOID buffer, ULONG bufferSize, PULONG returnLength); // ��ʱ����

	// ��ȡ��������
	NTSTATUS GetDriverList(PWCHAR dirName, PVOID buffer, ULONG bufferSize, PULONG returnLength);

	NTSTATUS GetDriverInfoByPtr(PDRIVER_OBJECT pdrv, SysModItem* info);
}