#pragma once
#include "Entry.h"
#include "typedef.h"

//��������,δ�ĵ�������
extern "C"
NTSTATUS NTAPI ObCreateObject(
	IN KPROCESSOR_MODE ObjectAttributesAccessMode  OPTIONAL,
	IN POBJECT_TYPE  Type,
	IN POBJECT_ATTRIBUTES ObjectAttributes  OPTIONAL,
	IN KPROCESSOR_MODE  AccessMode,
	IN OUT PVOID ParseContext  OPTIONAL,
	IN ULONG  ObjectSize,
	IN ULONG PagedPoolCharge  OPTIONAL,
	IN ULONG NonPagedPoolCharge  OPTIONAL,
	OUT PVOID* Object);

// δ���������ݽṹ
//SeCreateAccessState��2������
typedef struct _AUX_ACCESS_DATA {
	PPRIVILEGE_SET PrivilegesUsed;
	GENERIC_MAPPING GenericMapping;
	ACCESS_MASK AccessesToAudit;
	ACCESS_MASK MaximumAuditMask;
	ULONG Unknown[256];
} AUX_ACCESS_DATA, * PAUX_ACCESS_DATA;

//���ð�ȫ״̬
extern "C"
NTSTATUS NTAPI SeCreateAccessState(
	__out PACCESS_STATE AccessState,
	__out PAUX_ACCESS_DATA AuxData,
	__in ACCESS_MASK DesiredAccess,
	__in_opt PGENERIC_MAPPING GenericMapping
);

//
//��ѯĿ¼�µ��ļ�������ݽṹ(��Ӧ�ò㽻��)
//
//����ѯ��Ŀ¼����Ϣ
typedef struct _FIND_FILE_HANDLE_INFO {
	PVOID pDeviceObject; //���豸(�ڲ���)
	PVOID pFileObject;   //Ŀ¼�ļ�����(�ڲ���)
}FIND_FILE_HANDLE_INFO, * PFIND_FILE_HANDLE_INFO, * LPFIND_FILE_HANDLE_INFO;

//��������Ϣ
typedef struct _FIND_FILE_OUTPUT {
	LARGE_INTEGER CreationTime;  //����ʱ��
	LARGE_INTEGER LastAccessTime; //�������ʱ��
	LARGE_INTEGER LastWriteTime;  //���д��ʱ��
	LARGE_INTEGER ChangeTime;     //���ʱ��
	LARGE_INTEGER EndOfFile;      //�ļ���С
	LARGE_INTEGER AllocationSize; //ռ�ÿռ��С
	ULONG    ulFileAttributes;  //����
	WCHAR    wShortFileName[14]; //8.3 ��ʽ��
	WCHAR    wFileName[MAX_PATH_]; //����
} FIND_FILE_OUTPUT, * PFIND_FILE_OUTPUT, * LPFIND_FILE_OUTPUT;

//�״β�ѯ������������ṹ
typedef struct _FIND_FIRST_FILE_OUTPUT {
	FIND_FILE_HANDLE_INFO stFileFileHandleInfo; //����ѯ��Ŀ¼����Ϣ
	FIND_FILE_OUTPUT stFindFileItem; //��������Ϣ
}FIND_FIRST_FILE_OUTPUT, * PFIND_FIRST_FILE_OUTPUT, * LPFIND_FIRST_FILE_OUTPUT;


namespace FSCtrlIrp
{
	VOID FreeMdl(IN PMDL pMdl);

	//�ļ������������
	NTSTATUS FileOperationCompletion(
		IN PDEVICE_OBJECT pDeviceObject,
		IN PIRP pIrp,
		IN PVOID pContext OPTIONAL
	);

	// ���ļ�
	NTSTATUS IrpCreateFile(
		OUT PFILE_OBJECT* ppFileObject,
		OUT PDEVICE_OBJECT* ppDeviceObject, //����ɹ������ﱣ��:(*ppFileObject)->Vpb->DeviceObject
		IN ACCESS_MASK DesiredAccess,
		IN PUNICODE_STRING punsFilePath, //������"�̷�:\"��ͷ, ����: "C:\..."
		OUT PIO_STATUS_BLOCK pIoStatusBlock,
		IN PLARGE_INTEGER AllocationSize OPTIONAL,
		IN ULONG FileAttributes,
		IN ULONG ShareAccess,
		IN ULONG Disposition,
		IN ULONG CreateOptions,
		IN PVOID EaBuffer OPTIONAL,
		IN ULONG EaLength
	);

	NTSTATUS IrpCleanupFile(
		IN PDEVICE_OBJECT pDeviceObject,
		IN PFILE_OBJECT pFileObject
	);

	NTSTATUS IrpCloseFile(
		IN PDEVICE_OBJECT pDeviceObject,
		IN PFILE_OBJECT pFileObject
	);

	//�ļ���/д(IRP_MJ_READ/IRP_MJ_WRITE)
	NTSTATUS IrpReadOrWriteFile(
		IN PDEVICE_OBJECT pDeviceObject,
		IN PFILE_OBJECT pFileObject,
		OUT PIO_STATUS_BLOCK pIoStatusBlock,
		IN OUT PVOID pBuffer,
		IN ULONG ulLength,
		IN PLARGE_INTEGER pliByteOffset OPTIONAL,
		IN BOOLEAN bRead
	);

