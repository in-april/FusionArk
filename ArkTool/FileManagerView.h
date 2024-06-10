#pragma once
#include "DuiViewBase.h"
#include "MFCTreeCtrl.h"

enum FileManagerMenu {
	FileRefresh = 1,
	FileCut,
	FileCopy,
	FilePaste,
	FileDelete,
	FileRename,
};

typedef struct _ItemData
{
	std::string	path;
} ItemData, * PItemData;

class CFileManagerView : public CDuiViewBase
{

private:
	CFileManagerView();
	~CFileManagerView()
	{
		if (m_fileTree)
		{
			m_fileTree->DestroyWindow();
			delete m_fileTree;
		}
	}
public:
	static CFileManagerView& getInstance() {
		static CFileManagerView instance;
		return instance;
	}
	void Init();

	void ShowRoot();
	void ShowDirAllFiles(std::string dirpath);
	void SetTreeCtrl(CTreeCtrl* p);

	// 文件树的操作
	void LoadFileList();
	void RemoveAllChildItem(HTREEITEM hTree);

	int OnTVGetDisInfo(LPNMHDR pnmh);
	int OnTVItemExpanding(LPNMHDR pnmh);

	// 菜单
	int PopFileManagerMenu(bool selectFile);
	int DispatcherMenu(int menuIndex, std::wstring filepath);
	int DispatcherMenu(int menuIndex, std::string filepath);

	// 刷新
	void RefreshListUI();

private:
	int GetIconIndex(LPCTSTR lpszPath);
	HTREEITEM InsertTreeItem(LPCSTR szPath, HTREEITEM hParent);

	
private:
	CTreeCtrl* m_fileTree;
	CImageList m_imageList;
	CEditUI* m_addressEdit;
	CListUI* m_fileList;

	// 复制相关
	bool m_deleteSrc; // 粘贴完成后是否删除原文件
	std::wstring m_srcPath; // 原文件路径
	std::string curDirPath; // 当前目录
};

