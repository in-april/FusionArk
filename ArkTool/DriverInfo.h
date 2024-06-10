#pragma once
#include "framework.h"
class CDriverInfo
{

private:
	MajorOrder m_majorOrder;


	PSysModInfo m_sysModInfo;

	CDriverInfo();

public:
	static CDriverInfo& getInstance() {
		static CDriverInfo instance;
		return instance;
	}

	std::vector<PSysModItem> getSysModList();

};

