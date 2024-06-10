#include "KernelView.h"
#include "DriverInfo.h"

int CKernelView::PopProcessMenu()
{
	return 0;
}

int CKernelView::DispatcherMenu(int menuIndex, void* p)
{
	return 0;
}

void CKernelView::ShowSysMod()
{
	std::vector<PSysModItem> vec = CDriverInfo::getInstance().getSysModList();
	CListUI* pList = static_cast<CListUI*>(m_pPaintManager->FindControl(_T("SysModList")));
	pList->RemoveAll();
	// 测试数据
	//for (int i = 5; i < 50; i++)
	//{
	//	CListTextElementUI* pListElement = new CListTextElementUI;
	//	pListElement->SetTag(i);
	//	pList->Add(pListElement);

	//	pListElement->SetText(0, L"123");
	//	pListElement->SetText(1, L"123");
	//	pListElement->SetText(2, L"123");
	//	pListElement->SetText(3, L"123");
	//	pListElement->SetText(4, L"123");
	//	pListElement->SetText(5, L"123");
	//}
	CDuiString str;
	// 添加List列表内容，必须先Add(pListElement)，再SetText
	for (int i = 0; i < vec.size(); i++)
	{
		CListTextElementUI* pListElement = new CListTextElementUI;
		// pListElement->SetTag(vec[i]->process->pid);
		// pListElement->SetUserData(L"ProcessList");
		pList->Add(pListElement);

		std::string modName = CommonUtils::GetFileNameFromPath(vec[i]->path);
		str.Format(_T("%s"), CommonUtils::Ansi2Unicode(modName).c_str());
		pListElement->SetText(0, str);

		str.Format(_T("0x%llX"), vec[i]->base);
		pListElement->SetText(1, str);

		str.Format(_T("0x%llX"), vec[i]->size);
		pListElement->SetText(2, str);

		str.Format(_T("0x%llX"), vec[i]->driver_object);
		pListElement->SetText(3, str);

		str.Format(_T("%s"), CommonUtils::Ansi2Unicode(vec[i]->driver_name).c_str());
		pListElement->SetText(4, str);

		std::string path = vec[i]->path;
		CommonUtils::StrReplace(path, "\\??\\", "");
		std::string sysroot = "\\SystemRoot";
		auto pos = path.find(sysroot);
		if (pos == 0) path.replace(0, sysroot.size(), CommonUtils::GetWindowsPath());
		str.Format(_T("%s"), CommonUtils::Ansi2Unicode(path).c_str());
		pListElement->SetText(5, str);

		str.Format(_T("%d"), vec[i]->load_seq);
		pListElement->SetText(6, str);
		
	}
}
