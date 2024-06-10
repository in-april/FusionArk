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

	// ��ȡĿ¼�������ļ����ļ���
	std::vector<FileInfo> getSubFiles(std::string dir);

	// �����ļ�
	bool copyFile(std::string srcPath, std::string destPath, bool deleteSrc);
	bool copyFile(std::wstring srcPath, std::wstring destPath, bool deleteSrc);

	// ɾ���ļ�
	bool deleteFile(std::wstring filepath);

	// �������ļ�
	bool renameFile(std::wstring srcPath, std::wstring destName, bool replaceIfExists);
};

