#include "Main.h"
#include "Search.h"


// 键盘设备对象
PDEVICE_OBJECT g_keyboardDevice = NULL;
// 创建设备对象
PDEVICE_OBJECT g_deviceObject = NULL;
// 引用计数
LONG g_refCount = 0;

PVOID g_ProtectProcessRegistration = NULL;

// 进程通知回调函数
VOID ProcessNotifyRoutine(
	_In_ HANDLE ParentId,
	_In_ HANDLE ProcessId,
	_In_ BOOLEAN Create
);

VOID ProcessNotifyRoutineEx(
	_Inout_ PEPROCESS Process,
	_In_ HANDLE ProcessId,
	_Inout_opt_ PPS_CREATE_NOTIFY_INFO CreateInfo
);

// 驱动程序卸载函数
VOID UnloadDriver2(
	_In_ PDRIVER_OBJECT DriverObject
)
{
	UNREFERENCED_PARAMETER(DriverObject);

	KdPrint(("UnloadDriver called\n"));
	
	// 分离虚拟设备对象
	if (g_deviceObject != NULL && g_keyboardDevice != NULL) {
		IoDetachDevice(g_keyboardDevice);
	}

	// 删除虚拟设备对象
	if (g_deviceObject != NULL) {
		IoDeleteDevice(g_deviceObject);
	}

	UnregisterCallback();
	// KdPrint(("Process notify routine unregistered\n"));
	// 卸载代码可以在这里添加
	// PsSetCreateProcessNotifyRoutine(ProcessNotifyRoutine, TRUE);
	// PsSetCreateProcessNotifyRoutineEx(ProcessNotifyRoutineEx, TRUE);

	// 等待1秒钟
	//LARGE_INTEGER delay;
	//delay.QuadPart = -10000000;  // 3秒钟，以100纳秒为单位

	//KeDelayExecutionThread(KernelMode, FALSE, &delay);

	while (g_refCount != 0);

	KdPrint(("UnloadDriver completed\n"));
}

// IRP 处理函数
NTSTATUS DeviceIrpHandlerNormal(
	_In_ PDEVICE_OBJECT DeviceObject,
	_In_ PIRP Irp
)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	KdPrint(("DeviceIrpHandlerNormal enter\n"));
	IoSkipCurrentIrpStackLocation(Irp);

	return IoCallDriver(g_keyboardDevice, Irp);
}

// 键盘完成例程
NTSTATUS CompletionRoutine(
	_In_ PDEVICE_OBJECT DeviceObject,
	_In_ PIRP Irp,
	_In_ PVOID Context
)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	UNREFERENCED_PARAMETER(Context);

	// KdPrint(("YourCompletionRoutine called\n"));
	if (NT_SUCCESS(Irp->IoStatus.Status))
	{
		PKEYBOARD_INPUT_DATA pBuffer = Irp->AssociatedIrp.SystemBuffer;
		ULONG_PTR len = Irp->IoStatus.Information;
		for (int i = 0; i < len / sizeof(KEYBOARD_INPUT_DATA); i++)
		{
			KdPrint(("UnitId:%d MakeCode:%d Flags:%d \n", pBuffer->UnitId, pBuffer->MakeCode, pBuffer->Flags));
		}
	}

	if (Irp->PendingReturned)
	{
		IoMarkIrpPending(Irp);
	}

	InterlockedDecrement(&g_refCount);
	return Irp->IoStatus.Status;
}


// 键盘IRP 处理函数
NTSTATUS DeviceIrpHandlerRead(
	_In_ PDEVICE_OBJECT DeviceObject,
	_In_ PIRP Irp
)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	IoCopyCurrentIrpStackLocationToNext(Irp);


	// 设置完成例程
	IoSetCompletionRoutine(Irp, CompletionRoutine, NULL, TRUE, TRUE, TRUE);

	InterlockedIncrement(&g_refCount);


	return IoCallDriver(g_keyboardDevice, Irp);
}

