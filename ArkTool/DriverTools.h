#pragma once
#include "framework.h"
class CDriverTools {
	
public:
	static CDriverTools& getInstance() {
		static CDriverTools instance;
		return instance;
	}

	DWORD SendIoctl(LPVOID inBuffer, DWORD inBufferSize, LPVOID* pOutBuffer, LPDWORD bytesReturned);
	DWORD SendIoctlEx(LPVOID inBuffer, DWORD inBufferSize, LPVOID outBuffer, DWORD outBufferSize, LPDWORD bytesReturned);

private:
	CDriverTools();
	~CDriverTools();

	bool Connect();
	void Disconnect();

	HANDLE m_driverHandle;
};