	//�ļ�����(IRP_MJ_SET_INFORMATION��
	NTSTATUS IrpSetInformationFile(
		IN PDEVICE_OBJECT pDeviceObject,
		IN PFILE_OBJECT pFileObject,
		IN PFILE_OBJECT pTargetFileObject OPTIONAL,
		OUT PIO_STATUS_BLOCK pIoStatusBlock,
		IN PVOID pFileInformation,
		IN ULONG ulLength,
		IN FILE_INFORMATION_CLASS FileInformationClass
	);

	//�ļ���ѯ(IRP_MJ_QUERY_INFORMATION)
	NTSTATUS IrpQueryInformationFile(
		IN PDEVICE_OBJECT pDeviceObject,
		IN PFILE_OBJECT pFileObject,
		OUT PIO_STATUS_BLOCK pIoStatusBlock,
		OUT PVOID pFileInformation,
		IN ULONG ulLength,
		IN FILE_INFORMATION_CLASS FileInformationClass
	);

	//��ȡĿ¼�µ��ļ�����Ŀ¼(MajorFunction: IRP_MJ_DIRECTORY_CONTROL, MinorFunction: IRP_MN_QUERY_DIRECTORY)
	NTSTATUS IrpQueryDirectoryFile(
		IN PDEVICE_OBJECT pDeviceObject,
		IN PFILE_OBJECT pFileObject,
		OUT PIO_STATUS_BLOCK pIoStatusBlock,
		OUT PVOID pFileInformation,
		IN ULONG ulLength,
		IN FILE_INFORMATION_CLASS FileInformationClass,
		IN BOOLEAN bReturnSingleEntry,
		IN PUNICODE_STRING punsFileName OPTIONAL,
		IN BOOLEAN bRestartScan
	);

	//�Լ�����IRP�����ѯĿ¼�µ�1���װ
	NTSTATUS RootkitFindFileItem(
		IN PDEVICE_OBJECT pDeviceObject,
		IN PFILE_OBJECT pFileObject,
		IN BOOLEAN bRestartScan,
		OUT PFIND_FILE_OUTPUT pFindFileOut
	);

	//��1�β�ѯĿ¼�µ���(�豸���������е���)
	NTSTATUS RootkitFindFirstFileForIoctl(
		IN LPCWSTR wDirPath,
		OUT PFIND_FIRST_FILE_OUTPUT pFindFirstFileOutput
	);

	//�ٴβ�ѯĿ¼�µ���(�豸���������е���)
	NTSTATUS RootkitFindNextFileForIoctl(
		IN PFIND_FILE_HANDLE_INFO pFileFirstFileHandleInfo,
		OUT PFIND_FILE_OUTPUT pFindFileOutput
	);

	//����Ŀ¼����Ĳ�ѯ(�豸���������е���)
	NTSTATUS RootkitFindCloseForIoctl(
		IN PFIND_FILE_HANDLE_INFO pFileFirstFileHandleInfo
	);

	// �ļ�ɾ��
	//�Լ�����IRP����ɾ���ļ������ָ��ΪpFileObject->Vpb->DeviceObject,���ƹ��ļ�ϵͳ��������
	NTSTATUS RootkitDeleteFile(
		IN PDEVICE_OBJECT pDeviceObject,
		IN PFILE_OBJECT pFileObject
	);

	//ǿɾ�ļ�(�豸���������е���)
	NTSTATUS RootkitDeleteFileForIoctl(
		IN LPCWSTR wFilePath
	);

	//�Լ�����IRP��������ļ�
//wNewFileNameOrPath�� ���ֻ���ļ���,��򵥽��и�����������ļ�·�������ʾ�ļ��ƶ�����ͬ��Ĳ�ͬĿ¼��,���磺�Ƶ�����վ������Ĳ���
	NTSTATUS RootkitRenameFile(
		IN PDEVICE_OBJECT pDeviceObject, //���ָ��ΪpFileObject->Vpb->DeviceObject,���ƹ��ļ�ϵͳ��������
		IN PFILE_OBJECT pFileObject,
		IN LPCWSTR wNewFileNameOrPath,
		IN BOOLEAN bReplaceIfExists
	);

	// ������
	NTSTATUS RootkitRenameFileForIoctl(
		LPCWSTR wSrcPath,
		LPCWSTR wDestPath,
		BOOLEAN bReplaceIfExists
	);

	//�����ļ�
	NTSTATUS RootkitCopyFile(
		IN PDEVICE_OBJECT pSrcDeviceObject,
		IN PFILE_OBJECT pSrcFileObject,
		IN PDEVICE_OBJECT pDestDeviceObject,
		IN PFILE_OBJECT pDestFileObject,
		IN BOOLEAN bDeleteSrcFile
	);

	NTSTATUS RootkitCopyFileForIoctl(
		LPCWSTR wSrcPath,
		LPCWSTR wDestPath,
		BOOLEAN bDeleteSrcFile
	);

};

