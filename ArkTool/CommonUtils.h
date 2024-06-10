#pragma once
#include <string>
#include <map>
namespace CommonUtils
{
	std::wstring Ansi2Unicode(std::string ansiStr);
	std::string	Unicode2Ansi(std::wstring wStr);
	std::string intToHex(long long num);

	// 路径转换
	std::string DosPathToNtPath(const std::string& strPath);
	std::string NtPathToDosPath(const std::string& strPath);
	std::string GetFileNameFromPath(std::string fullPath);
	std::wstring GetFileNameFromPath(std::wstring fullPath);
	std::string GetFilePathFromPath(std::string fullPath);
	std::string GetWindowsPath();

	std::wstring GetCurExeDir(); // 获取当前可执行文件目录

	// 路径拼接
	std::string combinePath(const std::string& path1, const std::string& path2);
	std::wstring combinePath(const std::wstring& path1, const std::wstring& path2);


	// 获取一个临时文件
	std::string getUniqueTempFilePath();
	std::string readFile(std::string filepath);
	bool writeFile(std::string filepath, const char* data, int size);

	// 字符串操作
	bool StrReplace(std::string& source, const std::string& pattern, std::string replaced);
	bool StrReplace(std::wstring& source, const std::wstring& pattern, std::wstring replaced);

	// 时间
	std::string timestampToString(time_t timestamp);
	// 将 Windows 文件时间转换为 Unix 时间戳
	time_t fileTimeToUnixTime(time_t timestamp);
};