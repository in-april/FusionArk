#pragma once
#include "Entry.h"
#include "typedef.h"
#include <Aux_klib.h>
#pragma comment(lib, "Aux_Klib.lib")

namespace ProcessFunc
{
	NTSTATUS dispatcher(MinorOrder subOrder, PCMD pCmd, PVOID outBuffer, ULONG outBufferSize, PULONG retLength);
	
	// 判断是否为有效进程(目前只检测了进程退出状态)
	NTSTATUS IsValidProcess(PEPROCESS process);
	// 根据pid获取进程信息
	NTSTATUS GetProcessInfoByPID(HANDLE pid, PProcessItem pItem);

	NTSTATUS OpenProcess(HANDLE pid, HANDLE* handle, ACCESS_MASK mask);

	// 枚举进程（通过ZwQuerySystemInformation获取）
	NTSTATUS EnumProcess(PVOID buffer, ULONG bufferSize, PULONG returnLength);
	// 枚举进程2（通过暴力枚举pid获取）
	NTSTATUS EnumProcess2(PVOID buffer, ULONG bufferSize, PULONG returnLength);  // 暂时无用

	// 枚举进程的私有句柄表
	NTSTATUS EnumHandleInfoByPid(PVOID buffer, ULONG bufferSize, HANDLE pid, PULONG returnLen);

	// 关闭进程
	NTSTATUS KillProcessByPid(HANDLE pid);

	// 获取进程句柄
	NTSTATUS GetProcessHandleByPid(HANDLE pid, ULONG access, HANDLE *hProcess);

	// 读取r3内存
	NTSTATUS ReadUserMemory(HANDLE pid, PVOID address, PVOID buffer, ULONG bufferSize);

	// 读取r0内存
	NTSTATUS ReadKernelMemory(PVOID address, PVOID buffer, ULONG bufferSize);

	// 读取指定虚拟地址的内存(pid == 0表示读取r0内存)
	NTSTATUS ReadVirtualMemory(HANDLE pid, PVOID address, PVOID buffer, ULONG bufferSize);

};

