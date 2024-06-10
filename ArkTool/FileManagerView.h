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

	// �ļ����Ĳ���
	void LoadFileList();
	void RemoveAllChildItem(HTREEITEM hTree);

	int OnTVGetDisInfo(LPNMHDR pnmh);
	int OnTVItemExpanding(LPNMHDR pnmh);

	// �˵�
	int PopFileManagerMenu(bool selectFile);
	int DispatcherMenu(int menuIndex, std::wstring filepath);
	int DispatcherMenu(int menuIndex, std::string filepath);

	// ˢ��
	void RefreshListUI();

private:
	int GetIconIndex(LPCTSTR lpszPath);
	HTREEITEM InsertTreeItem(LPCSTR szPath, HTREEITEM hParent);

	
private:
	CTreeCtrl* m_fileTree;
	CImageList m_imageList;
	CEditUI* m_addressEdit;
	CListUI* m_fileList;

	// �������
	bool m_deleteSrc; // ճ����ɺ��Ƿ�ɾ��ԭ�ļ�
	std::wstring m_srcPath; // ԭ�ļ�·��
	std::string curDirPath; // ��ǰĿ¼
};

