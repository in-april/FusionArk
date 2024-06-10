#include "DriverInfo.h"
#include "DriverTools.h"

CDriverInfo::CDriverInfo()
{
	m_sysModInfo = nullptr;
	m_majorOrder = MajorOrder::Driver;
}

std::vector<PSysModItem> CDriverInfo::getSysModList()
{
	std::vector<PSysModItem> vec;
	// 先释放上一次的内存
	if (m_sysModInfo != nullptr)
	{
		m_sysModInfo = nullptr;
		free(m_sysModInfo);
	}

	// 获取系统模块信息
	CMD cmd(m_majorOrder, MinorOrder::SysModList);
	DWORD retLen = 0;
	DWORD ret = CDriverTools::getInstance().SendIoctl(&cmd, sizeof(cmd), (LPVOID*)&m_sysModInfo, &retLen);
	if (ret || m_sysModInfo == nullptr) return vec;

	// 获取driver信息
	PSysModInfo driverDirInfo = nullptr;
	std::map<ptr64_t, PSysModItem> driverMap;
	CMD cmd2(m_majorOrder, MinorOrder::DriverList);
	memcpy(&cmd2.data, L"\\Driver", sizeof(L"\\Driver"));
	ret = CDriverTools::getInstance().SendIoctl(&cmd2, sizeof(cmd), (LPVOID*)&driverDirInfo, &retLen);
	if (ret || driverDirInfo == nullptr) return vec;
	for (int i = 0; i < driverDirInfo->count; i++)
	{
		PSysModItem tmpMod = &driverDirInfo->items[i];
		driverMap[tmpMod->base] = tmpMod;
		printf("name : %s\n", tmpMod->driver_name);
	}

	
	PSysModInfo fileSystemDirInfo = nullptr;
	memset(&cmd2.data, 0, sizeof(cmd2.data));
	memcpy(&cmd2.data, L"\\FileSystem", sizeof(L"\\FileSystem"));
	ret = CDriverTools::getInstance().SendIoctl(&cmd2, sizeof(cmd), (LPVOID*)&fileSystemDirInfo, &retLen);
	if (ret || fileSystemDirInfo == nullptr) return vec;
	for (int i = 0; i < fileSystemDirInfo->count; i++)
	{
		PSysModItem tmpMod = &fileSystemDirInfo->items[i];
		driverMap[tmpMod->base] = tmpMod;
		printf("name : %s\n", tmpMod->driver_name);
	}

	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	ptr64_t start = (ptr64_t)sysinfo.lpMaximumApplicationAddress;
	for (int i = 0; i < m_sysModInfo->count; i++)
	{

		PSysModItem tmpMod = &m_sysModInfo->items[i];
		if (tmpMod->base < start) continue;
		auto it = driverMap.find(tmpMod->base);
		if (it != driverMap.end())
		{
			tmpMod->driver_object = it->second->driver_object;
			memcpy(tmpMod->driver_name, it->second->driver_name, sizeof(tmpMod->driver_name));

		}

		vec.push_back(tmpMod);
	}

	free(driverDirInfo);
	free(fileSystemDirInfo);

	return vec;
	
}


