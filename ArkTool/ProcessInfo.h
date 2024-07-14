#pragma once
#include "framework.h"

typedef struct _ProcessItemEx
{
	int level; // ���̵Ĳ㼶
	std::string processName; // ������
	PProcessItem process;
}ProcessItemEx, *PProcessItemEx;

// �ṹ�嶨�壬��ʾ���ڵ㣬����չʾ���νṹʱʹ��
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

	// ��ȡ���н���
	bool GetProcessAll();

	// ��ȡָ�����̵����о��
	std::vector<PHandleItem> getHandleByPid(uint64_t pid);

	// �����νṹת��Ϊ�б����������
	std::vector<ProcessItemEx*> getProcessShowData(int sortId = -1);

	// ��ȡģ���б�
	std::vector<ModuleItem> getModuleList(uint64_t pid);

	// ��ȡ�ڴ��б�
	std::vector<MemItem> getMemList(uint64_t pid);

	int killProcessByPid(uint64_t pid);

	// ��ȡָ��pid���̵ľ��
	HANDLE openProcessByPid(uint64_t pid, DWORD access);

	int suspendProcess(uint64_t pid);
	int resumeProcess(uint64_t pid);

	bool createDumpFile(uint64_t pid, std::wstring savePath, bool isMini);

	int readVirtualMemory(uint64_t pid, void* address, uint32_t size, void* outBuffer);

	int closeHandle(uint64_t pid, uint64_t handle);
};

