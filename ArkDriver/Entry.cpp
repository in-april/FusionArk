#include "Entry.h"
#include "typedef.h"
#include "ProcessTools.h"
#include "DriverTools.h"
#include "FSTools.h"

Offset gOffset;

#define DEVICE_NAME L"\\Device\\MyArkTools202403"
//r3 用 "\\\\.\\MyArkTools202403"
#define SYMBOLICLINK_NAME L"\\??\\MyArkTools202403"

//根据句柄返回句柄信息
#define OPER_CMD CTL_CODE(FILE_DEVICE_UNKNOWN, 0x900, METHOD_IN_DIRECT, FILE_ANY_ACCESS)


UNICODE_STRING deviceName = { 0 };
UNICODE_STRING symbolicLinkName = { 0 };
PDEVICE_OBJECT pDeviceObj = NULL;

// 驱动程序卸载函数
extern "C" VOID UnloadDriver(
	_In_ PDRIVER_OBJECT DriverObject
)
{
	UNREFERENCED_PARAMETER(DriverObject);
	// 删除符号链接 删除设备
	IoDeleteSymbolicLink(&symbolicLinkName);
	if (pDeviceObj)
	{
		IoDeleteDevice(pDeviceObj);
	}

	KdPrint(("UnloadDriver completed\n"));
}

NTSTATUS IrpCreateProc(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
	PIO_STACK_LOCATION stackLocation;
	PHandleContext fileContext = NULL;
	NTSTATUS status = STATUS_SUCCESS;

	// 获取当前的I/O堆栈位置
	stackLocation = IoGetCurrentIrpStackLocation(pIrp);

	// 为文件上下文分配内存
	fileContext = (PHandleContext)ExAllocatePool(NonPagedPool, sizeof(HandleContext));
	if (fileContext == NULL) {
		status = STATUS_INSUFFICIENT_RESOURCES;
	}
	else {
		// 初始化文件上下文
		RtlZeroMemory(fileContext, sizeof(HandleContext));
		fileContext = new (fileContext) HandleContext(); // 在分配的内存上调用构造函数
		// 设置FileObject的FsContext
		stackLocation->FileObject->FsContext = fileContext;
	}

	// 完成IRP
	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = 0;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	return status;
}

NTSTATUS IrpCloseProc(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
	//处理
	KdPrint(("close device success!\n"));

	PIO_STACK_LOCATION stackLocation;
	
	// 获取当前的I/O堆栈位置
	stackLocation = IoGetCurrentIrpStackLocation(pIrp);

	void* fileContext = stackLocation->FileObject->FsContext;
	if (fileContext != NULL)
	{
		ExFreePool(fileContext);
		stackLocation->FileObject->FsContext = NULL;
	}

	//设置返回状态
	pIrp->IoStatus.Status = STATUS_SUCCESS;	//getlasterror()得到的就是这个值
	pIrp->IoStatus.Information = 0;		//返回给3环多少数据 没有填0
	IoCompleteRequest(pIrp, IO_NO_INCREMENT); //向下传递
	return STATUS_SUCCESS;
}

// IRP_MJ_DEVICE_CONTROL处理函数 用来处理与Ring3交互
NTSTATUS IrpDeviceControlProc(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
	NTSTATUS status = STATUS_UNSUCCESSFUL;
	
	ULONG handle;

	// 设置临时变量的值
	PIO_STACK_LOCATION pIrpStack = IoGetCurrentIrpStackLocation(pIrp);
	// 获取控制码
	ULONG uIoControlCode = pIrpStack->Parameters.DeviceIoControl.IoControlCode;
	// 获取输入缓冲区
	PVOID pInBuffer = pIrp->AssociatedIrp.SystemBuffer;
	// 输入缓冲区长度
	ULONG InLength = pIrpStack->Parameters.DeviceIoControl.InputBufferLength;

	PHandleContext handleContext = (PHandleContext)pIrpStack->FileObject->FsContext;

	// 输出地址
	PVOID pOutBuffer = NULL;
	//KdBreakPoint();
	if (MmIsAddressValid(pIrp->MdlAddress))
	{
		pOutBuffer = MmGetSystemAddressForMdlSafe(pIrp->MdlAddress, NormalPagePriority);;
	}
	// 输出缓冲区长度
	ULONG OutLength = pIrpStack->Parameters.DeviceIoControl.OutputBufferLength;

	ULONG retLength = 0;
	switch (uIoControlCode)
	{
	case OPER_CMD:
	{
		PCMD commond = (PCMD)pInBuffer;
		
		switch (commond->major)
		{

		case MajorOrder::Init:
		{
			memcpy(&gOffset, &commond->offset, sizeof(gOffset));
			status = STATUS_SUCCESS;
			break;
		}
		case MajorOrder::Process:
		{
			status = ProcessFunc::dispatcher(commond->minor, commond, pOutBuffer, OutLength, &retLength);
			break;
		}
		case MajorOrder::Driver:
		{
			status = DriverFunc::dispatcher(commond->minor, commond, pOutBuffer, OutLength, &retLength);
			break;
		}
		case MajorOrder::FsCtrl:
		{
			status = FSTools::dispatcher(commond->minor, commond, pOutBuffer, OutLength, handleContext, &retLength);
			break;
		}
		default:
			status = STATUS_UNSUCCESSFUL;
			break;
		}
		
	}
	}
	pIrp->IoStatus.Information = retLength;
	
	// 设置返回状态
	pIrp->IoStatus.Status = status;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	return status;
	
}

// 驱动程序入口点
extern "C" NTSTATUS DriverEntry(
	_In_ PDRIVER_OBJECT   pDriver,
	_In_ PUNICODE_STRING  RegistryPath
)
{
	UNREFERENCED_PARAMETER(RegistryPath);
	KdPrint(("DriverEntry called\n"));

	// 设置驱动程序的卸载函数
	pDriver->DriverUnload = UnloadDriver;

	//初始化设备名称
	RtlInitUnicodeString(&deviceName, DEVICE_NAME);

	//创建设备
	NTSTATUS ntStatus = IoCreateDevice(pDriver, 0, &deviceName, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &pDeviceObj);
	if (ntStatus != STATUS_SUCCESS)
	{
		KdPrint(("设备创建失败:%x\n", ntStatus));
		return ntStatus;
	}

	//设置与r3通信方式(缓冲区方式读写),DO_DIRECT_IO为直接读写(映射r3地址的物理页，然后锁住)
	pDriver->Flags |= DO_DIRECT_IO;

	//初始化符号链接名称
	RtlInitUnicodeString(&symbolicLinkName, SYMBOLICLINK_NAME);

	//创建符号链接
	ntStatus = IoCreateSymbolicLink(&symbolicLinkName, &deviceName);
	if (ntStatus != STATUS_SUCCESS)
	{
		KdPrint(("设备符号链接失败:%x\n", ntStatus));
		IoDeleteDevice(pDeviceObj);
		return ntStatus;
	}

	//设置派遣函数
	pDriver->MajorFunction[IRP_MJ_CREATE] = IrpCreateProc;
	pDriver->MajorFunction[IRP_MJ_CLOSE] = IrpCloseProc;
	pDriver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = IrpDeviceControlProc;

	KdPrint(("DriverEntry completed\n"));
	return STATUS_SUCCESS;
}