// 驱动程序入口点
DriverEntry2(
	_In_ PDRIVER_OBJECT   DriverObject,
	_In_ PUNICODE_STRING  RegistryPath
)
{
	UNREFERENCED_PARAMETER(RegistryPath);

	KdPrint(("DriverEntry called\n"));

	// 设置驱动程序的卸载函数
	DriverObject->DriverUnload = UnloadDriver2;

	// 注册进程通知回调
	// NTSTATUS status = PsSetCreateProcessNotifyRoutine(ProcessNotifyRoutine, FALSE);
	//NTSTATUS status = PsSetCreateProcessNotifyRoutineEx(ProcessNotifyRoutineEx, FALSE);
	//if (NT_SUCCESS(status)) {
	//	KdPrint(("Process notify routine registered successfully\n"));
	//}
	//else {
	//	KdPrint(("Failed to register process notify routine: 0x%X\n", status));
	//	return status;
	//}
	// 
	
	// 过滤驱动
	//NTSTATUS status = IoCreateDevice(
	//	DriverObject,               // 驱动程序对象
	//	0, // 设备扩展结构大小
	//	NULL,                       // 设备名
	//	FILE_DEVICE_UNKNOWN,        // 设备类型
	//	0,                          // 设备特征
	//	FALSE,                      // 互斥锁
	//	&g_deviceObject      // 输出的设备对象指针
	//);

	//if (!NT_SUCCESS(status)) {
	//	KdPrint(("Failed to create virtual device: 0x%X\n", status));
	//	return status;
	//}
	//
	//for (int i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
	//{
	//	DriverObject->MajorFunction[i] = DeviceIrpHandlerNormal;
	//}
	//DriverObject->MajorFunction[IRP_MJ_READ] = DeviceIrpHandlerRead;

	//// 附加虚拟设备到键盘设备
	//UNICODE_STRING keyboardDeviceName;
	//RtlInitUnicodeString(&keyboardDeviceName, L"\\Device\\KeyboardClass0"); // 键盘设备名

	//status = IoAttachDevice(
	//	g_deviceObject,  // 虚拟设备对象
	//	&keyboardDeviceName,    // 键盘设备名
	//	&g_keyboardDevice // 输出的附加键盘设备对象指针
	//);

	//if (!NT_SUCCESS(status)) {
	//	KdPrint(("Failed to attach to keyboard device: 0x%X\n", status));
	//	IoDeleteDevice(g_deviceObject);
	//	return status;
	//}

	//// 通讯方式保持一致
	//g_deviceObject->Flags = g_keyboardDevice->Flags;

	// 枚举进程
	//EnumProcess();

	// 枚举线程
	//TerminateProcessByPid((HANDLE)1684);

	// 进程保护
	//RegisterCallback();

	//APC
	//ApcTest();

	//DPC
	//DpcTest();

	// 工作队列
	TestWorkItem();

	KdPrint(("DriverEntry completed\n"));

	return STATUS_SUCCESS;
}

// 枚举所有进程
VOID EnumProcess()
{
	PEPROCESS pEProcess = PsGetCurrentProcess();
	PEPROCESS pCurrent = pEProcess;
	INT32 index = 0;
	do 
	{
		index++;
		// 打印当前进程的信息
		//INT32 exitCode = *(INT32*)((CHAR*)pCurrent + 0x444);
		PUNICODE_STRING path = NULL;
		NTSTATUS status = SeLocateProcessImageName(pCurrent, &path);
		NTSTATUS exitCode = PsGetProcessExitStatus(pCurrent);
		if (NT_SUCCESS(status))
		{
			KdPrint(("pid:%p, exitCode:%d, path:%wZ\n", PsGetProcessId(pCurrent), exitCode, path));
			ExFreePool(path);
		}
		else
		{
			KdPrint(("pid:%p, exitCode:%d, path:null\n", PsGetProcessId(pCurrent), exitCode));
		}
		
		// 下一个进程
		pCurrent = (PEPROCESS)((CHAR*)((*(INT64*)((CHAR*)pCurrent + 0x188))) - 0x188);
		// KdPrint(("next process eprocess is %p\n", pCurrent));
	} while (pEProcess != pCurrent);
}

VOID EnumProcess2()
{
	// TODO
}

