#pragma once
#include "DuiViewBase.h"

enum ProcessMenu {
	RefreshProcess = 1,
	KillProcess,
	SuspendProcess,
	ResumeProcess,
	SaveDump,
	DumpString,
	HandleClose,
};

class CProcessView : public CDuiViewBase
{
public:
	static CProcessView& getInstance() {
		static CProcessView instance;
		return instance;
	}
	void ShowProcess();
	void ShowModList(uint64_t pid);
	void ShowHandleList(uint64_t pid);
	void ShowMemList(uint64_t pid);

	int PopProcessMenu();
	int PopHandleMenu();
	int DispatcherMenu(int menuIndex, void* p, void* p2 = nullptr);
};

