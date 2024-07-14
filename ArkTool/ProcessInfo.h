#pragma once
#include "framework.h"

typedef struct _ProcessItemEx
{
	int level; // 进程的层级
	std::string processName; // 进程名
	PProcessItem process;
}ProcessItemEx, *PProcessItemEx;

// 结构体定义，表示树节点，用于展示树形结构时使用
typedef struct _ProcessNode {
	vector<_ProcessNode*> children;
	ProcessItemEx* process = nullptr;
}ProcessNode, * PProcessNode;


class CProcessInfo
{
private:
	std::vector<ProcessItemEx> m_processVec;
	MajorOrder m_majorOrder;

	PProcessInfo m_processInfo;
	PHandleInfo m_handleInfo;

	CProcessInfo();

public:
	

	static CProcessInfo& getInstance() {
		static CProcessInfo instance;
		return instance;
	}

	bool PsIsWow64(HANDLE h);

	// 获取所有进程
	bool GetProcessAll();

	// 获取指定进程的所有句柄
	std::vector<PHandleItem> getHandleByPid(uint64_t pid);

	// 将树形结构转换为列表（先序遍历）
	std::vector<ProcessItemEx*> getProcessShowData(int sortId = -1);

	// 获取模块列表
	std::vector<ModuleItem> getModuleList(uint64_t pid);

	// 获取内存列表
	std::vector<MemItem> getMemList(uint64_t pid);

	int killProcessByPid(uint64_t pid);

	// 获取指定pid进程的句柄
	HANDLE openProcessByPid(uint64_t pid, DWORD access);

	int suspendProcess(uint64_t pid);
	int resumeProcess(uint64_t pid);

	bool createDumpFile(uint64_t pid, std::wstring savePath, bool isMini);

	int readVirtualMemory(uint64_t pid, void* address, uint32_t size, void* outBuffer);

	int closeHandle(uint64_t pid, uint64_t handle);
};