VOID EnumThreads(HANDLE pid)
{
	PEPROCESS pEProcess = NULL;
	NTSTATUS status = PsLookupProcessByProcessId(pid, &pEProcess);
	if (NT_SUCCESS(status))
	{
		PKTHREAD pEThread = (PKTHREAD) * (INT64*)((CHAR*)pEProcess + 0x030);
		PKTHREAD pCurEThread = pEThread;
		
		do 
		{
			pCurEThread = (PKTHREAD)((CHAR*)pCurEThread - 0x2f8);

			HANDLE tid = PsGetThreadId(pCurEThread);
			KdPrint(("Tid:%p\n", tid));

			pCurEThread = (PKTHREAD)*(INT64*)((CHAR*)pCurEThread + 0x2f8);
		} while (pCurEThread != pEThread);
		ObDereferenceObject(pEProcess);
	}
}

VOID TerminateProcessByPid(HANDLE pid)
{
	//KdBreakPoint();
	UNICODE_STRING funcName = RTL_CONSTANT_STRING(L"PsTerminateSystemThread");
	ULONG_PTR code = (ULONG_PTR)MmGetSystemRoutineAddress(&funcName);
	if (code == 0) return;

	char* slave = "41B001E8";

	FindCode fs[1] = { 0 };
	initFindCodeStruct(&fs[0], slave, 0, 0x4);

	ULONG_PTR addr = findAddressByCode(code, (ULONG_PTR)((char*)code + 0x30), fs, 1);
	if (addr == 0) return;

	int offset = *(int*)addr;
	addr = (ULONG_PTR)((char*)addr + 4 + offset);

	typedef __int64(__fastcall* PspTerminateThreadByPointer)(IN PETHREAD Thread,
		IN NTSTATUS ExitStatus,
		IN BOOLEAN bSelf);


	PspTerminateThreadByPointer terminate = (PspTerminateThreadByPointer)addr;

	PEPROCESS pEProcess = NULL;
	NTSTATUS status = PsLookupProcessByProcessId(pid, &pEProcess);
	if (NT_SUCCESS(status))
	{
		PKTHREAD pEThread = (PKTHREAD) * (INT64*)((CHAR*)pEProcess + 0x030);
		PKTHREAD pCurEThread = pEThread;

		do
		{
			pCurEThread = (PKTHREAD)((CHAR*)pCurEThread - 0x2f8);

			HANDLE tid = PsGetThreadId(pCurEThread);
			KdPrint(("Tid:%p\n", tid));
			if (tid < 65535)
				terminate(pCurEThread, 0, 1);
			pCurEThread = (PKTHREAD) * (INT64*)((CHAR*)pCurEThread + 0x2f8);
		} while (pCurEThread != pEThread);
		ObDereferenceObject(pEProcess);
	}
}

OB_PREOP_CALLBACK_STATUS ProcessPreCallback(
	_In_ PVOID RegistrationContext,
	_Inout_ POB_PRE_OPERATION_INFORMATION OperationInformation
)
{
	UNREFERENCED_PARAMETER(RegistrationContext);

	// 在这里处理进程对象的预操作

	if (OperationInformation->ObjectType == *PsProcessType) {
		PEPROCESS targetProcess = (PEPROCESS)OperationInformation->Object;

		// 获取进程的映像文件名
		PUCHAR  processImageFileName  = PsGetProcessImageFileName(targetProcess);

		if (processImageFileName != NULL) {
			// 判断是否是要保护的进程
			if (strstr(processImageFileName, "test.exe") != 0) {
				KdPrint(("target process name:%s\n", processImageFileName));
				// 阻止关闭操作
				if (OperationInformation->Operation == OB_OPERATION_HANDLE_CREATE ||
					OperationInformation->Operation == OB_OPERATION_HANDLE_DUPLICATE) {
					OperationInformation->Parameters->CreateHandleInformation.DesiredAccess = 0;
				}
			}
			
		}
	}

	return OB_PREOP_SUCCESS;
}


VOID ProcessPostCallback(
	_In_ PVOID RegistrationContext,
	_In_ POB_POST_OPERATION_INFORMATION OperationInformation
)
{

}

