#include "CommonUtils.h"
#include "framework.h"
#include <fstream>
#include <ctime>
#include <iomanip>
#include <sstream>

#define DELETE_POINTER_ARRAY(p)  {if(p){ delete []p; p = NULL;}}

std::wstring CommonUtils::Ansi2Unicode(std::string ansiStr)
{
	int nByte = 0;
	wchar_t* zBuf = NULL;
	std::wstring t = L"";

	if (ansiStr.empty() || (ansiStr.length() == 0))return t;

	nByte = MultiByteToWideChar(CP_ACP, 0, ansiStr.c_str(), ansiStr.length(), 0, 0);
	if (0 == nByte)	return t;

	zBuf = new wchar_t[nByte + 1];
	if (zBuf == NULL)  return t;


	memset(zBuf, 0, sizeof(wchar_t) * (nByte + 1));
	nByte = MultiByteToWideChar(CP_ACP, 0, ansiStr.c_str(), ansiStr.length(), zBuf, nByte + 1);
	if (nByte != 0)
		t = zBuf;

	DELETE_POINTER_ARRAY(zBuf);

	return t;
}


std::string CommonUtils::Unicode2Ansi(std::wstring wStr)
{
	std::string sdata("");
	char* data = 0;

	if (wStr.empty() || (wStr.length() == 0))return sdata;

	int nReq = WideCharToMultiByte(CP_ACP, 0, wStr.c_str(), -1, 0, 0, 0, 0);
	if (0 == nReq)	return sdata;

	data = new char[sizeof(char) * nReq];
	if (data == NULL)return sdata;

	memset(data, 0, nReq * sizeof(char));
	nReq = WideCharToMultiByte(CP_ACP, 0, wStr.c_str(), -1, data, nReq, 0, 0);
	if (0 != nReq)
		sdata = data;

	DELETE_POINTER_ARRAY(data);

	return sdata;
}


std::string CommonUtils::intToHex(long long num)
{
	std::stringstream stream;
	stream << std::hex << num;
	return stream.str();

}

std::map<std::string, std::string> g_DrivePathMap;

void InitializeDrivePathMap() {
	char targetPath[MAX_PATH] = { 0 }; // ʹ��һ���㹻��Ļ������洢�豸·��

	for (char i = 'A'; i <= 'Z'; i++)
	{
		std::string str = std::string(1, i) + ":";
		DWORD result = QueryDosDeviceA(str.c_str(), targetPath, MAX_PATH);
		if (result > 0)
		{
			g_DrivePathMap[targetPath] = str;
		}
	}
}

std::string CommonUtils::DosPathToNtPath(const std::string& strPath)
{
	if (strPath.empty()) return "";

	char ntPath[MAX_PATH] = { 0 };

	std::string str;
	str.push_back(strPath[0]);
	str += ":";
	DWORD result = QueryDosDeviceA(str.c_str(), ntPath, MAX_PATH);

	if (result > 0)
	{
		int pos = strPath.find("\\");
		std::string remainPath = strPath.substr(pos);
		std::string ret = ntPath + remainPath;
		return ret;
	}

	return "";
}

std::string CommonUtils::NtPathToDosPath(const std::string& strPath)
{
	if (strPath.empty()) return strPath;
	if (strPath[0] != '\\') return strPath;

	// ���ҵ�����"\\"��λ��
	int pos = 0;
	int count = 0;
	for (pos = 0; pos < strPath.length(); pos++) {
		if (strPath[pos] == '\\') {
			count++;
			if (count == 3) {
				break;
			}
		}
	}

	if (g_DrivePathMap.empty())
	{
		InitializeDrivePathMap();
	}

	std::string ntPath = strPath.substr(0, pos);
	std::string remainPath = strPath.substr(pos);
	auto it = g_DrivePathMap.find(ntPath);
	if (it != g_DrivePathMap.end())
	{
		return it->second + remainPath;
	}
	return strPath;
}

std::string CommonUtils::GetFileNameFromPath(std::string fullPath)
{
	// �ҵ����һ��·���ָ�����λ��
	size_t lastSlashPos = fullPath.find_last_of("\\/");
	if (lastSlashPos != std::string::npos) {
		// �����һ��·���ָ��������ȡ�ַ�����Ϊ�ļ���
		return fullPath.substr(lastSlashPos + 1);
	}
	// ���û���ҵ�·���ָ�����ֱ�ӷ���ԭʼ·��
	return fullPath;

}

std::string CommonUtils::GetFilePathFromPath(std::string fullPath)
{
	// �ҵ����һ��·���ָ�����λ��
	size_t lastSlashPos = fullPath.find_last_of("\\/");
	if (lastSlashPos != std::string::npos) {
		// ��·����ʼ�����һ��·���ָ���λ�ý�ȡ�ַ�����Ϊ�ļ�·��
		return fullPath.substr(0, lastSlashPos + 1);
	}
	// ���û���ҵ�·���ָ���������һ���յ� std::wstring
	return fullPath;
}

