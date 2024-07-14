#include "ProcessInfo.h"
#include "DriverTools.h"
#include "CommonUtils.h"
#include <tlhelp32.h>
#include <psapi.h>

CProcessInfo::CProcessInfo()
{
	m_processInfo = nullptr;
	m_handleInfo = nullptr;
	m_majorOrder = MajorOrder::Process;
}



bool CProcessInfo::PsIsWow64(HANDLE h)
{
	typedef BOOL(WINAPI* LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
	LPFN_ISWOW64PROCESS pIsWow64Process = NULL;
	BOOL is_wow64 = FALSE;
	pIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(GetModuleHandle(TEXT("kernel32")), "IsWow64Process");
	if (NULL != pIsWow64Process) {
		if (!pIsWow64Process(h, &is_wow64)) {
			printf("get last error : %d\n", GetLastError());
		}
	}
	return is_wow64 == TRUE;
}

bool CProcessInfo::GetProcessAll()
{
	// 先释放上一次的内存
	if (m_processInfo != nullptr)
	{
		m_processInfo = nullptr;
		free(m_processInfo);
	}
		
	m_processVec.clear();

	CMD cmd(m_majorOrder, MinorOrder::ProcessList);

	DWORD retLen = 0;
	DWORD ret = CDriverTools::getInstance().SendIoctl(&cmd, sizeof(cmd), (LPVOID*)&m_processInfo, &retLen);
	if (ret || m_processInfo == nullptr) return false;

	bool isFirst = true;
	for (uint32_t i = 0; i < m_processInfo->count; i++)
	{
		ProcessItemEx tmp = { 0 };
		tmp.process = &m_processInfo->items[i];
		if (tmp.process->pid == 0)
		{
			if(!isFirst) continue;
			tmp.processName = "Idle"; // pid == 0的进程可能有多个，比如在关闭进程的一瞬间获取进程列表
			isFirst = false;
		}
		else if (tmp.process->pid == 4)
		{
			tmp.processName = "system";
		}
		else
		{
			tmp.processName = CommonUtils::GetFileNameFromPath(tmp.process->path);
		}
		
		m_processVec.push_back(tmp);
	}
	printf("m_processVec.size() = %lld\n", m_processVec.size());
	return true;
}

std::vector<PHandleItem> CProcessInfo::getHandleByPid(uint64_t pid)
{
	// 先释放上一次的内存
	if (m_handleInfo != nullptr)
	{
		m_handleInfo = nullptr;
		free(m_handleInfo);
	}

	CMD cmd(m_majorOrder, MinorOrder::HandleList);
	cmd.pid = pid;
	std::vector<PHandleItem> handleList;

	DWORD retLen = 0;
	DWORD ret = CDriverTools::getInstance().SendIoctl(&cmd, sizeof(cmd), (LPVOID*)&m_handleInfo, &retLen);
	if (ret || m_handleInfo == nullptr) return handleList;

	for (uint32_t i = 0; i < m_handleInfo->count; i++)
	{
		handleList.push_back(&m_handleInfo->items[i]);
	}
	// printf("m_handleInfo.size() = %lld\n", handleList.size());
	return handleList;
}

void getShowDataSub(std::vector<ProcessItemEx*>& vec, ProcessNode* root, int depth)
{
	if (root == nullptr) return;
	if (depth > -1)
	{
		root->process->level = depth;
		vec.push_back(root->process);
	}
	for (ProcessNode* child : root->children) {
		getShowDataSub(vec, child, depth + 1);
	}
}

// 释放树的内存
void deleteTree(ProcessNode* root) {
	if (root == nullptr) return;
	for (ProcessNode* child : root->children) {
		deleteTree(child);
	}
	delete root;
}

std::vector<ProcessItemEx*> CProcessInfo::getProcessShowData(int sortId)
{
	//MessageBox(0, 0, 0, 0);
	std::vector<ProcessItemEx*> ret;
	if (sortId == -1) // 树形排序
	{
		std::map<uint64_t, PProcessNode> nodeMap;

		// 创建节点并填充 nodeMap
		for (auto& procEx : m_processVec) {
			ProcessNode* newNode = new ProcessNode;
			newNode->process = &procEx;
			nodeMap[procEx.process->pid] = newNode;
		}

		ProcessNode* root = new ProcessNode;

		// 连接节点
		for (auto& procEx : m_processVec) {
			uint64_t pid = procEx.process->pid;
			uint64_t parentid = procEx.process->parent_pid;
			auto it = nodeMap.find(parentid);
			if (parentid != 0 && it != nodeMap.end() 
				// 父进程的创建时间必须小于子进程创建时间，因为可能出现父进程已经退出，但pid被子进程占用的情况
				&& it->second->process->process->start_time <= procEx.process->start_time) 
			{
				nodeMap[parentid]->children.push_back(nodeMap[pid]);
			}
			else
			{
				root->children.push_back(nodeMap[pid]);
			}
		}
		getShowDataSub(ret, root, -1);
		deleteTree(root);
	}
	else if (sortId == 0) // 根据名称排序
	{

	}
	
	return ret;
}

std::vector<ModuleItem> CProcessInfo::getModuleList(uint64_t pid)
{
	std::vector<ModuleItem> ret;
	DWORD flags = TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32;

	HANDLE snap = CreateToolhelp32Snapshot(flags, pid);
	if (snap == INVALID_HANDLE_VALUE) {
		printf("CreateToolhelp32Snapshot pid:%lld err:%d\n", pid, GetLastError());
		return ret;
	}
	MODULEENTRY32 entry;
	entry.dwSize = sizeof(entry);
	if (!Module32First(snap, &entry)) {
		CloseHandle(snap);
		printf("Module32FirstW err:%d\n", GetLastError());
		return ret;
	}
	do {
		ModuleItem item = { 0 };
		item.image_base = (ptr64_t)entry.modBaseAddr;
		item.image_size = entry.modBaseSize;
		std::string path = CommonUtils::Unicode2Ansi(entry.szExePath);
		memcpy(&item.path, path.c_str(), path.size());
		std::string name = CommonUtils::Unicode2Ansi(entry.szModule);
		memcpy(&item.name, name.c_str(), name.size());
		// printf("path : %s, start : 0x%llx, size : 0x%d\n", path.c_str(), item.image_base, item.image_size);
		ret.push_back(item);
	} while (Module32Next(snap, &entry));
	CloseHandle(snap);
	return ret;

}

std::vector<MemItem> CProcessInfo::getMemList(uint64_t pid)
{
	std::vector<MemItem> ret;
	// HANDLE hProcess = OpenProcess(PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION, FALSE, pid);
	HANDLE hProcess = openProcessByPid(pid, PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION);
	if (!hProcess) {
		printf("OpenProcess pid:%lld err:%d", pid, GetLastError());
		return ret;
	}
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	char* start = (char*)sysinfo.lpMinimumApplicationAddress;
	char* end = (char*)sysinfo.lpMaximumApplicationAddress;
	if (PsIsWow64(hProcess)) {
		end = (char*)0x7FFE0000;
	}
	DWORD pagesize = sysinfo.dwPageSize;
	do {
		MEMORY_BASIC_INFORMATION mbi;
		if (VirtualQueryEx(hProcess, start, &mbi, sizeof(MEMORY_BASIC_INFORMATION))) {
			MemItem tmp = { 0 };
			tmp.base = (ptr64_t)mbi.BaseAddress;
			tmp.alloc_base = (ptr64_t)mbi.AllocationBase;
			tmp.alloc_attr = mbi.AllocationProtect;
			tmp.attr = mbi.Protect;
			tmp.size = mbi.RegionSize;
			tmp.type = mbi.Type;
			tmp.status = mbi.State;

			std::string mod_name;
			char name[MAX_PATH_] = { 0 };
			if (mbi.Type & MEM_IMAGE  || mbi.Type & MEM_MAPPED) {
				GetMappedFileNameA(hProcess, mbi.BaseAddress, name, MAX_PATH_);
				mod_name = CommonUtils::NtPathToDosPath(name);
				if (mod_name.size() > 3)
				{
					memcpy(&tmp.path, mod_name.c_str(), mod_name.size());
				}
			}
			
			ret.push_back(tmp);
			//printf("mod_name : %s\n", tmp.path);
			start += mbi.RegionSize;
		}
		else {
			start += pagesize;
		}
	} while (start < end);
	CloseHandle(hProcess);
	return ret;
}

int CProcessInfo::killProcessByPid(uint64_t pid)
{
	CMD cmd(m_majorOrder, MinorOrder::KillProcess);
	cmd.pid = pid;

	DWORD retLen = 0;
	DWORD ret = CDriverTools::getInstance().SendIoctlEx(&cmd, sizeof(cmd), nullptr, 0, &retLen);
	return ret;
}

HANDLE CProcessInfo::openProcessByPid(uint64_t pid, DWORD access)
{
	CMD cmd(m_majorOrder, MinorOrder::OpenProcess);
	cmd.pid = pid;
	cmd.access = access;
	DWORD retLen = 0;

	HANDLE hProcess = NULL;
	DWORD ret = CDriverTools::getInstance().SendIoctlEx(&cmd, sizeof(cmd), &hProcess, sizeof(hProcess), &retLen);
	if (ret) printf("openProcessByPid error : %d", ret);
	return hProcess;
}

int CProcessInfo::suspendProcess(uint64_t pid)
{
	DWORD ret = 0;
	HANDLE hProcess = openProcessByPid(pid, PROCESS_SUSPEND_RESUME);
	typedef DWORD(WINAPI* NtSuspendProcess)(HANDLE ProcessHandle);

	HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
	if (!ntdll) return -1;
	NtSuspendProcess pNtSuspendProcess = (NtSuspendProcess)GetProcAddress(ntdll, "NtSuspendProcess");
	if (pNtSuspendProcess && hProcess) {
		ret = pNtSuspendProcess(hProcess);
		CloseHandle(hProcess);
	}
	return ret;
}

int CProcessInfo::resumeProcess(uint64_t pid)
{
	DWORD ret = 0;
	HANDLE hProcess = openProcessByPid(pid, PROCESS_SUSPEND_RESUME);
	typedef DWORD(WINAPI* NtResumeProcess)(HANDLE hProcess);

	HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
	if (!ntdll) return -1;
	NtResumeProcess pNtResumeProcess = (NtResumeProcess)GetProcAddress(ntdll, "NtResumeProcess");
	if (pNtResumeProcess && hProcess) {
		ret = pNtResumeProcess(hProcess);
		CloseHandle(hProcess);
	}
	return ret;
}

bool CProcessInfo::createDumpFile(uint64_t pid, std::wstring savePath, bool isMini)
{
	MINIDUMP_TYPE dmp_type;
	if (isMini) {
		dmp_type = (MINIDUMP_TYPE)(MiniDumpWithThreadInfo | MiniDumpWithFullMemoryInfo |
			MiniDumpWithProcessThreadData | MiniDumpWithHandleData | MiniDumpWithDataSegs);
	}
	else {
		dmp_type = (MINIDUMP_TYPE)(MiniDumpWithThreadInfo | MiniDumpWithFullMemoryInfo | MiniDumpWithTokenInformation |
			MiniDumpWithProcessThreadData | MiniDumpWithDataSegs | MiniDumpWithFullMemory | MiniDumpWithHandleData);
	}
	HANDLE pProcess = openProcessByPid(pid, PROCESS_ALL_ACCESS);
	if (!pProcess) {
		return false;
	}
	HANDLE fd = CreateFileW(savePath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (fd == INVALID_HANDLE_VALUE) {
		CloseHandle(pProcess);
		return false;
	}
	BOOL ret = MiniDumpWriteDump(pProcess, pid, fd, dmp_type, NULL, NULL, NULL);
	CloseHandle(fd);
	CloseHandle(pProcess);
	return ret == true;
}

int CProcessInfo::readVirtualMemory(uint64_t pid, void* address, uint32_t size, void* outBuffer)
{
	CMD cmd(m_majorOrder, MinorOrder::ReadVm);
	cmd.pid = pid;
	cmd.address = (ptr64_t)address;
	cmd.data_size = size;

	DWORD retLen = 0;

	DWORD ret = CDriverTools::getInstance().SendIoctlEx(&cmd, sizeof(cmd), outBuffer, size, &retLen);
	if (ret) printf("readVirtualMemory error : %d", ret);
	return ret;
}

int CProcessInfo::closeHandle(uint64_t pid, uint64_t handle)
{
	CMD cmd(m_majorOrder, MinorOrder::CloseHandle);
	cmd.pid = pid;
	cmd.handle = handle;

	DWORD retLen = 0;
	DWORD ret = CDriverTools::getInstance().SendIoctlEx(&cmd, sizeof(cmd), nullptr, 0, &retLen);
	return ret;
}
