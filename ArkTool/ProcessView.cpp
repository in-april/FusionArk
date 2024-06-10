#include "ProcessView.h"
#include "ProcessInfo.h"

void CProcessView::ShowProcess()
{
	CProcessInfo::getInstance().GetProcessAll();

	CListUI* pList = static_cast<CListUI*>(m_pPaintManager->FindControl(_T("ProcessList")));
	pList->RemoveAll();
	auto vec = CProcessInfo::getInstance().getProcessShowData();
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
		pListElement->SetTag(vec[i]->process->pid);
		pListElement->SetUserData(L"ProcessList");
		pList->Add(pListElement);

		std::string space(vec[i]->level * 2, ' ');
		std::string processName = space + vec[i]->processName;
		str.Format(_T("%s"), CommonUtils::Ansi2Unicode(processName).c_str());
		pListElement->SetText(0, str);

		str.Format(_T("%d"), vec[i]->process->pid);
		pListElement->SetText(1, str);
		str.Format(_T("%d"), vec[i]->process->parent_pid);
		pListElement->SetText(2, str);
		str.Format(_T("%s"), CommonUtils::Ansi2Unicode(CommonUtils::NtPathToDosPath(vec[i]->process->path)).c_str());
		pListElement->SetText(3, str);
		str.Format(_T("0x%p"), vec[i]->process->eprocess);
		pListElement->SetText(4, str);
		if (vec[i]->process->r3_open == 0)
		{
			str.Format(_T("%s"), L"    拒绝");
		}
		else
		{
			str.Format(_T("%s"), L"     -");
		}

		pListElement->SetText(5, str);
	}
}

void CProcessView::ShowModList(uint64_t pid)
{
	CListUI* pList = static_cast<CListUI*>(m_pPaintManager->FindControl(_T("handleList")));
	pList->RemoveAll();
	std::vector<PHandleItem> vec = CProcessInfo::getInstance().getHandleByPid(pid);
	CDuiString str;
	for (PHandleItem& item : vec)
	{
		CListTextElementUI* pListElement = new CListTextElementUI;
		pListElement->SetTag(item->value);
		pList->Add(pListElement);

		str.Format(_T("%s"), CommonUtils::Ansi2Unicode(item->type_name).c_str());
		pListElement->SetText(0, str);

		str.Format(_T("0x%x"), item->value);
		pListElement->SetText(1, str);

		str.Format(_T("0x%p"), item->obj_address);
		pListElement->SetText(2, str);

		str.Format(_T("%s"), CommonUtils::Ansi2Unicode(item->obj_name).c_str());
		pListElement->SetText(3, str);

		str.Format(_T("0x%x"), item->access);
		pListElement->SetText(4, str);
	}
}

void CProcessView::ShowHandleList(uint64_t pid)
{
	CListUI* pModList = static_cast<CListUI*>(m_pPaintManager->FindControl(_T("moduleList")));
	pModList->RemoveAll();
	CDuiString str;
	std::vector<ModuleItem> modVec = CProcessInfo::getInstance().getModuleList(pid);
	for (auto& item : modVec)
	{
		CListTextElementUI* pListElement = new CListTextElementUI;
		pListElement->SetTag(item.image_base);
		pModList->Add(pListElement);

		str.Format(_T("%s"), CommonUtils::Ansi2Unicode(item.name).c_str());
		pListElement->SetText(0, str);

		str.Format(_T("0x%p"), item.image_base);
		pListElement->SetText(1, str);

		str.Format(_T("%dKB"), item.image_size / 1024);
		pListElement->SetText(2, str);

		str.Format(_T("%s"), CommonUtils::Ansi2Unicode(item.path).c_str());
		pListElement->SetText(3, str);
	}
}

