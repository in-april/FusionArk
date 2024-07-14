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

// 各结构体成员偏移
typedef struct _KProcessOffset
{
	uint32_t tmp;
}KProcessOffset;

typedef struct _EProcessOffset
{
	uint32_t CreateTime; // 进程创建时间
	uint32_t InheritedFromUniqueProcessId; // 父进程pid
	uint32_t Flags2; // 从0开始的第11位，为ProtectedProcess，该位为1时，r3不能打开
}EProcessOffset;

typedef struct _DriverObjOffset
{
	uint32_t DeviceObject; 
	uint32_t DriverStart; 
	uint32_t Flags;
}DriverObjOffset;


// 总的偏移结构体，用于传输给驱动
typedef struct _Offset
{
	KProcessOffset kprocess;
	EProcessOffset eprocess;
	DriverObjOffset drvobj;
}Offset;

// 进程结构
typedef struct _ProcessItem
{
	uint64_t pid;
	uint64_t parent_pid;
	ptr64_t eprocess;
	char path[MAX_PATH_];
	char cmdline[MAX_CMDLINE];
	uint64_t start_time;  // 启动时间，1601年1月1日起的100纳秒数
	uint8_t r3_open; // r3的访问权限
} ProcessItem, *PProcessItem;

typedef struct _ProcessInfo
{
	uint32_t count; // 进程的数量
	ProcessItem items[1];
}ProcessInfo, * PProcessInfo;

// 句柄结构
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
	uint32_t count; // 句柄的数量
	HandleItem items[1];
}HandleInfo, * PHandleInfo;

// 模块
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
	uint16_t load_seq; // 加载顺序
	uint16_t init_seq; // 初始化顺序
	char  path[MAX_PATH_];

	// 有驱动对象的模块
	ptr64_t driver_object;
	char  driver_name[MAX_PATH_];
} SysModItem, * PSysModItem;

typedef struct _SysModInfo
{
	uint32_t count; // 模块的数量
	SysModItem items[1];
}SysModInfo, * PSysModInfo;

// 内存
typedef struct _MemItem {
	ptr64_t base;
	ptr64_t alloc_base;
	uint32_t attr; // 当前属性
	uint32_t alloc_attr; // 初始属性
	uint32_t size;
	uint32_t status;
	uint32_t type;
	char path[MAX_PATH_];
}MemItem, *PMemItem;


// 文件信息
typedef struct _FileInfo {
	int64_t   create_time;  //创建时间
	int64_t   last_access_time; //最近访问时间
	int64_t   last_write_time;  //最近写入时间
	int64_t	  change_time;     //变更时间
	int64_t   size;      //文件大小
	int64_t   allocation_size; //占用空间大小
	uint32_t  attr;  //属性
	// wchar_t   short_filename[14]; //8.3 格式名
	wchar_t   filename[MAX_PATH_]; //名称
} FileInfo, *PFileInfo;

typedef struct _FirstFileInfo
{
	int32_t fd;
	FileInfo file_info;
}FirstFileInfo, *PFirstFileInfo;


// 定义与驱动通信的结构体
enum class MajorOrder {
	Init,
	Process,
	Driver,
	FsCtrl,
};

enum class MinorOrder {
	InitOffset, // 初始化结构体偏移

	ProcessList, // 获取进程列表
	HandleList, // 获取指定进程的所有私有句柄
	KillProcess, // 关闭进程
	OpenProcess, // 打开进程
	ReadVm, // 读取指定位置的内存
	CloseHandle, // 关闭指定句柄

	SysModList, // 获取内核模块列表
	DriverList, // 获取驱动列表（补充内核列表的信息）

	CreateFile,
	FindFirstFile,
	FindNextFile,
	FindClose,
	CopyFile,
	DeleteFile,
	RenameFile,

};


// 传入给驱动的结构体，按需填充
typedef struct _CMD
{
	MajorOrder major;
	MinorOrder minor;
	Offset offset;

	uint64_t pid;

	// 打开进程相关
	uint32_t access; // MASK_ACCESS
	uint64_t handle; // 句柄

	// 内存读写相关
	ptr64_t address;   // 读写内存的地址
	uint32_t data_size; // 读写内存的大小

	// 文件相关
	int32_t fd;
	wchar_t srcPath[MAX_PATH_];
	wchar_t destPath[MAX_PATH_];
	bool bReplaceIfExists; // 重命名文件时，若目标文件已存在，是否替换
	bool bDeleteSrcFile; // 拷贝文件时，是否删除原文件

	// 其他数据
	uint8_t data[1024];

	// 构造函数
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

	uint8_t extend[1]; // 扩展

}CMD, *PCMD;