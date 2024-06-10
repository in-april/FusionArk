#include "FileManagerView.h"
#include <ShlObj.h>
#include "FSTools.h"
#include "DuiDialog.h"

CFileManagerView::CFileManagerView()
{
	m_deleteSrc = false;
}

void CFileManagerView::Init()
{
	m_addressEdit = static_cast<CEditUI*>(m_pPaintManager->FindControl(_T("addressEdit")));
	m_fileList = static_cast<CListUI*>(m_pPaintManager->FindControl(_T("FileList")));
}

void CFileManagerView::ShowRoot()
{
	m_fileTree->DeleteAllItems();
	::SetWindowTheme(m_fileTree->GetSafeHwnd(), L"Explorer", NULL);

	SHFILEINFO sfi;
	HIMAGELIST hImageList = (HIMAGELIST)SHGetFileInfo(_T(""), 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
	if (hImageList && m_imageList.GetSafeHandle() == NULL)
	{
		m_imageList.Attach(hImageList);
	}
	m_fileTree->SetImageList(&m_imageList, TVSIL_NORMAL);

	DWORD dwDrives = GetLogicalDrives();
	CHAR szDrive[] = "A:\\";

	for (int i = 0; i < 26; ++i)
	{
		if (dwDrives & (1 << i))
		{
			szDrive[0] = 'A' + i;
			// HTREEITEM hDriveItem = m_fileTree->InsertItem(szDrive, GetIconIndex(szDrive), GetIconIndex(szDrive));
			InsertTreeItem(szDrive, NULL);
			// m_fileTree->SetItemData(hDriveItem, 1); // 标记节点未加载子项
		}
	}
}

void CFileManagerView::ShowDirAllFiles(std::string dirpath)
{
	std::vector<FileInfo> vec = CFSTools::getInstance().getSubFiles(dirpath);

	//m_fileTree->SetRedraw(FALSE);
	for (auto& item : vec)
	{
		std::string filename = CommonUtils::Unicode2Ansi(item.filename);
		std::string path = CommonUtils::combinePath(dirpath, filename);
		if (item.attr & FILE_ATTRIBUTE_DIRECTORY)
		{
			// InsertTreeItem(path.c_str(), hTreeSelected);
		}
		else
		{
			CListTextElementUI* pListElement = new CListTextElementUI;

			m_fileList->Add(pListElement);

			CDuiString str;
			str.Format(_T("%s"), CommonUtils::Ansi2Unicode(filename).c_str());
			pListElement->SetText(0, str);

			str.Format(_T("%d"), item.size);
			pListElement->SetText(1, str);

			str.Format(_T("%d"), item.allocation_size);
			pListElement->SetText(2, str);

			time_t fileTime = CommonUtils::fileTimeToUnixTime(item.create_time);
			str.Format(_T("%s"), CommonUtils::Ansi2Unicode(CommonUtils::timestampToString(fileTime)).c_str());
			pListElement->SetText(3, str);

			fileTime = CommonUtils::fileTimeToUnixTime(item.change_time);
			str.Format(_T("%s"), CommonUtils::Ansi2Unicode(CommonUtils::timestampToString(fileTime)).c_str());
			pListElement->SetText(4, str);

			// 隐藏数据，保存路径
			str.Format(_T("%s"), CommonUtils::Ansi2Unicode(path).c_str());
			pListElement->SetUserData(str);
		}

	}
	//m_fileTree->SetRedraw(TRUE);
	//m_fileTree->Invalidate();

}

void CFileManagerView::SetTreeCtrl(CTreeCtrl* p)
{
	m_fileTree = p;
}

void CFileManagerView::LoadFileList()
{
	//因为双击或单击时，可能还没有选择的条目，所以需要用HitTest获取结点
	CPoint mousePos;
	GetCursorPos(&mousePos);
	m_fileTree->ScreenToClient(&mousePos);
	HTREEITEM hTreeSelected = m_fileTree->HitTest(mousePos);
	if (hTreeSelected == NULL) return;
	//清空所选结点的子节点
	//RemoveAllChildItem(hTreeSelected);
	m_fileList->RemoveAll();

	PItemData data = (PItemData)m_fileTree->GetItemData(hTreeSelected);
	printf("%s\n", data->path.c_str());

	ShowDirAllFiles(data->path);
	//if (m_tree.ItemHasChildren(hTreeSelected)) return; //如果有子节点则不再更新
	curDirPath = data->path;

	m_addressEdit->SetText(CommonUtils::Ansi2Unicode(data->path).c_str());

}

void CFileManagerView::RemoveAllChildItem(HTREEITEM hTree)
{
	HTREEITEM chlid = NULL;
	do
	{
		chlid = m_fileTree->GetChildItem(hTree);
		if (chlid != NULL)
		{
			m_fileTree->DeleteItem(chlid);
		}
	} while (chlid != NULL);
}

int CFileManagerView::GetIconIndex(LPCTSTR lpszPath)
{
	SHFILEINFO sfi = { 0 };
	if (SHGetFileInfo(lpszPath, 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_SMALLICON | SHGFI_DISPLAYNAME))
	{
		return sfi.iIcon;
	}
	return 0;

}

HTREEITEM CFileManagerView::InsertTreeItem(LPCSTR szPath, HTREEITEM hParent)
{
	TVINSERTSTRUCT	tvistrc;
	SHFILEINFO		sfi = { 0 };

	std::wstring path = CommonUtils::Ansi2Unicode(szPath);
	::SHGetFileInfo(path.c_str(), 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_DISPLAYNAME);

	ItemData* data = new ItemData;
	data->path = szPath;

	tvistrc.hInsertAfter = TVI_LAST;
	tvistrc.hParent = hParent;
	tvistrc.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN;

	tvistrc.item.pszText = sfi.szDisplayName;
	tvistrc.item.cchTextMax = sizeof(sfi.szDisplayName);;

	tvistrc.item.iImage = sfi.iIcon;
	tvistrc.item.lParam = (LPARAM)data;
	tvistrc.item.iSelectedImage = sfi.iIcon;
	tvistrc.item.cChildren = I_CHILDRENCALLBACK;

	return m_fileTree->InsertItem(&tvistrc);
}

int CFileManagerView::OnTVGetDisInfo(LPNMHDR pnmh)
{
	SHFILEINFOA		sfi = { 0 };
	NMTVDISPINFO* pTVDispInfo = reinterpret_cast<NMTVDISPINFO*>(pnmh);
	PItemData pData = (PItemData)pTVDispInfo->item.lParam;

	/*::SHGetFileInfoA(pData->path.c_str(), 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_DISPLAYNAME);

	if (pTVDispInfo->item.mask & TVIF_IMAGE)
	{
		if (sfi.iIcon > 0)
			pTVDispInfo->item.iImage = sfi.iIcon;
	}
	if (pTVDispInfo->item.mask & TVIF_SELECTEDIMAGE)
	{
		if (sfi.iIcon > 0)
			pTVDispInfo->item.iImage = sfi.iIcon;
	}

	if (pTVDispInfo->item.mask & TVIF_TEXT)
	{
		std::wstring name = CommonUtils::Ansi2Unicode(sfi.szDisplayName);
		memcpy(pTVDispInfo->item.pszText, name.c_str(), name.size() * 2);
	}*/

	if (pTVDispInfo->item.mask & TVIF_CHILDREN)
	{
		if (pData->path.size() >= 3)
		{
			if (CFSTools::getInstance().hasSubDir(pData->path))
			{
				pTVDispInfo->item.cChildren = 1;
			}
			else
			{
				pTVDispInfo->item.cChildren = 0;
			}

		}
		else {
			pTVDispInfo->item.cChildren = 0;
		}
	}

	return 0;
}

int CFileManagerView::OnTVItemExpanding(LPNMHDR pnmh)
{
	NMTREEVIEW* pTreeView = reinterpret_cast<NMTREEVIEW*>(pnmh);

	if (pTreeView->action == TVE_EXPAND)
	{
		PItemData data = (PItemData)pTreeView->itemNew.lParam;
		RemoveAllChildItem(pTreeView->itemNew.hItem);
		//if (m_tree.ItemHasChildren(hTreeSelected)) return; //如果有子节点则不再更新
		std::vector<FileInfo> vec = CFSTools::getInstance().getSubFiles(data->path);

		//m_fileTree->SetRedraw(FALSE);
		for (auto& item : vec)
		{
			std::string filename = CommonUtils::Unicode2Ansi(item.filename);
			std::string path = CommonUtils::combinePath(data->path, filename);
			if (item.attr & FILE_ATTRIBUTE_DIRECTORY)
			{
				InsertTreeItem(path.c_str(), pTreeView->itemNew.hItem);
			}
		}
	}
	m_fileTree->Invalidate();
	return 0;
}

int CFileManagerView::PopFileManagerMenu(bool selectFile)
{
	// 获取鼠标坐标
	POINT point;
	GetCursorPos(&point);
	// 右击后点别地可以清除“右击出来的菜单”
	HWND hWnd = m_pPaintManager->GetPaintWindow();
	// SetForegroundWindow(hWnd);
	// 生成托盘菜单
	HMENU hPopup = CreatePopupMenu();
	AppendMenu(hPopup, MF_STRING | MF_ENABLED, (UINT_PTR)FileRefresh, _T("刷新"));
	AppendMenu(hPopup, MF_SEPARATOR, NULL, NULL);
	if (selectFile)
	{
		AppendMenu(hPopup, MF_STRING | MF_ENABLED, FileCut, _T("剪切"));
		AppendMenu(hPopup, MF_STRING | MF_ENABLED, FileCopy, _T("复制"));
	}


	if (m_srcPath.empty())
	{
		AppendMenu(hPopup, MF_STRING | MF_DISABLED, FilePaste, _T("粘贴"));
	}
	else
	{
		AppendMenu(hPopup, MF_STRING | MF_ENABLED, FilePaste, _T("粘贴"));
	}
	
	if (selectFile)
	{
		AppendMenu(hPopup, MF_SEPARATOR, NULL, NULL);
		AppendMenu(hPopup, MF_STRING | MF_ENABLED, FileRename, _T("重命名"));
		AppendMenu(hPopup, MF_STRING | MF_ENABLED, FileDelete, _T("删除"));
	}

	
	int cmd = TrackPopupMenu(hPopup, TPM_RETURNCMD, point.x, point.y, 0, hWnd, NULL);
	return cmd;
}

int CFileManagerView::DispatcherMenu(int menuIndex, std::string filepath)
{
	return DispatcherMenu(menuIndex, CommonUtils::Ansi2Unicode(filepath));
}

int CFileManagerView::DispatcherMenu(int menuIndex, std::wstring filepath)
{
	switch (menuIndex)
	{
	case FileRefresh:
	{
		RefreshListUI();
		break;
	}
	case FileCut:
	{
		m_deleteSrc = true;
		m_srcPath = filepath;
		break;
	}
	case FileCopy:
	{
		m_deleteSrc = false;
		m_srcPath = filepath;
		break;
	}
	case FilePaste:
	{
		std::wstring destName = CommonUtils::GetFileNameFromPath(m_srcPath);
		std::wstring destPath = CommonUtils::combinePath(CommonUtils::Ansi2Unicode(curDirPath), destName);
		CFSTools::getInstance().copyFile(m_srcPath, destPath, m_deleteSrc);
		RefreshListUI();
		break;
	}
	case FileRename:
	{
		DuiDialog* dialog = new DuiDialog();
		if (dialog)
		{
			dialog->m_filename = CommonUtils::GetFileNameFromPath(filepath);
			dialog->Create(m_pPaintManager->GetPaintWindow(), _T(""), UI_WNDSTYLE_DIALOG, 0, 0, 0, 0, 0, NULL);
			dialog->CenterWindow();
			dialog->ShowModal();

			CFSTools::getInstance().renameFile(filepath, dialog->m_filename, false);

			RefreshListUI();
			delete dialog;
		}
		
		break;
	}
	case FileDelete:
	{
		CFSTools::getInstance().deleteFile(filepath);
		RefreshListUI();
		break;
	}
	default:
		break;
	}
	return 0;
}

void CFileManagerView::RefreshListUI()
{
	m_fileList->RemoveAll();
	std::wstring dirpath = m_addressEdit->GetText().GetData();
	ShowDirAllFiles(CommonUtils::Unicode2Ansi(dirpath));
}