void CProcessView::ShowMemList(uint64_t pid)
{
	CListUI* pMemList = static_cast<CListUI*>(m_pPaintManager->FindControl(_T("memList")));
	pMemList->RemoveAll();
	CDuiString str;
	std::vector<MemItem> memVec = CProcessInfo::getInstance().getMemList(pid);
	for (auto& item : memVec)
	{
		CListTextElementUI* pListElement = new CListTextElementUI;
		pListElement->SetTag(item.base);
		pMemList->Add(pListElement);

		str.Format(_T("0x%p"), item.base);
		pListElement->SetText(0, str);

		str.Format(_T("0x%x"), item.size);
		pListElement->SetText(1, str);

		std::string attr;
		if (item.attr & PAGE_EXECUTE) attr += "PAGE_EXECUTE | ";
		if (item.attr & PAGE_EXECUTE_READ) attr += "PAGE_EXECUTE_READ | ";
		if (item.attr & PAGE_EXECUTE_READWRITE) attr += "PAGE_EXECUTE_READWRITE | ";
		if (item.attr & PAGE_EXECUTE_WRITECOPY) attr += "PAGE_EXECUTE_WRITECOPY | ";
		if (item.attr & PAGE_NOACCESS) attr += "PAGE_NOACCESS | ";
		if (item.attr & PAGE_READONLY) attr += "PAGE_READONLY | ";
		if (item.attr & PAGE_READWRITE) attr += "PAGE_READWRITE | ";
		if (item.attr & PAGE_GUARD) attr += "PAGE_GUARD | ";
		if (item.attr & PAGE_NOCACHE) attr += "PAGE_NOCACHE | ";
		if (item.attr & PAGE_WRITECOMBINE) attr += "PAGE_WRITECOMBINE | ";
		if (!attr.empty())
			attr = attr.substr(0, attr.size() - 3);
		str.Format(_T("%s"), CommonUtils::Ansi2Unicode(attr).c_str());
		pListElement->SetText(2, str);

		std::string strStatus;
		if (item.status & MEM_COMMIT) strStatus += "MEM_COMMIT | ";
		if (item.status & MEM_FREE) strStatus += "MEM_FREE | ";
		if (item.status & MEM_RESERVE) strStatus += "MEM_RESERVE | ";
		if (!strStatus.empty())
			strStatus = strStatus.substr(0, strStatus.size() - 3);
		str.Format(_T("%s"), CommonUtils::Ansi2Unicode(strStatus).c_str());
		pListElement->SetText(3, str);

		std::string strType;
		if (item.type & MEM_PRIVATE) strType += "MEM_PRIVATE | ";
		if (item.type & MEM_IMAGE) strType += "MEM_IMAGE | ";
		if (item.type & MEM_MAPPED) strType += "MEM_MAPPED | ";
		if (!strType.empty())
			strType = strType.substr(0, strType.size() - 3);
		str.Format(_T("%s"), CommonUtils::Ansi2Unicode(strType).c_str());
		pListElement->SetText(4, str);

		str.Format(_T("0x%x"), item.alloc_base);
		pListElement->SetText(5, str);

		str.Format(_T("%s"), CommonUtils::Ansi2Unicode(item.path).c_str());
		pListElement->SetText(6, str);
	}
}

int CProcessView::PopProcessMenu()
{
	// 获取鼠标坐标
	POINT point;
	GetCursorPos(&point);
	// 右击后点别地可以清除“右击出来的菜单”
	HWND hWnd = m_pPaintManager->GetPaintWindow();
	// SetForegroundWindow(hWnd);
	// 生成托盘菜单
	HMENU hPopup = CreatePopupMenu();
	AppendMenu(hPopup, MF_STRING | MF_ENABLED, (UINT_PTR)RefreshProcess, _T("刷新"));
	AppendMenu(hPopup, MF_SEPARATOR, NULL, NULL);
	AppendMenu(hPopup, MF_STRING | MF_ENABLED, KillProcess, _T("结束进程"));
	// AppendMenu(hPopup, MF_STRING | MF_ENABLED, 3, _T("结束进程树"));
	AppendMenu(hPopup, MF_STRING | MF_ENABLED, SuspendProcess, _T("挂起进程"));
	AppendMenu(hPopup, MF_STRING | MF_ENABLED, ResumeProcess, _T("恢复进程"));
	AppendMenu(hPopup, MF_SEPARATOR, NULL, NULL);
	// AppendMenu(hPopup, MF_STRING | MF_ENABLED, 5, _T("进程保护"));
	// AppendMenu(hPopup, MF_STRING | MF_ENABLED, 5, _T("注入dll"));
	// AppendMenu(hPopup, MF_SEPARATOR, NULL, NULL);
	AppendMenu(hPopup, MF_STRING | MF_ENABLED, SaveDump, _T("创建Dump文件"));
	AppendMenu(hPopup, MF_STRING | MF_ENABLED, DumpString, _T("提取内存字符串"));
	int cmd = TrackPopupMenu(hPopup, TPM_RETURNCMD, point.x, point.y, 0, hWnd, NULL);
	return cmd;
}

