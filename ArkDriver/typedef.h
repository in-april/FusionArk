#pragma once

#define MAX_NAME 260
#define MAX_PATH_ 512
#define MAX_CMDLINE 1024

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t; 
typedef int int32_t;
typedef unsigned int uint32_t;
typedef __int64 int64_t;
typedef unsigned long long uint64_t;
typedef unsigned long ptr32_t;
typedef unsigned long long ptr64_t;

// ���ṹ���Աƫ��
typedef struct _KProcessOffset
{
	uint32_t tmp;
}KProcessOffset;

typedef struct _EProcessOffset
{
	uint32_t CreateTime; // ���̴���ʱ��
	uint32_t InheritedFromUniqueProcessId; // ������pid
	uint32_t Flags2; // ��0��ʼ�ĵ�11λ��ΪProtectedProcess����λΪ1ʱ��r3���ܴ�
}EProcessOffset;

typedef struct _DriverObjOffset
{
	uint32_t DeviceObject; 
	uint32_t DriverStart; 
	uint32_t Flags;
}DriverObjOffset;


// �ܵ�ƫ�ƽṹ�壬���ڴ��������
typedef struct _Offset
{
	KProcessOffset kprocess;
	EProcessOffset eprocess;
	DriverObjOffset drvobj;
}Offset;

// ���̽ṹ
typedef struct _ProcessItem
{
	uint64_t pid;
	uint64_t parent_pid;
	ptr64_t eprocess;
	char path[MAX_PATH_];
	char cmdline[MAX_CMDLINE];
	uint64_t start_time;  // ����ʱ�䣬1601��1��1�����100������
	uint8_t r3_open; // r3�ķ���Ȩ��
} ProcessItem, *PProcessItem;

typedef struct _ProcessInfo
{
	uint32_t count; // ���̵�����
	ProcessItem items[1];
}ProcessInfo, * PProcessInfo;

// ����ṹ
typedef struct _HandleItem
{
	uint64_t pid;
	ptr64_t value;
	ptr64_t obj_address;
	uint32_t access;
	uint32_t obj_count;
	uint32_t handle_count;
	char type_name[64];
	char obj_name[MAX_NAME];
} HandleItem, * PHandleItem;

typedef struct _HandleInfo
{
	uint32_t count; // ���������
	HandleItem items[1];
}HandleInfo, * PHandleInfo;

// ģ��
typedef struct _ModuleItem {
	ptr64_t image_base;
	uint32_t image_size;
	char path[MAX_PATH_];
	char name[MAX_NAME];
}ModuleItem, * PModuleItem;

typedef struct _SysModItem
{
	ptr64_t base;
	uint32_t size;
	uint16_t load_seq; // ����˳��
	uint16_t init_seq; // ��ʼ��˳��
	char  path[MAX_PATH_];

	// �����������ģ��
	ptr64_t driver_object;
	char  driver_name[MAX_PATH_];
} SysModItem, * PSysModItem;

typedef struct _SysModInfo
{
	uint32_t count; // ģ�������
	SysModItem items[1];
}SysModInfo, * PSysModInfo;

// �ڴ�
typedef struct _MemItem {
	ptr64_t base;
	ptr64_t alloc_base;
	uint32_t attr; // ��ǰ����
	uint32_t alloc_attr; // ��ʼ����
	uint32_t size;
	uint32_t status;
	uint32_t type;
	char path[MAX_PATH_];
}MemItem, *PMemItem;


// �ļ���Ϣ
typedef struct _FileInfo {
	int64_t   create_time;  //����ʱ��
	int64_t   last_access_time; //�������ʱ��
	int64_t   last_write_time;  //���д��ʱ��
	int64_t	  change_time;     //���ʱ��
	int64_t   size;      //�ļ���С
	int64_t   allocation_size; //ռ�ÿռ��С
	uint32_t  attr;  //����
	// wchar_t   short_filename[14]; //8.3 ��ʽ��
	wchar_t   filename[MAX_PATH_]; //����
} FileInfo, *PFileInfo;

typedef struct _FirstFileInfo
{
	int32_t fd;
	FileInfo file_info;
}FirstFileInfo, *PFirstFileInfo;


// ����������ͨ�ŵĽṹ��
enum class MajorOrder {
	Init,
	Process,
	Driver,
	FsCtrl,
};

enum class MinorOrder {
	InitOffset, // ��ʼ���ṹ��ƫ��

	ProcessList, // ��ȡ�����б�
	HandleList, // ��ȡָ�����̵�����˽�о��
	KillProcess, // �رս���
	OpenProcess, // �򿪽���
	ReadVm, // ��ȡָ��λ�õ��ڴ�
	CloseHandle, // �ر�ָ�����

	SysModList, // ��ȡ�ں�ģ���б�
	DriverList, // ��ȡ�����б������ں��б����Ϣ��

	CreateFile,
	FindFirstFile,
	FindNextFile,
	FindClose,
	CopyFile,
	DeleteFile,
	RenameFile,

};


// ����������Ľṹ�壬�������
typedef struct _CMD
{
	MajorOrder major;
	MinorOrder minor;
	Offset offset;

	uint64_t pid;

	// �򿪽������
	uint32_t access; // MASK_ACCESS
	uint64_t handle; // ���

	// �ڴ��д���
	ptr64_t address;   // ��д�ڴ�ĵ�ַ
	uint32_t data_size; // ��д�ڴ�Ĵ�С

	// �ļ����
	int32_t fd;
	wchar_t srcPath[MAX_PATH_];
	wchar_t destPath[MAX_PATH_];
	bool bReplaceIfExists; // �������ļ�ʱ����Ŀ���ļ��Ѵ��ڣ��Ƿ��滻
	bool bDeleteSrcFile; // �����ļ�ʱ���Ƿ�ɾ��ԭ�ļ�

	// ��������
	uint8_t data[1024];

	// ���캯��
	_CMD(MajorOrder _major, MinorOrder _minor)
		: major(_major), minor(_minor) 
	{
		memset(&offset, 0, sizeof(Offset));
		memset(&data, 0, sizeof(data));

		memset(&srcPath, 0, sizeof(srcPath));
		memset(&destPath, 0, sizeof(destPath));
		pid = 0;
		access = 0;
		handle = 0;

		address = 0;
		data_size = 0;

		fd = 0;
	}

	uint8_t extend[1]; // ��չ

}CMD, *PCMD;