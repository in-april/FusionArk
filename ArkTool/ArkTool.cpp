// ArkTool.cpp : 定义应用程序的入口点。
//

#include "framework.h"
#include "ArkTool.h"
#include "ProcessInfo.h"
#include "ProcessView.h"
#include "KernelView.h"
#include "FileManagerView.h"

#define FILE_TREE_ID 1001

CMainWnd::CMainWnd()
{
}

LPCTSTR CMainWnd::GetWindowClassName()const
{
	return _T("UIMainFrame");
}

UINT CMainWnd::GetClassStyle() const
{
	return UI_CLASSSTYLE_FRAME | CS_DBLCLKS;
}

void CMainWnd::OnFinalMessage(HWND /*hWnd*/)
{
	delete this;
}

void CMainWnd::Notify(TNotifyUI& msg)
{
	if (msg.sType == DUI_MSGTYPE_CLICK)
	{
		CDuiString name = msg.pSender->GetName();
		CTabLayoutUI* pControl = static_cast<CTabLayoutUI*>(m_pm.FindControl(_T("switch")));
		CTabLayoutUI* pSubControl = static_cast<CTabLayoutUI*>(m_pm.FindControl(_T("procSubSwitch")));
		if (name == _T("process"))
			pControl->SelectItem(0);
		else if (name == _T("driver"))
		{
			pControl->SelectItem(1);
			CKernelView::getInstance().setPaintManager(&m_pm);
			CKernelView::getInstance().ShowSysMod();
		}
			
		else if (name == _T("kernel"))
			pControl->SelectItem(2);
		else if (name == _T("hook"))
			pControl->SelectItem(3);
		else if (name == _T("files"))
		{
			pControl->SelectItem(2);
			CFileManagerView::getInstance().setPaintManager(&m_pm);
			CFileManagerView::getInstance().Init();
			CFileManagerView::getInstance().ShowRoot();

		}
			
		else if (name == _T("network"))
			pControl->SelectItem(5);
		else if (name == _T("startInfo"))
			pControl->SelectItem(6);
		else if (name == _T("register"))
			pControl->SelectItem(7);
		else if (name == _T("moduleOpt"))
			pSubControl->SelectItem(0);
		else if (name == _T("handleOpt"))
			pSubControl->SelectItem(1);
		else if (name == _T("memOpt"))
			pSubControl->SelectItem(2);
	}
	else if (msg.sType == DUI_MSGTYPE_MENU)
	{
		CListUI* list = ((CListUI*)msg.pSender);
		CDuiString str = list->GetName();
		if (str == L"ProcessList")
		{
			CControlUI* p = list->GetItemAt(list->GetCurSel());
			if (p)
			{
				CListTextElementUI* pListElement = (CListTextElementUI*)p;
				int pid = pListElement->GetTag();
				printf("menu pid:%d\n", pid);

				int idx = CProcessView::getInstance().PopProcessMenu();
				if (idx > 0)  CProcessView::getInstance().DispatcherMenu(idx, (void*)pid);
			}
			
		}
		else if (str == L"FileList")
		{
			CControlUI* p = list->GetItemAt(list->GetCurSel());
			if (p)
			{
				CListTextElementUI* pListElement = (CListTextElementUI*)p;
				std::wstring filepath = pListElement->GetUserData();
				printf("file path : %s\n", CommonUtils::Unicode2Ansi(filepath).c_str());

				int idx = CFileManagerView::getInstance().PopFileManagerMenu(true);
				if (idx > 0)  CFileManagerView::getInstance().DispatcherMenu(idx, filepath);
			}
			else
			{
				int idx = CFileManagerView::getInstance().PopFileManagerMenu(false);
				if (idx > 0)  CFileManagerView::getInstance().DispatcherMenu(idx, "");
			}
			
			
			
		}
		
	}
	else if (msg.sType == DUI_MSGTYPE_ITEMCLICK)
	{
		CDuiString str = ((CListTextElementUI*)msg.pSender)->GetUserData();
		if (str == L"ProcessList")
		{
			int pid = ((CListTextElementUI*)msg.pSender)->GetTag();
			printf("Process list gettag:%d\n", pid);
			CProcessView::getInstance().ShowModList(pid);
			CProcessView::getInstance().ShowHandleList(pid);
			CProcessView::getInstance().ShowMemList(pid);
		}
		
	}
}



