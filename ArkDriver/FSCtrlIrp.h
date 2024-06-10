#pragma once
#include "Entry.h"
#include "typedef.h"

//创建对象,未文档化函数
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

// 未公开的数据结构
//SeCreateAccessState第2个参数
typedef struct _AUX_ACCESS_DATA {
	PPRIVILEGE_SET PrivilegesUsed;
	GENERIC_MAPPING GenericMapping;
	ACCESS_MASK AccessesToAudit;
	ACCESS_MASK MaximumAuditMask;
	ULONG Unknown[256];
} AUX_ACCESS_DATA, * PAUX_ACCESS_DATA;

//设置安全状态
extern "C"
NTSTATUS NTAPI SeCreateAccessState(
	__out PACCESS_STATE AccessState,
	__out PAUX_ACCESS_DATA AuxData,
	__in ACCESS_MASK DesiredAccess,
	__in_opt PGENERIC_MAPPING GenericMapping
);

//
//查询目录下的文件相关数据结构(与应用层交互)
//
//被查询的目录打开信息
typedef struct _FIND_FILE_HANDLE_INFO {
	PVOID pDeviceObject; //卷设备(内部用)
	PVOID pFileObject;   //目录文件对象(内部用)
}FIND_FILE_HANDLE_INFO, * PFIND_FILE_HANDLE_INFO, * LPFIND_FILE_HANDLE_INFO;

//发现项信息
typedef struct _FIND_FILE_OUTPUT {
	LARGE_INTEGER CreationTime;  //创建时间
	LARGE_INTEGER LastAccessTime; //最近访问时间
	LARGE_INTEGER LastWriteTime;  //最近写入时间
	LARGE_INTEGER ChangeTime;     //变更时间
	LARGE_INTEGER EndOfFile;      //文件大小
	LARGE_INTEGER AllocationSize; //占用空间大小
	ULONG    ulFileAttributes;  //属性
	WCHAR    wShortFileName[14]; //8.3 格式名
	WCHAR    wFileName[MAX_PATH_]; //名称
} FIND_FILE_OUTPUT, * PFIND_FILE_OUTPUT, * LPFIND_FILE_OUTPUT;

//首次查询的输出缓冲区结构
typedef struct _FIND_FIRST_FILE_OUTPUT {
	FIND_FILE_HANDLE_INFO stFileFileHandleInfo; //被查询的目录打开信息
	FIND_FILE_OUTPUT stFindFileItem; //发现项信息
}FIND_FIRST_FILE_OUTPUT, * PFIND_FIRST_FILE_OUTPUT, * LPFIND_FIRST_FILE_OUTPUT;


namespace FSCtrlIrp
{
	VOID FreeMdl(IN PMDL pMdl);

	//文件操作完成例程
	NTSTATUS FileOperationCompletion(
		IN PDEVICE_OBJECT pDeviceObject,
		IN PIRP pIrp,
		IN PVOID pContext OPTIONAL
	);

	// 打开文件
	NTSTATUS IrpCreateFile(
		OUT PFILE_OBJECT* ppFileObject,
		OUT PDEVICE_OBJECT* ppDeviceObject, //如果成功，这里保存:(*ppFileObject)->Vpb->DeviceObject
		IN ACCESS_MASK DesiredAccess,
		IN PUNICODE_STRING punsFilePath, //必须以"盘符:\"开头, 比如: "C:\..."
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

	//文件读/写(IRP_MJ_READ/IRP_MJ_WRITE)
	NTSTATUS IrpReadOrWriteFile(
		IN PDEVICE_OBJECT pDeviceObject,
		IN PFILE_OBJECT pFileObject,
		OUT PIO_STATUS_BLOCK pIoStatusBlock,
		IN OUT PVOID pBuffer,
		IN ULONG ulLength,
		IN PLARGE_INTEGER pliByteOffset OPTIONAL,
		IN BOOLEAN bRead
	);

	//文件设置(IRP_MJ_SET_INFORMATION）
	NTSTATUS IrpSetInformationFile(
		IN PDEVICE_OBJECT pDeviceObject,
		IN PFILE_OBJECT pFileObject,
		IN PFILE_OBJECT pTargetFileObject OPTIONAL,
		OUT PIO_STATUS_BLOCK pIoStatusBlock,
		IN PVOID pFileInformation,
		IN ULONG ulLength,
		IN FILE_INFORMATION_CLASS FileInformationClass
	);

	//文件查询(IRP_MJ_QUERY_INFORMATION)
	NTSTATUS IrpQueryInformationFile(
		IN PDEVICE_OBJECT pDeviceObject,
		IN PFILE_OBJECT pFileObject,
		OUT PIO_STATUS_BLOCK pIoStatusBlock,
		OUT PVOID pFileInformation,
		IN ULONG ulLength,
		IN FILE_INFORMATION_CLASS FileInformationClass
	);

	//获取目录下的文件及子目录(MajorFunction: IRP_MJ_DIRECTORY_CONTROL, MinorFunction: IRP_MN_QUERY_DIRECTORY)
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

	//自己发送IRP请求查询目录下的1项封装
	NTSTATUS RootkitFindFileItem(
		IN PDEVICE_OBJECT pDeviceObject,
		IN PFILE_OBJECT pFileObject,
		IN BOOLEAN bRestartScan,
		OUT PFIND_FILE_OUTPUT pFindFileOut
	);

	//第1次查询目录下的项(设备控制请求中调用)
	NTSTATUS RootkitFindFirstFileForIoctl(
		IN LPCWSTR wDirPath,
		OUT PFIND_FIRST_FILE_OUTPUT pFindFirstFileOutput
	);

	//再次查询目录下的项(设备控制请求中调用)
	NTSTATUS RootkitFindNextFileForIoctl(
		IN PFIND_FILE_HANDLE_INFO pFileFirstFileHandleInfo,
		OUT PFIND_FILE_OUTPUT pFindFileOutput
	);

	//结束目录下项的查询(设备控制请求中调用)
	NTSTATUS RootkitFindCloseForIoctl(
		IN PFIND_FILE_HANDLE_INFO pFileFirstFileHandleInfo
	);

	// 文件删除
	//自己发送IRP请求删除文件，如果指定为pFileObject->Vpb->DeviceObject,可绕过文件系统过滤驱动
	NTSTATUS RootkitDeleteFile(
		IN PDEVICE_OBJECT pDeviceObject,
		IN PFILE_OBJECT pFileObject
	);

	//强删文件(设备控制请求中调用)
	NTSTATUS RootkitDeleteFileForIoctl(
		IN LPCWSTR wFilePath
	);

	//自己发送IRP请求更名文件
//wNewFileNameOrPath： 如果只是文件名,则简单进行更名。如果是文件路径，则表示文件移动到相同卷的不同目录下,比如：移到回收站是最常见的操作
	NTSTATUS RootkitRenameFile(
		IN PDEVICE_OBJECT pDeviceObject, //如果指定为pFileObject->Vpb->DeviceObject,可绕过文件系统过滤驱动
		IN PFILE_OBJECT pFileObject,
		IN LPCWSTR wNewFileNameOrPath,
		IN BOOLEAN bReplaceIfExists
	);

	// 重命名
	NTSTATUS RootkitRenameFileForIoctl(
		LPCWSTR wSrcPath,
		LPCWSTR wDestPath,
		BOOLEAN bReplaceIfExists
	);

	//拷贝文件
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