std::wstring CommonUtils::GetFileNameFromPath(std::wstring fullPath)
{
	std::string str = GetFileNameFromPath(Unicode2Ansi(fullPath));
	return Ansi2Unicode(str);
}

std::string CommonUtils::GetWindowsPath()
{
	char windir[MAX_PATH] = { 0 };
	GetWindowsDirectoryA(windir, MAX_PATH);
	return windir;
}

std::wstring CommonUtils::GetCurExeDir()
{
	wchar_t buffer[MAX_PATH];
	GetModuleFileName(nullptr, buffer, MAX_PATH);
	std::wstring::size_type pos = std::wstring(buffer).find_last_of(L"\\/");
	return std::wstring(buffer).substr(0, pos);
}

std::string CommonUtils::combinePath(const std::string& path1, const std::string& path2)
{
	if (path1.back() == '\\' || path1.back() == '/') {
		return path1 + path2;
	}
	return path1 + "\\" + path2;
}

std::wstring CommonUtils::combinePath(const std::wstring& path1, const std::wstring& path2)
{
	if (path1.back() == '\\' || path1.back() == '/') {
		return path1 + path2;
	}
	return path1 + L"\\" + path2;
}

std::string CommonUtils::getUniqueTempFilePath()
{
	char tempPath[MAX_PATH];
	// ��ȡ��ʱ�ļ�·��
	DWORD pathLength = GetTempPathA(MAX_PATH, tempPath);
	if (pathLength > MAX_PATH || (pathLength == 0)) {
		return "";
	}

	char tempFileName[MAX_PATH];
	// ����Ψһ����ʱ�ļ���
	if (GetTempFileNameA(tempPath, "tmp", 0, tempFileName) == 0) {
		return "";
	}

	return std::string(tempFileName);
}

std::string CommonUtils::readFile(std::string filepath)
{
	std::string data;
	std::ifstream streamReader(filepath, std::ios::binary);
	if (!streamReader.is_open()) { //���ļ�ʧ��
		return data;
	}
	streamReader.seekg(0, std::ios::end);
	unsigned filesize = streamReader.tellg();
	data.resize(filesize);
	streamReader.seekg(0, std::ios::beg);
	streamReader.read((char*)data.data(), filesize);
	streamReader.close();
	return data;
}

bool CommonUtils::writeFile(std::string filepath, const char* data, int size)
{
	std::ofstream streamWriter(filepath, std::ios::binary);
	if (!streamWriter.is_open()) { //���ļ�ʧ��
		return false;
	}
	streamWriter.write(data, size);
	streamWriter.close();
	return true;
}

bool CommonUtils::StrReplace(std::string& source, const std::string& pattern, std::string replaced)
{
	try {
		bool result = false;
		if (source.empty() || pattern.empty())
			return false;
		size_t pos = 0;
		while ((pos = source.find(pattern, pos)) != std::string::npos) {
			source.replace(pos, pattern.size(), replaced);
			pos += replaced.size();
			result = true;
		}
		return result;
	}
	catch (...) {
		return false;
	}
}

std::string CommonUtils::timestampToString(time_t timestamp)
{
	// ����һ��ʱ��ṹ�壬����ʱ���ת�����ʱ����Ϣ
	struct tm timeInfo;
	// ʹ�� localtime_s ������ʱ���ת��Ϊ����ʱ��
	localtime_s(&timeInfo, &timestamp);

	// ����һ���ַ����������ڸ�ʽ�����
	std::ostringstream oss;
	// ��ָ����ʽ��ʱ����Ϣ������ַ�������
	oss << std::put_time(&timeInfo, "%Y-%m-%d %H:%M:%S");

	// ���ظ�ʽ������ַ���
	return oss.str();
}

#define WINDOWS_TICK 10000000  //10��7�η�
#define SEC_TO_UNIX_EPOCH 11644473600LL //1601��1970��ʱ����
time_t CommonUtils::fileTimeToUnixTime(time_t timestamp)
{
	return timestamp / WINDOWS_TICK - SEC_TO_UNIX_EPOCH;
}

bool CommonUtils::StrReplace(std::wstring& source, const std::wstring& pattern, std::wstring replaced)
{
	try {
		bool result = false;
		if (source.empty() || pattern.empty())
			return false;
		size_t pos = 0;
		while ((pos = source.find(pattern, pos)) != std::wstring::npos) {
			source.replace(pos, pattern.size(), replaced);
			pos += replaced.size();
			result = true;
		}
		return result;
	}
	catch (...) {
		return false;
	}
}
