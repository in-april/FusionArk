#include "MFCTreeCtrl.h"


CTreeCtrl* CMFCTreeView::CreateTree(HWND parentHwnd, int id)
{
	parentWnd.Attach(parentHwnd);
	CTreeCtrl* tree = new CTreeCtrl;
	tree->Create(WS_VISIBLE | WS_TABSTOP | WS_CHILD | WS_BORDER |
		TVS_HASBUTTONS | TVS_LINESATROOT | TVS_HASLINES |
		TVS_DISABLEDRAGDROP | TVS_NOTOOLTIPS | TVS_SHOWSELALWAYS,
		CRect(1,1,1,1), &parentWnd, id);

	m_hWnd = tree->GetSafeHwnd();
	return tree;
}