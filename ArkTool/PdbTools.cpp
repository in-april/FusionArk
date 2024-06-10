#include "framework.h"
#include "PdbTools.h"

std::map<std::wstring, int> GetStruct(std::wstring name)
{
	std::map< std::wstring, int> ret;
	DWORD  error;
	HANDLE hCurrentProcess;
	HANDLE hProcess;

	SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);

	hCurrentProcess = GetCurrentProcess();

	if (!DuplicateHandle(hCurrentProcess, hCurrentProcess, hCurrentProcess, &hProcess, 0, FALSE, DUPLICATE_SAME_ACCESS))
	{
		// DuplicateHandle failed
		error = GetLastError();
		printf("DuplicateHandle returned error : %d\n", error);
		return ret;
	}

	if (!SymInitialize(hProcess, NULL, FALSE))
	{
		// SymInitialize failed
		error = GetLastError();
		printf("SymInitialize returned error : %d\n", error);
		return ret;
	}

	CHAR  szImageName[MAX_PATH] = "C:\\Windows\\System32\\ntoskrnl.exe";
	DWORD64 dwBaseAddr = 0;
	DWORD64 dwBaseAddress = SymLoadModuleEx(hProcess,    // target process 
		NULL,        // handle to image - not used
		szImageName, // name of image file
		NULL,        // name of module - not required
		dwBaseAddr,  // base address - not required
		0,           // size of image - not required
		NULL,        // MODLOAD_DATA used for special cases 
		0);          // flags - not required
	if (dwBaseAddress)
	{
		// SymLoadModuleEx returned success
	}
	else
	{
		// SymLoadModuleEx failed
		DWORD error = GetLastError();
		printf("SymLoadModuleEx returned error : %d\n", error);
	}

	//IMAGEHLP_MODULEW64 im;
	//ZeroMemory(&im, sizeof(im));
	//im.SizeOfStruct = sizeof(im);
	//SymGetModuleInfoW64(hProcess, dwBaseAddress, &im);
	BYTE buffer[sizeof(PSYMBOL_INFOW) + sizeof(TCHAR) * MAX_SYM_NAME];
	ZeroMemory(buffer, sizeof(buffer));
	PSYMBOL_INFOW pSymbol = (PSYMBOL_INFOW)buffer;
	pSymbol->SizeOfStruct = sizeof(SYMBOL_INFOW);
	pSymbol->MaxNameLen = MAX_SYM_NAME;

	// Get Type Info by Symbol Name (we need the index)
	SymGetTypeFromNameW(hProcess, dwBaseAddress, name.c_str(), pSymbol);

	// Get Child Count
	BYTE buffer2[sizeof(TI_FINDCHILDREN_PARAMS) + sizeof(TCHAR) * MAX_SYM_NAME];
	ZeroMemory(buffer2, sizeof(buffer2));
	TI_FINDCHILDREN_PARAMS* ChildParams = (TI_FINDCHILDREN_PARAMS*)buffer2;
	SymGetTypeInfo(hProcess, dwBaseAddress, pSymbol->TypeIndex, TI_GET_CHILDRENCOUNT, &ChildParams->Count);


	SymGetTypeInfo(hProcess, dwBaseAddress, pSymbol->TypeIndex, TI_FINDCHILDREN, ChildParams);
	for (ULONG i = ChildParams->Start; i < ChildParams->Count; ++i) {
		// Get Child Name
		PWCHAR pSymName = nullptr;
		SymGetTypeInfo(hProcess, dwBaseAddress, ChildParams->ChildId[i], TI_GET_SYMNAME, &pSymName);

		// Get Child Offset
		DWORD dwOffset;
		SymGetTypeInfo(hProcess, dwBaseAddress, ChildParams->ChildId[i], TI_GET_OFFSET, &dwOffset);
		//std::wcout << L"+0x" << std::hex << dwOffset << L" " << pSymName << std::endl;
		ret[std::wstring(pSymName)] = dwOffset;
		LocalFree(pSymName);
	}

	// Unload Module
	SymUnloadModule64(hProcess, dwBaseAddress);
	SymCleanup(hProcess);
	return ret;
}

bool InitOffsetAll(Offset* offset)
{
	// std::map<std::wstring, int> kprocessMap = GetStruct(L"_KPROCESS");
	std::map<std::wstring, int> eprocessMap = GetStruct(L"_EPROCESS");
	if (eprocessMap.empty()) return false;
	offset->eprocess.CreateTime = eprocessMap[L"CreateTime"];
	offset->eprocess.Flags2 = eprocessMap[L"Flags2"];
	offset->eprocess.InheritedFromUniqueProcessId = eprocessMap[L"InheritedFromUniqueProcessId"];


	std::map<std::wstring, int> drvobjMap = GetStruct(L"_DRIVER_OBJECT");
	if (drvobjMap.empty()) return false;
	offset->drvobj.DeviceObject = drvobjMap[L"DeviceObject"];
	offset->drvobj.DriverStart = drvobjMap[L"DriverStart"];
	offset->drvobj.Flags = drvobjMap[L"Flags"];

	return true;
}
