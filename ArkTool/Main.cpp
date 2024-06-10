#include "ArkTool.h"
#include "ProcessInfo.h"
#include "DriverTools.h"
#include "PdbTools.h"
#include "security.h"

#define DRIVER_FILENAME L"ArkDriver.sys"
#define DRIVER_NAME L"FusionArk"

DWORD LoadDriver(const TCHAR* driverPath, const TCHAR* driverName) {
	// 1. Open the SCM (Service Control Manager)
	SC_HANDLE scmHandle = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
	if (scmHandle == nullptr) {
		return GetLastError();
	}

	// 2. Check if the service already exists
	SC_HANDLE serviceHandle = OpenService(scmHandle, driverName, SERVICE_QUERY_STATUS | SERVICE_START | DELETE | SERVICE_STOP);
	if (serviceHandle == nullptr) {
		if (GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST) {
			// The service does not exist, create it
			serviceHandle = CreateService(
				scmHandle,
				driverName,
				driverName,
				SERVICE_START | DELETE | SERVICE_STOP,
				SERVICE_KERNEL_DRIVER,
				SERVICE_DEMAND_START,
				SERVICE_ERROR_NORMAL,
				driverPath,
				nullptr,
				nullptr,
				nullptr,
				nullptr,
				nullptr
			);
			if (serviceHandle == nullptr) {
				DWORD error = GetLastError();
				CloseServiceHandle(scmHandle);
				return error;
			}
		}
		else {
			DWORD error = GetLastError();
			CloseServiceHandle(scmHandle);
			return error;
		}
	}
	else {
		// The service exists, check if it's already running
		SERVICE_STATUS serviceStatus;
		if (QueryServiceStatus(serviceHandle, &serviceStatus)) {
			if (serviceStatus.dwCurrentState == SERVICE_RUNNING) {
				// Service is already running, return success
				CloseServiceHandle(serviceHandle);
				CloseServiceHandle(scmHandle);
				return ERROR_SUCCESS;
			}
		}
		else {
			DWORD error = GetLastError();
			CloseServiceHandle(serviceHandle);
			CloseServiceHandle(scmHandle);
			return error;
		}
	}

	// 3. Start the service (load the driver)
	if (!StartService(serviceHandle, 0, nullptr)) {
		DWORD error = GetLastError();
		if (error != ERROR_SERVICE_ALREADY_RUNNING) {
			DeleteService(serviceHandle);
			CloseServiceHandle(serviceHandle);
			CloseServiceHandle(scmHandle);
			return error;
		}
	}

	// 4. Clean up
	CloseServiceHandle(serviceHandle);
	CloseServiceHandle(scmHandle);

	// Success
	return ERROR_SUCCESS;
}

bool Init()
{	
	// MessageBox(0, 0, 0, 0);
	// 加载驱动
	std::wstring driverPath = CommonUtils::GetCurExeDir() + L"\\" + DRIVER_FILENAME;
	DWORD result = LoadDriver(driverPath.c_str(), DRIVER_NAME);
	if (result != ERROR_SUCCESS) {
		std::wstring message = L"Failed to load driver. Error: " + std::to_wstring(result);
		MessageBox(nullptr, message.c_str(), L"Error", MB_OK | MB_ICONERROR);
		return false;
	}


	HMODULE hModule = ::GetModuleHandle(nullptr);
	if (hModule == nullptr)
	{
		// TODO: 更改错误代码以符合需要
		printf("错误: GetModuleHandle 失败\n");
	}
	if (!AfxWinInit(hModule, nullptr, ::GetCommandLine(), 0))
	{
		// TODO: 在此处为应用程序的行为编写代码。
		printf("错误: MFC 初始化失败\n");

	}
	
	// 向驱动发送结构体偏移
	CMD cmd(MajorOrder::Init, MinorOrder::InitOffset);
	if (!InitOffsetAll(&cmd.offset))
	{
		MessageBox(nullptr, L"符号初始化失败，请先使用PDBDownloader.exe下载符号", L"Error", MB_ICONERROR | MB_OK);
		return false;
	}
	DWORD retLen = 0;
	DWORD ret = CDriverTools::getInstance().SendIoctlEx(&cmd, sizeof(cmd), nullptr, 0, &retLen);
	if (ret)
	{
		MessageBox(nullptr, L"驱动初始化失败", L"Error", MB_ICONERROR | MB_OK);
		printf("errorcode:%d", ret);
		return false;
	}	// 初始化进程类
	
	return true;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	// 提权
	if (!SeEnableDebugPrivilege())
	{

	}

	CPaintManagerUI::SetInstance(hInstance);
	CPaintManagerUI::SetResourcePath(CPaintManagerUI::GetInstancePath());

#ifdef DEBUG
	AllocConsole();
	ShowWindow(GetConsoleWindow(), SW_NORMAL);
	freopen("conin$", "r+t", stdin);
	freopen("conout$", "r+t", stdout);
	SetConsoleTitleA("Fusion调试窗口");
	system("cls");  // 调试用，卸载动态库时清空控制台
#endif // DEBUG

	// 初始化
	int ret = Init();
	if (!ret)
	{
		
		return 0;
	}

	CMainWnd* pMainWnd = new CMainWnd();
	if (pMainWnd == NULL) return 0;
	pMainWnd->Create(NULL, _T("FusionArk"), UI_WNDSTYLE_FRAME, WS_EX_WINDOWEDGE);
	CPaintManagerUI::MessageLoop();

	return 0;
}