int CProcessView::DispatcherMenu(int menuIndex, void* p)
{
	switch (menuIndex)
	{
	case RefreshProcess:
	{
		ShowProcess();
		break;
	}
	case KillProcess:
	{
		int ret = CProcessInfo::getInstance().killProcessByPid((uint64_t)p);
		if (ret) MessageBoxA(NULL, "关闭进程失败", "Info", MB_OK);
		ShowProcess();
		break;
	}
	case SuspendProcess:
	{
		int ret = CProcessInfo::getInstance().suspendProcess((uint64_t)p);
		if (ret) MessageBoxA(NULL, "挂起进程失败", "Info", MB_OK);
		break;
	}
	case ResumeProcess:
	{
		int ret = CProcessInfo::getInstance().resumeProcess((uint64_t)p);
		if (ret) MessageBoxA(NULL, "恢复进程失败", "Info", MB_OK);
		break;
	}
	case SaveDump:
	{
		CString defaultFileName = _T("default.dmp"); // 默认的文件名
		CString filter = _T("Dump Files (*.dmp)|*.dmp||");

		CFileDialog dlg(FALSE, _T("dmp"), defaultFileName, OFN_OVERWRITEPROMPT, filter, NULL);

		if (dlg.DoModal() == IDOK)
		{
			CString filePath = dlg.GetPathName();
			// 在这里执行保存文件的操作，使用 filePath 变量保存用户选择的文件路径
			int ret = CProcessInfo::getInstance().createDumpFile((uint64_t)p, filePath.GetBuffer(), false);
			filePath.ReleaseBuffer();
			if(!ret)
				MessageBoxA(NULL, "创建Dump失败", "Info", MB_OK);
		}
		break;
	}
	case DumpString:
	{
		vector<MemItem> memList = CProcessInfo::getInstance().getMemList((uint64_t)p);
		std::string str;
		for (auto& mem : memList)
		{
			if (mem.status & MEM_COMMIT)
			{
				// 过滤掉系统dll
				std::string path = mem.path;
				if (path.find("\\Windows\\") != std::string::npos) continue;

				// 过滤map类型中的只读内存
				if((mem.type & MEM_MAPPED) && (mem.attr & PAGE_READONLY)) continue;

				std::string tmp;
				tmp.resize(mem.size);
				int ret = CProcessInfo::getInstance().readVirtualMemory((uint64_t)p, (void*)mem.base, tmp.size(), (void*)tmp.data());
				if (ret) continue;
				printf("read address %llx, size %lld\n", mem.base, tmp.size());
				auto tmpData = extract_all_strings((uint8_t*)tmp.data(), tmp.size(), 5, true);
				
				if (tmpData.empty()) continue;

				str += "[0x" + CommonUtils::intToHex(mem.base) + "]\r\n";

				if (!path.empty())
					str += "[" + path + "]\r\n";

				for (const auto& item : tmpData) {
					//std::cout << std::get<0>(item) << ", " << std::get<1>(item) << ", (" << std::get<2>(item).first
					//	<< ", " << std::get<2>(item).second << "), " << std::get<3>(item) << std::endl;

					str += "[" + std::get<1>(item) + "]" + "[0x" + CommonUtils::intToHex(std::get<2>(item).first + mem.base) + "]"
						+ " : " + std::get<0>(item) + "\r\n";
				}

				str += "\r\n\r\n";
			}
			
		}
		std::string filepath = CommonUtils::getUniqueTempFilePath();
		CommonUtils::writeFile(filepath, str.data(), str.size());
		printf("filename is %s\n", filepath.c_str());


		if (::ShellExecuteA(NULL, "open", "notepad.exe", filepath.c_str(), NULL, SW_SHOWNORMAL) < (HINSTANCE)32)
		{
			DeleteFileA(filepath.c_str());
		}
		break;
	}
	default:
		break;
	}
	return 0;
}
