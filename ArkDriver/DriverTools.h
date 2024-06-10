#pragma once
#include "Entry.h"
#include "typedef.h"

extern Offset gOffset;

namespace DriverFunc
{
	NTSTATUS dispatcher(MinorOrder subOrder, PCMD pCmd, PVOID outBuffer, ULONG outBufferSize, PULONG retLength);

	// 枚举所有内核模块
	NTSTATUS GetSysModList(PVOID buffer, ULONG bufferSize, PULONG returnLength);
	NTSTATUS GetSysModList2(PVOID buffer, ULONG bufferSize, PULONG returnLength); // 暂时无用

	// 获取所有驱动
	NTSTATUS GetDriverList(PWCHAR dirName, PVOID buffer, ULONG bufferSize, PULONG returnLength);

	NTSTATUS GetDriverInfoByPtr(PDRIVER_OBJECT pdrv, SysModItem* info);
}