#include "security.h"
#include "framework.h"

bool OpProcessPrivilege(const wchar_t* privName, bool isOpen)
{
	typedef BOOL(WINAPI* __OpenProcessToken)(
		__in        HANDLE ProcessHandle,
		__in        DWORD DesiredAccess,
		__deref_out PHANDLE TokenHandle
		);
	typedef BOOL(WINAPI* __LookupPrivilegeValueW)(
		__in_opt LPCWSTR lpSystemName,
		__in     LPCWSTR lpName,
		__out    PLUID   lpLuid
		);
	typedef BOOL(WINAPI* __AdjustTokenPrivileges)(
		__in      HANDLE TokenHandle,
		__in      BOOL DisableAllPrivileges,
		__in_opt  PTOKEN_PRIVILEGES NewState,
		__in      DWORD BufferLength,
		__out_bcount_part_opt(BufferLength, *ReturnLength) PTOKEN_PRIVILEGES PreviousState,
		__out_opt PDWORD ReturnLength
		);
	bool result = false;
	HANDLE token;
	LUID name_value;
	TOKEN_PRIVILEGES tp;
	do {
		HMODULE advapi32 = GetModuleHandleW(L"advapi32.dll");
		if (advapi32 == NULL) {
			advapi32 = LoadLibraryW(L"advapi32.dll");
			if (advapi32 == NULL) break;
		}
		__OpenProcessToken pOpenProcessToken = (__OpenProcessToken)GetProcAddress(advapi32, "OpenProcessToken");
		__LookupPrivilegeValueW pLookupPrivilegeValueW = (__LookupPrivilegeValueW)GetProcAddress(advapi32, "LookupPrivilegeValueW");
		__AdjustTokenPrivileges pAdjustTokenPrivileges = (__AdjustTokenPrivileges)GetProcAddress(advapi32, "AdjustTokenPrivileges");
		if (pOpenProcessToken == NULL || pLookupPrivilegeValueW == NULL || pAdjustTokenPrivileges == NULL) {
			break;
		}
		if (!pOpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)) {
			printf("OpenProcessToken err:%d", GetLastError());
			break;
		}
		if (!pLookupPrivilegeValueW(NULL, privName, &name_value)) {
			printf("LookupPrivilegeValueW err:%d", GetLastError());
			CloseHandle(token);
			break;
		}
		tp.PrivilegeCount = 1;
		tp.Privileges[0].Luid = name_value;
		if (isOpen)
			tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		else
			tp.Privileges[0].Attributes = 0;
		if (!pAdjustTokenPrivileges(token, FALSE, &tp, sizeof(tp), NULL, NULL) ||
			GetLastError() != ERROR_SUCCESS) {
			printf("AdjustTokenPrivileges err:%d", GetLastError());
			CloseHandle(token);
			break;
		}
		CloseHandle(token);
		result = true;
	} while (0);
	return result;
}

bool SeEnableDebugPrivilege(bool isOpen)
{
	return OpProcessPrivilege(SE_DEBUG_NAME, isOpen);
}