// 注册回调
VOID RegisterCallback()
{
	OB_CALLBACK_REGISTRATION callbackRegistration = { 0 };

	UNICODE_STRING altitude;
	RtlInitUnicodeString(&altitude, L"321000");

	callbackRegistration.Version = ObGetFilterVersion();
	callbackRegistration.OperationRegistrationCount = 1;
	callbackRegistration.Altitude = altitude;
	callbackRegistration.RegistrationContext = NULL;

	OB_OPERATION_REGISTRATION objCallBackInfo = { 0 };
	objCallBackInfo.ObjectType = PsProcessType;
	objCallBackInfo.Operations = OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE;
	objCallBackInfo.PreOperation = ProcessPreCallback;
	objCallBackInfo.PostOperation = ProcessPostCallback;

	callbackRegistration.OperationRegistration = &objCallBackInfo;

	NTSTATUS status = ObRegisterCallbacks(&callbackRegistration, &g_ProtectProcessRegistration);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Failed to register callback: 0x%X\n", status));
	}
}

// 注销回调
VOID UnregisterCallback()
{
	if (g_ProtectProcessRegistration != NULL) {
		ObUnRegisterCallbacks(g_ProtectProcessRegistration);
		g_ProtectProcessRegistration = NULL;
	}
}


VOID KernelRoutineCallback(
	IN struct _KAPC* Apc,
	IN OUT PKNORMAL_ROUTINE* NormalRoutine,
	IN OUT PVOID* NormalContext,
	IN OUT PVOID* SystemArgument1,
	IN OUT PVOID* SystemArgument2
)
{
	PUNICODE_STRING unicode = NULL;
	NTSTATUS st = SeLocateProcessImageName(IoGetCurrentProcess(), &unicode);
	if (NT_SUCCESS(st))
	{
		KdPrint(("插入后 image name: %wZ\n", unicode));
		ExFreePool(unicode);
	}
	KdPrint(("func:%s\n",__FUNCTION__));
	ExFreePool(Apc);
}

VOID NormalApcCallback(
	IN PVOID NormalContext,
	IN PVOID SystemArgument1,
	IN PVOID SystemArgument2
)
{
	KdPrint(("func:%s\n", __FUNCTION__));
}

VOID ApcTest()
{
	PUNICODE_STRING unicode = NULL;
	NTSTATUS st = SeLocateProcessImageName(IoGetCurrentProcess(), &unicode);
	if (NT_SUCCESS(st))
	{
		KdPrint(("插入前 image name: %wZ\n", unicode));
		ExFreePool(unicode);
	}

	// 获取想要插入线程的ethread
	PETHREAD thread = GetThreadById(2716);


	PKAPC pKAPC = ExAllocatePool(NonPagedPool, sizeof(KAPC));
	if (pKAPC == NULL) return;
	memset(pKAPC, 0, sizeof(KAPC));
	KeInitializeApc(pKAPC,
		thread,
		CurrentApcEnvironment,
		KernelRoutineCallback,
		NULL,
		NormalApcCallback,
		KernelMode,
		NULL);


	KeInsertQueueApc(pKAPC, NULL, NULL, 0);
}

VOID DpcRoutineCallback(
	_In_ struct _KDPC* Dpc,
	_In_opt_ PVOID DeferredContext,
	_In_opt_ PVOID SystemArgument1,
	_In_opt_ PVOID SystemArgument2
)
{
	KdPrint(("func:%s\n", __FUNCTION__));
	// ExFreePool(Dpc); 有间隔时不能释放，因为还要继续调用
}

VOID DpcTest()
{
	PKDPC pKDPC = ExAllocatePool(NonPagedPool, sizeof(KDPC));
	if (pKDPC == NULL) return;
	KeInitializeDpc(pKDPC, DpcRoutineCallback, NULL);
	//KeInsertQueueDpc(pKDPC, NULL, NULL);
	LARGE_INTEGER time = { 0 };
	time.QuadPart = -50000 * 1000; // 单位是100纳秒

	PKTIMER pTime = ExAllocatePool(NonPagedPool, sizeof(KTIMER));
	KeInitializeTimer(pTime);
	//KeSetTimer(pTime, time, pKDPC);
	KeSetTimerEx(pTime, time, 1000,pKDPC); // 比KeSetTimer多了一个间隔时间
}

