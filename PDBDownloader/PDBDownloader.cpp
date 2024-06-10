#include <iostream>
#include <Windows.h>
#include "FileDownloader.h"
#include "PEHeaderReader.h"

int main()
{
	char* ntPath = "C:\\Windows\\System32\\ntoskrnl.exe";
	char* pdbName = 0;
	char url[UCHAR_MAX] = { 0 };

	PEHeaderReader(ntPath, url);
	if (*url) {
		printf("PDB Download URL: %s\n", url);
		pdbName = strrchr(url, '/');
		++pdbName;
		FileDownloader(pdbName, url);
	}
	else {
		printf("URL not found\n");
		printf("下载pdb失败\n");
	}
	system("下载pdb成功\n");
}
