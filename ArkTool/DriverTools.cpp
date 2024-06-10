#include "DriverTools.h"
#include "framework.h"
#include <WinIoCtl.h>
#define OPER_CMD CTL_CODE(FILE_DEVICE_UNKNOWN, 0x900, METHOD_IN_DIRECT, FILE_ANY_ACCESS)
#define SYMBOLICLINK_NAME L"\\\\.\\MyArkTools202403"

CDriverTools::CDriverTools()
{
	Connect();
}

CDriverTools::~CDriverTools() {
	Disconnect();
}

bool CDriverTools::Connect() {
	m_driverHandle = CreateFile(
		SYMBOLICLINK_NAME,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		nullptr
	);

	if (m_driverHandle == INVALID_HANDLE_VALUE) {
		// MessageBox(nullptr, L"Failed to connect to the driver.", L"Error", MB_ICONERROR | MB_OK);
		return false;
	}

	// MessageBox(nullptr, L"Connected to the driver.", L"Success", MB_ICONINFORMATION | MB_OK);
	return true;
}

void CDriverTools::Disconnect() {
	if (m_driverHandle != nullptr && m_driverHandle != INVALID_HANDLE_VALUE) {
		CloseHandle(m_driverHandle);
		m_driverHandle = nullptr;
		// MessageBox(nullptr, L"Disconnected from the driver.", L"Success", MB_ICONINFORMATION | MB_OK);
	}
}

DWORD CDriverTools::SendIoctl(LPVOID inBuffer, DWORD inBufferSize, LPVOID* pOutBuffer, LPDWORD bytesReturned) 
{
	int len = 0x20000;
	DWORD retCode = 0;
	PVOID pMem = nullptr;
	do
	{
		pMem = malloc(len);
		if (pMem == nullptr) return ERROR_BUFFER_OVERFLOW;
		memset(pMem, 0, len);
		retCode = SendIoctlEx(inBuffer, inBufferSize, pMem, len, bytesReturned);
		if (retCode == ERROR_INSUFFICIENT_BUFFER)
		{
			len += 0x10000;
			free(pMem);
			pMem = nullptr;
		}

	} while (retCode == ERROR_INSUFFICIENT_BUFFER);
	if (pOutBuffer != nullptr && !retCode)
	{
		*pOutBuffer = pMem;
	}
	else
	{
		free(pMem);
	}
		
	return retCode;
}

DWORD CDriverTools::SendIoctlEx(LPVOID inBuffer, DWORD inBufferSize, LPVOID outBuffer, DWORD outBufferSize, LPDWORD bytesReturned)
{
	if (!DeviceIoControl(
		m_driverHandle,
		OPER_CMD,
		inBuffer,
		inBufferSize,
		outBuffer,
		outBufferSize,
		bytesReturned,
		nullptr
	)) {
		int error = GetLastError();
		// MessageBox(nullptr, L"Failed to send IOCTL to driver.", L"Error", MB_ICONERROR | MB_OK);
		return error;
	}

	return 0;
}
