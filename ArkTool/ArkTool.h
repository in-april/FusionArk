#pragma once

#include "resource.h"
#include "framework.h"

class CMainWnd : public CWindowWnd, public INotifyUI, public IDialogBuilderCallback
{
public:
	CMainWnd();
	LPCTSTR GetWindowClassName()const;
	UINT GetClassStyle() const;
	void OnFinalMessage(HWND /*hWnd*/);
	void Notify(TNotifyUI& msg);
	LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

	CControlUI* CMainWnd::CreateControl(LPCTSTR pstrClassName);
public:
	CPaintManagerUI m_pm;
};