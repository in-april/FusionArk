#pragma once
#include "framework.h"

class CFSTools
{
private:
	CFSTools();
	MajorOrder m_majorOrder;

public:
	static CFSTools& getInstance() {
		static CFSTools instance;
		return instance;
	}

	int findFirstFile(std::string dir, PFirstFileInfo firstInfo);

	int findNextFile(int fd, PFileInfo info);

	int findClose(int fd);

	bool hasSubDir(std::string dir);

	// 获取目录中所有文件和文件夹
	std::vector<FileInfo> getSubFiles(std::string dir);

	// 复制文件
	bool copyFile(std::string srcPath, std::string destPath, bool deleteSrc);
	bool copyFile(std::wstring srcPath, std::wstring destPath, bool deleteSrc);

	// 删除文件
	bool deleteFile(std::wstring filepath);

	// 重命名文件
	bool renameFile(std::wstring srcPath, std::wstring destName, bool replaceIfExists);
};

