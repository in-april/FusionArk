#pragma once
#include "Entry.h"
#include "typedef.h"
#include "FSCtrlIrp.h"

// �Զ�����ļ�������
typedef struct _FD
{
	PDEVICE_OBJECT pDevObj; // ���豸
	PFILE_OBJECT pFileObj; // �ļ�����ָ��
	uint64_t readOffset; // ���ļ���ƫ��
	uint64_t writeOffset; // д�ļ���ƫ��
}FD, * PFD;

// ������������ģ����ڴ��ڴ������豸ʱ����������FileObject->FsContext��
typedef struct _HandleContext
{
	std::map<int, FD> vecFD;
}HandleContext, * PHandleContext;

namespace FSTools
{
	NTSTATUS dispatcher(MinorOrder subOrder, PCMD pCmd, PVOID outBuffer, ULONG outBufferSize, PHandleContext context, PULONG retLength);
	
	// r0�ṹ��r3�ṹת��
	void structTransfer(PFIND_FILE_OUTPUT in, PFileInfo out);

	// ����һ���ļ�������
	int32_t genFd();
	
	// �ҵ��ĵ�һ���ļ���Ŀ¼����Ϣ
	NTSTATUS FindFirstFile(PHandleContext context, LPCWSTR dirPath, PVOID outBuffer, PULONG retLength);

	// ������һ���ļ���Ŀ¼����Ϣ
	NTSTATUS FindNextFile(PHandleContext context, int fd, PVOID outBuffer, PULONG retLength);

	// ֹͣĿ¼��Ĳ�ѯ
	NTSTATUS FindClose(PHandleContext context, int fd);

	// ɾ���ļ�
	NTSTATUS DeleteFile(LPCWSTR dirPath);

	// �������ļ�
	NTSTATUS RenameFile(LPCWSTR srcPath, LPCWSTR destPath, bool bReplaceIfExists);

	// �����ļ�
	NTSTATUS CopyFile(LPCWSTR srcPath, LPCWSTR destPath, bool bDeleteSrcFile);

	// �򿪻��½��ļ�
	NTSTATUS CreateFile(PHandleContext context, LPCWSTR filename, PVOID outBuffer, PULONG retLength);

	// д���ļ�
	NTSTATUS WriteFile(PHandleContext context, int fd, PVOID data, PVOID outBuffer, PULONG retLength);

	// ��ȡ�ļ�
	NTSTATUS ReadFile(PHandleContext context, int fd, PVOID data, PVOID outBuffer, PULONG retLength);

	// �ر��ļ�
	NTSTATUS CloseFile(PHandleContext context, int fd, PVOID data, PVOID outBuffer, PULONG retLength);
};

