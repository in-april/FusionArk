#pragma once
#include "DuiViewBase.h"

class CKernelView : public CDuiViewBase
{
public:
	static CKernelView& getInstance() {
		static CKernelView instance;
		return instance;
	}

	int PopProcessMenu();
	int DispatcherMenu(int menuIndex, void* p);

	void ShowSysMod();
};