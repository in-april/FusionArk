#pragma once
#include "framework.h"
#include "resource.h"

class DuiDialog : public WindowImplBase
{
public:
	virtual LPCTSTR    GetWindowClassName() const { return _T("RenameDialog"); }
	virtual CDuiString GetSkinFile() { return _T("renameDialog.xml"); }
	virtual CDuiString GetSkinFolder() { return _T(""); }
	virtual UILIB_RESOURCETYPE GetResourceType() { return UILIB_ZIPRESOURCE;  }
	virtual LPCTSTR GetResourceID() { return MAKEINTRESOURCE(IDR_ZIPRES1); }

	void Notify(TNotifyUI& msg);
	CEditUI* m_nameEdit;
	std::wstring m_filename;
};

//class CLoginFrameWnd : public CWindowWnd, public INotifyUI, public IMessageFilterUI
//{
//public:
//	CLoginFrameWnd() { };
//	LPCTSTR GetWindowClassName() const { return _T("UILoginFrame"); };
//	UINT GetClassStyle() const { return UI_CLASSSTYLE_DIALOG; };
//	void OnFinalMessage(HWND /*hWnd*/)
//	{
//		m_pm.RemovePreMessageFilter(this);
//		delete this;
//	};
//
//	void Init() {
//		/*CComboUI* pAccountCombo = static_cast<CComboUI*>(m_pm.FindControl(_T("accountcombo")));
//		CEditUI* pAccountEdit = static_cast<CEditUI*>(m_pm.FindControl(_T("accountedit")));
//		if (pAccountCombo && pAccountEdit) pAccountEdit->SetText(pAccountCombo->GetText());
//		pAccountEdit->SetFocus();*/
//	}
//
//	void Notify(TNotifyUI& msg)
//	{
//		if (msg.sType == _T("click")) {
//			if (msg.pSender->GetName() == _T("closebtn")) { PostQuitMessage(0); return; }
//			else if (msg.pSender->GetName() == _T("loginBtn")) { Close(); return; }
//		}
//		else if (msg.sType == _T("itemselect")) {
//			if (msg.pSender->GetName() == _T("accountcombo")) {
//				CEditUI* pAccountEdit = static_cast<CEditUI*>(m_pm.FindControl(_T("accountedit")));
//				if (pAccountEdit) pAccountEdit->SetText(msg.pSender->GetText());
//			}
//		}
//	}
//
//	LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
//	{
//		LONG styleValue = ::GetWindowLong(*this, GWL_STYLE);
//		styleValue &= ~WS_CAPTION;
//		::SetWindowLong(*this, GWL_STYLE, styleValue | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
//
//		m_pm.Init(m_hWnd);
//		m_pm.AddPreMessageFilter(this);
//		CDialogBuilder builder;
//		CControlUI* pRoot = builder.Create(_T("renameDialog.xml"), (UINT)0, nullptr, &m_pm);
//		ASSERT(pRoot && "Failed to parse XML");
//		m_pm.AttachDialog(pRoot);
//		m_pm.AddNotifier(this);
//
//		Init();
//		return 0;
//	}
//
//	LRESULT OnNcActivate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
//	{
//		if (::IsIconic(*this)) bHandled = FALSE;
//		return (wParam == 0) ? TRUE : FALSE;
//	}
//
//	LRESULT OnNcCalcSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
//	{
//		return 0;
//	}
//
//	LRESULT OnNcPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
//	{
//		return 0;
//	}
//
//	LRESULT OnNcHitTest(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
//	{
//		POINT pt; pt.x = GET_X_LPARAM(lParam); pt.y = GET_Y_LPARAM(lParam);
//		::ScreenToClient(*this, &pt);
//
//		RECT rcClient;
//		::GetClientRect(*this, &rcClient);
//
//		RECT rcCaption = m_pm.GetCaptionRect();
//		if (pt.x >= rcClient.left + rcCaption.left && pt.x < rcClient.right - rcCaption.right \
//			&& pt.y >= rcCaption.top && pt.y < rcCaption.bottom) {
//			CControlUI* pControl = static_cast<CControlUI*>(m_pm.FindControl(pt));
//			if (pControl && _tcscmp(pControl->GetClass(), DUI_CTR_BUTTON) != 0)
//				return HTCAPTION;
//		}
//
//		return HTCLIENT;
//	}
//
//	LRESULT OnSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
//	{
//		SIZE szRoundCorner = m_pm.GetRoundCorner();
//		if (!::IsIconic(*this) && (szRoundCorner.cx != 0 || szRoundCorner.cy != 0)) {
//			CDuiRect rcWnd;
//			::GetWindowRect(*this, &rcWnd);
//			rcWnd.Offset(-rcWnd.left, -rcWnd.top);
//			rcWnd.right++; rcWnd.bottom++;
//			HRGN hRgn = ::CreateRoundRectRgn(rcWnd.left, rcWnd.top, rcWnd.right, rcWnd.bottom, szRoundCorner.cx, szRoundCorner.cy);
//			::SetWindowRgn(*this, hRgn, TRUE);
//			::DeleteObject(hRgn);
//		}
//
//		bHandled = FALSE;
//		return 0;
//	}
//
//	LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
//	{
//		LRESULT lRes = 0;
//		BOOL bHandled = TRUE;
//		switch (uMsg) {
//		case WM_CREATE:        lRes = OnCreate(uMsg, wParam, lParam, bHandled); break;
//		case WM_NCACTIVATE:    lRes = OnNcActivate(uMsg, wParam, lParam, bHandled); break;
//		case WM_NCCALCSIZE:    lRes = OnNcCalcSize(uMsg, wParam, lParam, bHandled); break;
//		case WM_NCPAINT:       lRes = OnNcPaint(uMsg, wParam, lParam, bHandled); break;
//		case WM_NCHITTEST:     lRes = OnNcHitTest(uMsg, wParam, lParam, bHandled); break;
//		case WM_SIZE:          lRes = OnSize(uMsg, wParam, lParam, bHandled); break;
//		default:
//			bHandled = FALSE;
//		}
//		if (bHandled) return lRes;
//		if (m_pm.MessageHandler(uMsg, wParam, lParam, lRes)) return lRes;
//		return CWindowWnd::HandleMessage(uMsg, wParam, lParam);
//	}
//
//	LRESULT MessageHandler(UINT uMsg, WPARAM wParam, LPARAM lParam, bool& bHandled)
//	{
//		if (uMsg == WM_KEYDOWN) {
//			if (wParam == VK_RETURN) {
//				CEditUI* pEdit = static_cast<CEditUI*>(m_pm.FindControl(_T("accountedit")));
//				if (pEdit->GetText().IsEmpty()) pEdit->SetFocus();
//				else {
//					pEdit = static_cast<CEditUI*>(m_pm.FindControl(_T("pwdedit")));
//					if (pEdit->GetText().IsEmpty()) pEdit->SetFocus();
//					else Close();
//				}
//				return true;
//			}
//			else if (wParam == VK_ESCAPE) {
//				PostQuitMessage(0);
//				return true;
//			}
//
//		}
//		return false;
//	}
//
//public:
//	CPaintManagerUI m_pm;
//};