#include "FSTools.h"
#include "DriverTools.h"

CFSTools::CFSTools()
{
	m_majorOrder = MajorOrder::FsCtrl;
}

int CFSTools::findFirstFile(std::string dir, PFirstFileInfo firstInfo)
{
	CMD cmd(m_majorOrder, MinorOrder::FindFirstFile);

	std::wstring wdir = CommonUtils::Ansi2Unicode(dir);

	memcpy(cmd.data, wdir.c_str(), wdir.size() * 2);
	
	DWORD retLen = 0;
	DWORD ret = CDriverTools::getInstance().SendIoctlEx(&cmd, sizeof(cmd), (LPVOID*)firstInfo, sizeof(FirstFileInfo), &retLen);

	if (ret || retLen == 0) return -1;
	return 0;
}

int CFSTools::findNextFile(int fd, PFileInfo info)
{
	CMD cmd(m_majorOrder, MinorOrder::FindNextFile);
	cmd.fd = fd;

	DWORD retLen = 0;
	DWORD ret = CDriverTools::getInstance().SendIoctlEx(&cmd, sizeof(cmd), (LPVOID*)info, sizeof(FileInfo), &retLen);

	if (ret || retLen == 0) return -1;
	return 0;
}

int CFSTools::findClose(int fd)
{
	CMD cmd(m_majorOrder, MinorOrder::FindNextFile);
	cmd.fd = fd;

	DWORD retLen = 0;
	DWORD ret = CDriverTools::getInstance().SendIoctlEx(&cmd, sizeof(cmd), nullptr, 0, &retLen);
	return ret;
}

bool CFSTools::hasSubDir(std::string dir)
{
	FirstFileInfo firstInfo = { 0 };
	CFSTools::findFirstFile(dir, &firstInfo);

	if (firstInfo.fd == 0) return false;
	int fd = firstInfo.fd;
	
	PFileInfo fileInfo = &firstInfo.file_info;

	// 列出找到的所有文件和目录
	do {
		std::wstring filename = fileInfo->filename;
		if (filename == L"." || filename == L"..")
		{
			continue;
		}
		if (fileInfo->attr & FILE_ATTRIBUTE_DIRECTORY) {
			findClose(fd);
			return true;
		}
		
	} while (findNextFile(fd, fileInfo) == 0);
	findClose(fd);
	return false;
}

std::vector<FileInfo> CFSTools::getSubFiles(std::string dir)
{
	std::vector<FileInfo> ret;
	FirstFileInfo firstInfo = { 0 };
	CFSTools::findFirstFile(dir, &firstInfo);

	if (firstInfo.fd == 0) return ret;
	int fd = firstInfo.fd;

	PFileInfo fileInfo = &firstInfo.file_info;

	// 列出找到的所有文件和目录
	do {
		std::wstring filename = fileInfo->filename;
		if (filename == L"." || filename == L"..")
		{
			continue;
		}
		ret.push_back(*fileInfo);

	} while (findNextFile(fd, fileInfo) == 0);
	findClose(fd);
	return ret;
}

bool CFSTools::copyFile(std::wstring srcPath, std::wstring destPath, bool deleteSrc)
{
	CMD cmd(m_majorOrder, MinorOrder::CopyFile);
	memcpy(cmd.srcPath, srcPath.c_str(), srcPath.size() * 2);
	memcpy(cmd.destPath, destPath.c_str(), destPath.size() * 2);
	cmd.bDeleteSrcFile = deleteSrc;
	DWORD retLen = 0;
	DWORD ret = CDriverTools::getInstance().SendIoctlEx(&cmd, sizeof(cmd), nullptr, 0, &retLen);
	return ret;
}

bool CFSTools::copyFile(std::string srcPath, std::string destPath, bool deleteSrc)
{
	std::wstring src = CommonUtils::Ansi2Unicode(srcPath);
	std::wstring dest = CommonUtils::Ansi2Unicode(destPath);
	return copyFile(src, dest, deleteSrc);
}

bool CFSTools::deleteFile(std::wstring filepath)
{
	CMD cmd(m_majorOrder, MinorOrder::DeleteFile);
	memcpy(cmd.srcPath, filepath.c_str(), filepath.size() * 2);

	DWORD retLen = 0;
	DWORD ret = CDriverTools::getInstance().SendIoctlEx(&cmd, sizeof(cmd), nullptr, 0, &retLen);
	return ret;
}

bool CFSTools::renameFile(std::wstring srcPath, std::wstring destName, bool replaceIfExists)
{
	CMD cmd(m_majorOrder, MinorOrder::RenameFile);
	memcpy(cmd.srcPath, srcPath.c_str(), srcPath.size() * 2);
	memcpy(cmd.destPath, destName.c_str(), destName.size() * 2);
	cmd.bReplaceIfExists = replaceIfExists;
	DWORD retLen = 0;
	DWORD ret = CDriverTools::getInstance().SendIoctlEx(&cmd, sizeof(cmd), nullptr, 0, &retLen);
	return ret;
}
