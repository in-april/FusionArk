#pragma once
#include "Entry.h"
#include "typedef.h"
#include "FSCtrlIrp.h"

// 自定义的文件描述符
typedef struct _FD
{
	PDEVICE_OBJECT pDevObj; // 卷设备
	PFILE_OBJECT pFileObj; // 文件对象指针
	uint64_t readOffset; // 读文件的偏移
	uint64_t writeOffset; // 写文件的偏移
}FD, * PFD;

// 驱动句柄上下文，该内存在打开驱动设备时创建，放着FileObject->FsContext中
typedef struct _HandleContext
{
	std::map<int, FD> vecFD;
}HandleContext, * PHandleContext;

namespace FSTools
{
	NTSTATUS dispatcher(MinorOrder subOrder, PCMD pCmd, PVOID outBuffer, ULONG outBufferSize, PHandleContext context, PULONG retLength);
	
	// r0结构和r3结构转换
	void structTransfer(PFIND_FILE_OUTPUT in, PFileInfo out);

	// 生成一个文件描述符
	int32_t genFd();
	
	// 找到的第一个文件或目录的信息
	NTSTATUS FindFirstFile(PHandleContext context, LPCWSTR dirPath, PVOID outBuffer, PULONG retLength);

	// 查找下一个文件或目录的信息
	NTSTATUS FindNextFile(PHandleContext context, int fd, PVOID outBuffer, PULONG retLength);

	// 停止目录项的查询
	NTSTATUS FindClose(PHandleContext context, int fd);

	// 删除文件
	NTSTATUS DeleteFile(LPCWSTR dirPath);

	// 重命名文件
	NTSTATUS RenameFile(LPCWSTR srcPath, LPCWSTR destPath, bool bReplaceIfExists);

	// 拷贝文件
	NTSTATUS CopyFile(LPCWSTR srcPath, LPCWSTR destPath, bool bDeleteSrcFile);

	// 打开或新建文件
	NTSTATUS CreateFile(PHandleContext context, LPCWSTR filename, PVOID outBuffer, PULONG retLength);

	// 写入文件
	NTSTATUS WriteFile(PHandleContext context, int fd, PVOID data, PVOID outBuffer, PULONG retLength);

	// 读取文件
	NTSTATUS ReadFile(PHandleContext context, int fd, PVOID data, PVOID outBuffer, PULONG retLength);

	// 关闭文件
	NTSTATUS CloseFile(PHandleContext context, int fd, PVOID data, PVOID outBuffer, PULONG retLength);
};