VOID WorkItemCallback(
	_In_ PVOID Parameter
)
{
	// 可延时
	KdPrint(("func:%s\n", __FUNCTION__));
}

VOID TestWorkItem()
{
	PWORK_QUEUE_ITEM p = ExAllocatePool(PagedPool, sizeof(WORK_QUEUE_ITEM));
	if (p == NULL) return;
	ExInitializeWorkItem(p, WorkItemCallback, NULL);
	ExQueueWorkItem(p, DelayedWorkQueue);
}
// 进程通知回调函数
VOID ProcessNotifyRoutine(
	_In_ HANDLE ParentId,
	_In_ HANDLE ProcessId,
	_In_ BOOLEAN Create
)
{
	UNREFERENCED_PARAMETER(ParentId);
	PEPROCESS process;
	NTSTATUS status = PsLookupProcessByProcessId(ProcessId, &process);

	if (NT_SUCCESS(status)) {
		if (Create) {
			// 进程创建
			KdPrint(("Process created: %p, Process name: %s\n", ProcessId, PsGetProcessImageFileName(process)));
			// 在这里可以使用 process 变量进行操作
		}
		else {
			// 进程终止
			KdPrint(("Process terminated: %p, Process name: %s\n", ProcessId, PsGetProcessImageFileName(process)));
		}

		// 释放 PEPROCESS 结构体
		ObDereferenceObject(process);
	}
	else {
		KdPrint(("Failed to lookup process: %p\n", ProcessId));
	}
}

VOID ProcessNotifyRoutineEx(
	_Inout_ PEPROCESS Process,
	_In_ HANDLE ProcessId,
	_Inout_opt_ PPS_CREATE_NOTIFY_INFO CreateInfo
)
{
	// 获取进程映像文件名
	UCHAR* ImageName = PsGetProcessImageFileName(Process);
	if (CreateInfo != NULL) {
		// 检查进程映像文件名是否为 "calc.exe"
		if (strcmp((char*)ImageName, "calc.exe") == 0) {
			CreateInfo->CreationStatus = STATUS_ACCESS_DENIED;
			KdPrint(("Blocked process creation: %s\n", ImageName));
		}
		else
		{
			KdPrint(("Create process image file name: %s, pid : %p\n", ImageName, ProcessId));
		}
	}
	else {
		KdPrint(("Exit process image file name: %s, pid : %p\n", ImageName, ProcessId));
	}
}

NTSTATUS QueryMemory(ULONG64 Pid, ULONG64 VirtualAddress, PMEMORY_BASIC_INFORMATION pInfo)
{
	if (!MmIsAddressValid(pInfo))
	{
		return STATUS_INVALID_PARAMETER;
	}

	PEPROCESS process = NULL;
	NTSTATUS st = PsLookupProcessByProcessId(Pid, &process);
	if (!NT_SUCCESS(st))
	{
		return st;
	}

	if (PsGetProcessExitStatus(process) != STATUS_PENDING)
	{
		// 判断进程是否还在运行，若没有在运行则返回失败
		ObDereferenceObject(process);
		return STATUS_UNSUCCESSFUL;
	}

	KAPC_STATE ApcState = { 0 };

	KeStackAttachProcess(process, &ApcState);

	SIZE_T retLen = 0;
	st = ZwQueryVirtualMemory(NtCurrentProcess(), 
		VirtualAddress, 
		MemoryBasicInformation, 
		pInfo,
		sizeof(MEMORY_BASIC_INFORMATION), 
		&retLen);

	KeUnstackDetachProcess(&ApcState);

	ObDereferenceObject(process);
	return st;
}




ULONG64 QueryExeMoudle(ULONG64 Pid)
{

	PEPROCESS process = NULL;
	NTSTATUS st = PsLookupProcessByProcessId(Pid, &process);
	if (!NT_SUCCESS(st))
	{
		return st;
	}
	ObDereferenceObject(process);
	return PsGetProcessSectionBaseAddress(process);
}