LRESULT CMainWnd::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CREATE:
	{
		// MessageBox(0, 0, 0, 0);
		CenterWindow();
		m_pm.Init(m_hWnd);
		CDialogBuilder builder;

		HRSRC hResource = ::FindResource(m_pm.GetResourceDll(), MAKEINTRESOURCE(IDR_ZIPRES1), _T("ZIPRES"));
		if (hResource == NULL)
			return 0L;
		DWORD dwSize = 0;
		HGLOBAL hGlobal = ::LoadResource(m_pm.GetResourceDll(), hResource);
		if (hGlobal == NULL)
		{
			::FreeResource(hResource);
			return 0L;
		}
		dwSize = ::SizeofResource(m_pm.GetResourceDll(), hResource);
		if (dwSize == 0)
			return 0L;
		BYTE* zipBuffer = new BYTE[dwSize];
		if (zipBuffer != NULL)
		{
			::CopyMemory(zipBuffer, (LPBYTE)::LockResource(hGlobal), dwSize);
		}
		::FreeResource(hResource);
		m_pm.SetResourceZip(zipBuffer, dwSize);

		CControlUI* pRoot = builder.Create(_T("Main.xml"), 0, this, &m_pm);
		
		m_pm.AttachDialog(pRoot);
		m_pm.AddNotifier(this);


		CProcessView::getInstance().setPaintManager(&m_pm);
		CProcessView::getInstance().ShowProcess();

		return 0;
		break;
	}
	case WM_NOTIFY:
	{
		if (wParam == FILE_TREE_ID)
		{
			NMHDR* p = (NMHDR*)lParam;
			if (p != NULL )
			{
				switch (p->code)
				{
				case NM_CLICK:
				{
					CFileManagerView::getInstance().LoadFileList();
					
					break;
				}
				case TVN_ITEMEXPANDING:
				{
					CFileManagerView::getInstance().OnTVItemExpanding((LPNMHDR)lParam);
					break;
				}
				case TVN_GETDISPINFO:
				{
					CFileManagerView::getInstance().OnTVGetDisInfo((LPNMHDR)lParam);
					break;
				}
				}
				
			}
			
		}
		break;
	}
	case WM_DESTROY:
	{
		::PostQuitMessage(0);
		break;
	}
	default:
		break;
	}

	//else if (uMsg == WM_NCCALCSIZE)
	//{
	//	return 0;
	//}
	LRESULT lRes = 0;
	if (m_pm.MessageHandler(uMsg, wParam, lParam, lRes)) return lRes;
	return CWindowWnd::HandleMessage(uMsg, wParam, lParam);
}
CControlUI* CMainWnd::CreateControl(LPCTSTR pstrClassName)
{

	if (_tcsicmp(pstrClassName, _T("MFCTree")) == 0)
	{
		//win32按钮
		CMFCTreeView* pUI = new CMFCTreeView();
		CTreeCtrl* tree = pUI->CreateTree(m_hWnd, FILE_TREE_ID);
		//CWnd* wnd = new CWnd();
		//wnd->Attach(m_hWnd);
		//CTreeCtrl* m_TreeCtrl = new CTreeCtrl;
		//m_TreeCtrl->Create(WS_VISIBLE | WS_TABSTOP | WS_CHILD | WS_BORDER |
		//	TVS_HASBUTTONS | TVS_LINESATROOT | TVS_HASLINES |
		//	TVS_DISABLEDRAGDROP | TVS_NOTOOLTIPS | TVS_EDITLABELS,
		//	CRect(0, 0, 100, 100), wnd, 1001);
		//pUI->Attach(m_TreeCtrl->GetSafeHwnd());

		CFileManagerView::getInstance().SetTreeCtrl(tree);


		return pUI;
	}

	return NULL;
}
