#pragma once
#include "DuiViewBase.h"
//自定义一个duilib控件来容纳MFC控件
class CMFCTreeView : public CControlUI
{
public:
	CMFCTreeView(void) {
		m_hWnd = NULL;
	};
	~CMFCTreeView(void) {
		//m_TreeCtrl.DestroyWindow();
		parentWnd.Detach();
	};

	virtual void SetInternVisible(bool bVisible = true)
	{
		__super::SetInternVisible(bVisible);
		::ShowWindow(m_hWnd, bVisible);
	}

	virtual void SetPos(RECT rc, bool bNeedInvalidate)
	{
		__super::SetPos(rc);
		::SetWindowPos(m_hWnd, NULL, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SWP_NOZORDER | SWP_NOACTIVATE);
	}

	virtual bool DoPaint(HDC hDC, const RECT& rcPaint, CControlUI* pStopControl)
	{
		::RedrawWindow(m_hWnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE);
		//::UpdateWindow(m_hWnd);
		//::InvalidateRect(m_hWnd, NULL, FALSE);
		//::SendMessage(m_hWnd, WM_NCPAINT, 0, 0);
		return true;
	}

	BOOL Attach(HWND hWndNew)
	{
		if (!::IsWindow(hWndNew))
		{
			return FALSE;
		}

		m_hWnd = hWndNew;
		return TRUE;
	}

	HWND Detach()
	{
		HWND hWnd = m_hWnd;
		m_hWnd = NULL;
		return hWnd;
	}

	CTreeCtrl* CreateTree(HWND parentHwnd, int id);

protected:
	HWND m_hWnd;
	CWnd parentWnd;
};
