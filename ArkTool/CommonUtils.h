#pragma once
#include <string>
#include <map>
namespace CommonUtils
{
	std::wstring Ansi2Unicode(std::string ansiStr);
	std::string	Unicode2Ansi(std::wstring wStr);
	std::string intToHex(long long num);

	// ·��ת��
	std::string DosPathToNtPath(const std::string& strPath);
	std::string NtPathToDosPath(const std::string& strPath);
	std::string GetFileNameFromPath(std::string fullPath);
	std::wstring GetFileNameFromPath(std::wstring fullPath);
	std::string GetFilePathFromPath(std::string fullPath);
	std::string GetWindowsPath();

	std::wstring GetCurExeDir(); // ��ȡ��ǰ��ִ���ļ�Ŀ¼

	// ·��ƴ��
	std::string combinePath(const std::string& path1, const std::string& path2);
	std::wstring combinePath(const std::wstring& path1, const std::wstring& path2);


	// ��ȡһ����ʱ�ļ�
	std::string getUniqueTempFilePath();
	std::string readFile(std::string filepath);
	bool writeFile(std::string filepath, const char* data, int size);

	// �ַ�������
	bool StrReplace(std::string& source, const std::string& pattern, std::string replaced);
	bool StrReplace(std::wstring& source, const std::wstring& pattern, std::wstring replaced);

	// ʱ��
	std::string timestampToString(time_t timestamp);
	// �� Windows �ļ�ʱ��ת��Ϊ Unix ʱ���
	time_t fileTimeToUnixTime(time_t timestamp);
};