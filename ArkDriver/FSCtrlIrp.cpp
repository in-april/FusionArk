#include "FSCtrlIrp.h"

#define FILE_TRANS_BUF_MAXLEN (4*1024*1024)

VOID FSCtrlIrp::FreeMdl(IN PMDL pMdl)
{
	PMDL pMdlCurrent = pMdl, pMdlNext;

	while (pMdlCurrent != NULL) {
		pMdlNext = pMdlCurrent->Next;

		if (pMdlCurrent->MdlFlags & MDL_PAGES_LOCKED) {
			MmUnlockPages(pMdlCurrent);
		}
		IoFreeMdl(pMdlCurrent);

		pMdlCurrent = pMdlNext;
	}
}

NTSTATUS FSCtrlIrp::FileOperationCompletion(IN PDEVICE_OBJECT pDeviceObject, IN PIRP pIrp, IN PVOID pContext OPTIONAL)
{
	PKEVENT pkEvent = pIrp->UserEvent;

	ASSERT(pkEvent != NULL);

	//趁机拷贝完成状态
	RtlCopyMemory(
		pIrp->UserIosb,
		&pIrp->IoStatus,
		sizeof(IO_STATUS_BLOCK)
	);

	//检查并释放MDL
	if (pIrp->MdlAddress != NULL) {
		FreeMdl(pIrp->MdlAddress);
		pIrp->MdlAddress = NULL;
	}

	//释放IRP
	IoFreeIrp(pIrp);

	//设置事件
	KeSetEvent(pkEvent,
		IO_NO_INCREMENT,
		FALSE
	);

	return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS FSCtrlIrp::IrpCreateFile(OUT PFILE_OBJECT* ppFileObject, OUT PDEVICE_OBJECT* ppDeviceObject, /*如果成功，这里保存:(*ppFileObject)->Vpb->DeviceObject */ IN ACCESS_MASK DesiredAccess, IN PUNICODE_STRING punsFilePath, /*必须以"盘符:\"开头, 比如: "C:\..." */ OUT PIO_STATUS_BLOCK pIoStatusBlock, IN PLARGE_INTEGER AllocationSize OPTIONAL, IN ULONG FileAttributes, IN ULONG ShareAccess, IN ULONG Disposition, IN ULONG CreateOptions, IN PVOID EaBuffer OPTIONAL, IN ULONG EaLength)
{
	PAUX_ACCESS_DATA pAuxData = NULL;
	HANDLE hRoot = NULL;
	PFILE_OBJECT pRootObject = NULL, pFileObject = NULL;
	PDEVICE_OBJECT pDeviceObject = NULL, pRealDeviceObject = NULL;
	PIRP pIrp;
	PIO_STACK_LOCATION pIrpSp;
	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
	ACCESS_STATE AccessState;
	KEVENT kEvent;
	IO_SECURITY_CONTEXT IoSecurityContext;
	OBJECT_ATTRIBUTES objAttrs;
	UNICODE_STRING unsRootPath;
	IO_STATUS_BLOCK userIosb;
	WCHAR wRootPath[8];  //"\??\C:\"

	//路径长度检查(最短: "C:\")
	if (punsFilePath->Length < 3 * sizeof(CHAR)) {
		return STATUS_INVALID_PARAMETER;
	}

	RtlZeroMemory(pIoStatusBlock, sizeof(IO_STATUS_BLOCK));

	//根目录符号链接
	ntStatus = RtlStringCbPrintfW(
		wRootPath,
		sizeof(wRootPath),
		L"\\??\\%c:\\",
		punsFilePath->Buffer[0]);
	ASSERT(NT_SUCCESS(ntStatus));
	RtlInitUnicodeString(&unsRootPath, (PCWSTR)wRootPath);

	do {
		//打开根目录
		InitializeObjectAttributes(&objAttrs,
			&unsRootPath,
			OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
			NULL,
			NULL);
		ntStatus = IoCreateFile(&hRoot,
			FILE_READ_ATTRIBUTES | SYNCHRONIZE,
			&objAttrs,
			&userIosb,
			NULL,
			FILE_ATTRIBUTE_NORMAL,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			FILE_OPEN,
			FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
			NULL,
			0,
			CreateFileTypeNone,
			NULL,
			IO_NO_PARAMETER_CHECKING);
		*pIoStatusBlock = userIosb;
		if (!NT_SUCCESS(ntStatus)) {
			break;
		}

		//得到根目录文件对象
		ntStatus = ObReferenceObjectByHandle(hRoot,
			FILE_READ_ATTRIBUTES,
			*IoFileObjectType,
			KernelMode,
			(PVOID*)&pRootObject,
			NULL);
		if (!NT_SUCCESS(ntStatus)) {
			break;
		}
		if (pRootObject->Vpb == NULL || pRootObject->Vpb->DeviceObject == NULL || pRootObject->Vpb->RealDevice == NULL) {
			ntStatus = STATUS_INVALID_PARAMETER;
			break;
		}

		//得到卷设备
		pDeviceObject = pRootObject->Vpb->DeviceObject;  //文件系统卷设备
		pRealDeviceObject = pRootObject->Vpb->RealDevice; //卷管理器生成的卷设备(物理设备)
		ObDereferenceObject((PVOID)pRootObject);
		pRootObject = NULL;
		ZwClose(hRoot);
		hRoot = NULL;

		//创建文件对象
		InitializeObjectAttributes(&objAttrs,
			NULL,
			OBJ_CASE_INSENSITIVE,
			NULL,
			NULL);
		ntStatus = ObCreateObject(
			KernelMode,
			*IoFileObjectType,
			&objAttrs,
			KernelMode,
			NULL,
			sizeof(FILE_OBJECT),
			0,
			0,
			(PVOID*)&pFileObject);
		if (!NT_SUCCESS(ntStatus)) {
			break;
		}

		//填写文件对象内容
		RtlZeroMemory(pFileObject,
			sizeof(FILE_OBJECT)
		);
		pFileObject->Type = IO_TYPE_FILE;
		pFileObject->Size = sizeof(FILE_OBJECT);
		pFileObject->DeviceObject = pRealDeviceObject;
		pFileObject->Flags = FO_SYNCHRONOUS_IO;
		pFileObject->FileName.Buffer = &punsFilePath->Buffer[2];
		pFileObject->FileName.Length = punsFilePath->Length - 2 * sizeof(WCHAR);
		pFileObject->FileName.MaximumLength = punsFilePath->MaximumLength - 2 * sizeof(WCHAR);
		KeInitializeEvent(&pFileObject->Lock,
			SynchronizationEvent,
			FALSE);
		KeInitializeEvent(&pFileObject->Event,
			NotificationEvent,
			FALSE);

		//分配AUX_ACCESS_DATA缓冲区
		pAuxData = (PAUX_ACCESS_DATA)ExAllocatePool(NonPagedPool,
			sizeof(AUX_ACCESS_DATA));
		if (pAuxData == NULL) {
			ntStatus = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		//设置安全状态
		ntStatus = SeCreateAccessState(&AccessState,
			pAuxData,
			DesiredAccess,
			IoGetFileObjectGenericMapping());
		if (!NT_SUCCESS(ntStatus)) {
			break;
		}

		//分配IRP
		pIrp = IoAllocateIrp(pDeviceObject->StackSize,
			FALSE);
		if (pIrp == NULL) {
			ntStatus = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		//填写IRP
		pIrp->MdlAddress = NULL;
		pIrp->UserBuffer = NULL;
		pIrp->AssociatedIrp.SystemBuffer = EaBuffer;
		pIrp->Flags = IRP_CREATE_OPERATION | IRP_SYNCHRONOUS_API;
		pIrp->RequestorMode = KernelMode;
		pIrp->PendingReturned = FALSE;
		pIrp->Cancel = FALSE;
		pIrp->CancelRoutine = NULL;
		KeInitializeEvent(&kEvent,
			NotificationEvent,
			FALSE);
		pIrp->UserEvent = &kEvent;
		pIrp->UserIosb = &userIosb;
		pIrp->Overlay.AllocationSize.QuadPart = AllocationSize != NULL ? AllocationSize->QuadPart : 0;

		pIrp->Tail.Overlay.Thread = PsGetCurrentThread();
		pIrp->Tail.Overlay.OriginalFileObject = pFileObject;
		pIrp->Tail.Overlay.AuxiliaryBuffer = NULL;

		//得到下层栈空间
		pIrpSp = IoGetNextIrpStackLocation(pIrp);
		ASSERT(pIrpSp != NULL);

		//填写下层栈空间
		pIrpSp->MajorFunction = IRP_MJ_CREATE;
		IoSecurityContext.SecurityQos = NULL;
		IoSecurityContext.AccessState = &AccessState;
		IoSecurityContext.DesiredAccess = DesiredAccess;
		IoSecurityContext.FullCreateOptions = 0;
		pIrpSp->Parameters.Create.SecurityContext = &IoSecurityContext;
		pIrpSp->Parameters.Create.Options = (Disposition << 24) | CreateOptions;
		pIrpSp->Parameters.Create.FileAttributes = (USHORT)FileAttributes;
		pIrpSp->Parameters.Create.ShareAccess = (USHORT)ShareAccess;
		pIrpSp->Parameters.Create.EaLength = EaLength;
		pIrpSp->FileObject = pFileObject;
		pIrpSp->DeviceObject = pDeviceObject;

		//设置完成例程: IoSetCompletionRoutine(pIrp,FileOperationCompletion,NULL,TRUE,TRUE,TRUE);
		pIrpSp->CompletionRoutine = FileOperationCompletion;
		pIrpSp->Context = NULL;
		pIrpSp->Control = SL_INVOKE_ON_SUCCESS | SL_INVOKE_ON_ERROR | SL_INVOKE_ON_CANCEL;

		//下发IRP请求
		ntStatus = IoCallDriver(pDeviceObject,
			pIrp);

		//等候完成
		(VOID)KeWaitForSingleObject(&kEvent,
			Executive,
			KernelMode,
			FALSE,
			NULL);

		//得到完成状态
		*pIoStatusBlock = userIosb;
		ntStatus = pIoStatusBlock->Status;
	} while (FALSE);

	if (NT_SUCCESS(ntStatus)) {
		//成功后的处理
		ASSERT(pFileObject != NULL && pFileObject->DeviceObject != NULL);
		InterlockedIncrement(&pFileObject->DeviceObject->ReferenceCount);
		if (pFileObject->Vpb != NULL) {
			InterlockedIncrement((PLONG)&pFileObject->Vpb->ReferenceCount);
		}
		*ppFileObject = pFileObject;
		*ppDeviceObject = pDeviceObject;
	}
	else {
		//失败后的处理
		if (pFileObject != NULL) {
			pFileObject->DeviceObject = NULL;
			ObDereferenceObject(pFileObject);
		}
		pIoStatusBlock->Status = ntStatus;
	}

	//检查并释放相关资源
	if (pAuxData != NULL) {
		ExFreePool(pAuxData);
	}
	if (pRootObject != NULL) {
		ObDereferenceObject((PVOID)pRootObject);
	}
	if (hRoot != NULL) {
		ZwClose(hRoot);
	}

	return ntStatus;
}

NTSTATUS FSCtrlIrp::IrpCleanupFile(IN PDEVICE_OBJECT pDeviceObject, IN PFILE_OBJECT pFileObject)
{
	PIRP pIrp;
	PIO_STACK_LOCATION pIrpSp;
	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
	KEVENT kEvent;
	IO_STATUS_BLOCK userIosb;

	//卷参数块检查
	if (pFileObject->Vpb == NULL || pFileObject->Vpb->DeviceObject == NULL) {
		return STATUS_INVALID_PARAMETER;
	}

	//
	// IRP_MJ_CLEANUP
	//
	//分配IRP
	pIrp = IoAllocateIrp(pDeviceObject->StackSize,
		FALSE);
	if (pIrp == NULL) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//填写IRP
	pIrp->MdlAddress = NULL;
	pIrp->AssociatedIrp.SystemBuffer = NULL;
	pIrp->UserBuffer = NULL;
	pIrp->Flags = IRP_CLOSE_OPERATION | IRP_SYNCHRONOUS_API;
	pIrp->RequestorMode = KernelMode;
	pIrp->UserIosb = &userIosb;
	KeInitializeEvent(&kEvent,
		NotificationEvent,
		FALSE);
	pIrp->UserEvent = &kEvent;
	pIrp->Tail.Overlay.Thread = PsGetCurrentThread();
	pIrp->Tail.Overlay.OriginalFileObject = pFileObject;

	//得到下层栈空间
	pIrpSp = IoGetNextIrpStackLocation(pIrp);
	ASSERT(pIrpSp != NULL);

	//填写下层栈空间
	pIrpSp->MajorFunction = IRP_MJ_CLEANUP;
	pIrpSp->FileObject = pFileObject;
	pIrpSp->DeviceObject = pDeviceObject;

	//设置完成例程: IoSetCompletionRoutine(pIrp,FileOperationCompletion,NULL,TRUE,TRUE,TRUE);
	pIrpSp->CompletionRoutine = FileOperationCompletion;
	pIrpSp->Context = NULL;
	pIrpSp->Control = SL_INVOKE_ON_SUCCESS | SL_INVOKE_ON_ERROR | SL_INVOKE_ON_CANCEL;

	//下发请求
	ntStatus = IoCallDriver(pDeviceObject,
		pIrp);

	//等待请求结束
	KeWaitForSingleObject(&kEvent,
		Executive,
		KernelMode,
		FALSE,
		NULL);

	//得到完成状态
	ntStatus = userIosb.Status;

	return ntStatus;
}

NTSTATUS FSCtrlIrp::IrpCloseFile(IN PDEVICE_OBJECT pDeviceObject, IN PFILE_OBJECT pFileObject)
{
	PIRP pIrp;
	PIO_STACK_LOCATION pIrpSp;
	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
	KEVENT kEvent;
	IO_STATUS_BLOCK userIosb;

	//检查并设置文件打开取消标志
	if (pFileObject->Vpb != NULL && !(pFileObject->Flags & FO_DIRECT_DEVICE_OPEN)) {
		InterlockedDecrement((volatile LONG*)&pFileObject->Vpb->ReferenceCount);
		pFileObject->Flags |= FO_FILE_OPEN_CANCELLED;
	}

	//分配IRP
	pIrp = IoAllocateIrp(pDeviceObject->StackSize,
		FALSE);
	if (pIrp == NULL) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//填写IRP
	pIrp->MdlAddress = NULL;
	pIrp->AssociatedIrp.SystemBuffer = NULL;
	pIrp->UserBuffer = NULL;
	pIrp->Flags = IRP_CLOSE_OPERATION | IRP_SYNCHRONOUS_API;
	pIrp->RequestorMode = KernelMode;
	pIrp->UserIosb = &userIosb;
	KeInitializeEvent(&kEvent,
		NotificationEvent,
		FALSE);
	pIrp->UserEvent = &kEvent;
	pIrp->Tail.Overlay.Thread = PsGetCurrentThread();
	pIrp->Tail.Overlay.OriginalFileObject = pFileObject;

	//得到IRP下层栈空间
	pIrpSp = IoGetNextIrpStackLocation(pIrp);
	ASSERT(pIrpSp != NULL);

	//填写IRP下层栈空间
	pIrpSp->MajorFunction = IRP_MJ_CLOSE;
	pIrpSp->FileObject = pFileObject;
	pIrpSp->DeviceObject = pDeviceObject;

	//设置完成例程: IoSetCompletionRoutine(pIrp,FileOperationCompletion,NULL,TRUE,TRUE,TRUE);
	pIrpSp->CompletionRoutine = FileOperationCompletion;
	pIrpSp->Context = NULL;
	pIrpSp->Control = SL_INVOKE_ON_SUCCESS | SL_INVOKE_ON_ERROR | SL_INVOKE_ON_CANCEL;

	//下发IRP请求
	ntStatus = IoCallDriver(pDeviceObject,
		pIrp);

	//等待请求结束
	KeWaitForSingleObject(&kEvent,
		Executive,
		KernelMode,
		FALSE,
		NULL);

	//得到完成状态
	ntStatus = userIosb.Status;

	return ntStatus;
}

NTSTATUS FSCtrlIrp::IrpReadOrWriteFile(IN PDEVICE_OBJECT pDeviceObject, IN PFILE_OBJECT pFileObject, OUT PIO_STATUS_BLOCK pIoStatusBlock, IN OUT PVOID pBuffer, IN ULONG ulLength, IN PLARGE_INTEGER pliByteOffset OPTIONAL, IN BOOLEAN bRead)
{
	PIRP pIrp;
	PIO_STACK_LOCATION pIrpSp;
	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
	IO_STATUS_BLOCK userIosb;
	KEVENT kEvent;

	//分配IRP
	pIrp = IoAllocateIrp(
		pDeviceObject->StackSize,
		FALSE);
	RtlZeroMemory(pIoStatusBlock,
		sizeof(IO_STATUS_BLOCK));
	if (pIrp == NULL) {
		pIoStatusBlock->Status = STATUS_INSUFFICIENT_RESOURCES;
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//填写IRP
	pIrp->UserBuffer = NULL;
	pIrp->MdlAddress = NULL;
	pIrp->AssociatedIrp.SystemBuffer = NULL;
	if (pDeviceObject->Flags & DO_BUFFERED_IO) {
		//缓冲读写方式
		pIrp->AssociatedIrp.SystemBuffer = pBuffer;
	}
	else if (pDeviceObject->Flags & DO_DIRECT_IO) {
		//直接读写方式
		pIrp->MdlAddress = IoAllocateMdl(
			pBuffer,
			ulLength,
			FALSE,
			FALSE,
			NULL);
		if (pIrp->MdlAddress == NULL) {
			IoFreeIrp(pIrp);
			pIoStatusBlock->Status = STATUS_INSUFFICIENT_RESOURCES;
			return STATUS_INSUFFICIENT_RESOURCES;
		}
		MmBuildMdlForNonPagedPool(pIrp->MdlAddress);
	}
	else {
		//非缓冲非直接读写方式: 在文件读写请求中比较常见
		pIrp->UserBuffer = pBuffer;
	}
	pIrp->Flags = IRP_DEFER_IO_COMPLETION | IRP_NOCACHE;
	pIrp->Flags |= (bRead ? IRP_READ_OPERATION : IRP_WRITE_OPERATION);
	pIrp->RequestorMode = KernelMode;
	pIrp->UserIosb = &userIosb;
	KeInitializeEvent(&kEvent,
		NotificationEvent,
		FALSE);
	pIrp->UserEvent = &kEvent;
	pIrp->Tail.Overlay.Thread = PsGetCurrentThread();
	pIrp->Tail.Overlay.OriginalFileObject = pFileObject;

	//获取下层栈空间
	pIrpSp = IoGetNextIrpStackLocation(pIrp);
	ASSERT(pIrpSp != NULL);

	pIrpSp->MajorFunction = (bRead ? IRP_MJ_READ : IRP_MJ_WRITE);
	pIrpSp->MinorFunction = IRP_MN_NORMAL;
	if (bRead) {
		pIrpSp->Parameters.Read.Length = ulLength;
		pIrpSp->Parameters.Read.ByteOffset.QuadPart = 0;
		if (pliByteOffset != NULL) {
			pIrpSp->Parameters.Read.ByteOffset.QuadPart = pliByteOffset->QuadPart;
		}
	}
	else {
		pIrpSp->Parameters.Write.Length = ulLength;
		pIrpSp->Parameters.Write.ByteOffset.QuadPart = 0;
		if (pliByteOffset != NULL) {
			pIrpSp->Parameters.Write.ByteOffset.QuadPart = pliByteOffset->QuadPart;
		}
	}
	pIrpSp->FileObject = pFileObject;
	pIrpSp->DeviceObject = pDeviceObject;

	//设置完成例程: IoSetCompletionRoutine(pIrp,FileOperationCompletion,NULL,TRUE,TRUE,TRUE);
	pIrpSp->CompletionRoutine = FileOperationCompletion;
	pIrpSp->Context = NULL;
	pIrpSp->Control = SL_INVOKE_ON_SUCCESS | SL_INVOKE_ON_ERROR | SL_INVOKE_ON_CANCEL;

	//下发IRP请求
	ntStatus = IoCallDriver(pDeviceObject,
		pIrp);

	//等候IRP完成
	KeWaitForSingleObject((PVOID)&kEvent,
		Executive,
		KernelMode,
		FALSE,
		NULL);

	//保存完成状态
	*pIoStatusBlock = userIosb;
	ntStatus = pIoStatusBlock->Status;

	return ntStatus;
}

NTSTATUS FSCtrlIrp::IrpSetInformationFile(IN PDEVICE_OBJECT pDeviceObject, IN PFILE_OBJECT pFileObject, IN PFILE_OBJECT pTargetFileObject OPTIONAL, OUT PIO_STATUS_BLOCK pIoStatusBlock, IN PVOID pFileInformation, IN ULONG ulLength, IN FILE_INFORMATION_CLASS FileInformationClass)
{
	PIRP pIrp = NULL;
	PIO_STACK_LOCATION pIrpSp;
	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
	IO_STATUS_BLOCK userIosb;
	KEVENT kEvent;

	//分配IRP
	pIrp = IoAllocateIrp(
		pDeviceObject->StackSize,
		FALSE);
	RtlZeroMemory(pIoStatusBlock,
		sizeof(IO_STATUS_BLOCK));
	if (pIrp == NULL) {
		pIoStatusBlock->Status = STATUS_INSUFFICIENT_RESOURCES;
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//填写IRP
	pIrp->MdlAddress = NULL;
	pIrp->UserBuffer = NULL;
	pIrp->AssociatedIrp.SystemBuffer = pFileInformation;
	pIrp->Flags = IRP_SYNCHRONOUS_API;
	pIrp->RequestorMode = KernelMode;
	pIrp->UserIosb = &userIosb;
	KeInitializeEvent(&kEvent,
		NotificationEvent,
		FALSE);
	pIrp->UserEvent = &kEvent;
	pIrp->Tail.Overlay.Thread = PsGetCurrentThread();
	pIrp->Tail.Overlay.OriginalFileObject = pFileObject;

	//得到下层栈空间
	pIrpSp = IoGetNextIrpStackLocation(pIrp);
	ASSERT(pIrpSp != NULL);

	//填写栈空间
	pIrpSp->MajorFunction = IRP_MJ_SET_INFORMATION;
	pIrpSp->Parameters.SetFile.Length = ulLength;
	pIrpSp->Parameters.SetFile.FileInformationClass = FileInformationClass;
	pIrpSp->Parameters.SetFile.FileObject = pTargetFileObject;
	//对于文件重命名,创建硬链接需要考虑: ReplaceIfExists
	switch (FileInformationClass) {
	case FileRenameInformation:
		pIrpSp->Parameters.SetFile.ReplaceIfExists = ((PFILE_RENAME_INFORMATION)pFileInformation)->ReplaceIfExists;
		break;
	case FileLinkInformation:
		pIrpSp->Parameters.SetFile.ReplaceIfExists = ((PFILE_LINK_INFORMATION)pFileInformation)->ReplaceIfExists;
		break;
	}
	pIrpSp->FileObject = pFileObject;
	pIrpSp->DeviceObject = pDeviceObject;

	//设置完成例程: IoSetCompletionRoutine(pIrp,FileOperationCompletion,NULL,TRUE,TRUE,TRUE);
	pIrpSp->CompletionRoutine = FileOperationCompletion;
	pIrpSp->Context = NULL;
	pIrpSp->Control = SL_INVOKE_ON_SUCCESS | SL_INVOKE_ON_ERROR | SL_INVOKE_ON_CANCEL;

	//下发IRP请求
	ntStatus = IoCallDriver(pDeviceObject,
		pIrp);

	//等候IRP完成
	KeWaitForSingleObject((PVOID)&kEvent,
		Executive,
		KernelMode,
		FALSE,
		NULL);

	//保存完成状态
	*pIoStatusBlock = userIosb;
	ntStatus = pIoStatusBlock->Status;

	return ntStatus;
}

NTSTATUS FSCtrlIrp::IrpQueryInformationFile(IN PDEVICE_OBJECT pDeviceObject, IN PFILE_OBJECT pFileObject, OUT PIO_STATUS_BLOCK pIoStatusBlock, OUT PVOID pFileInformation, IN ULONG ulLength, IN FILE_INFORMATION_CLASS FileInformationClass)
{
	PIRP pIrp;
	PIO_STACK_LOCATION pIrpSp;
	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
	IO_STATUS_BLOCK userIosb;
	KEVENT kEvent;

	//分配IRP
	pIrp = IoAllocateIrp(pDeviceObject->StackSize,
		FALSE);
	RtlZeroMemory(pIoStatusBlock,
		sizeof(IO_STATUS_BLOCK));
	if (pIrp == NULL) {
		pIoStatusBlock->Status = STATUS_INSUFFICIENT_RESOURCES;
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//填写IRP
	pIrp->MdlAddress = NULL;
	pIrp->AssociatedIrp.SystemBuffer = pFileInformation;
	pIrp->UserBuffer = NULL;
	pIrp->Flags = IRP_SYNCHRONOUS_API;
	pIrp->RequestorMode = KernelMode;
	pIrp->UserIosb = &userIosb;
	KeInitializeEvent(&kEvent,
		NotificationEvent,
		FALSE);
	pIrp->UserEvent = &kEvent;
	pIrp->Tail.Overlay.Thread = PsGetCurrentThread();
	pIrp->Tail.Overlay.OriginalFileObject = pFileObject;

	//获取下层栈空间
	pIrpSp = IoGetNextIrpStackLocation(pIrp);
	ASSERT(pIrpSp != NULL);

	//填写栈空间
	pIrpSp->MajorFunction = IRP_MJ_QUERY_INFORMATION;
	pIrpSp->Parameters.QueryFile.FileInformationClass = FileInformationClass;
	pIrpSp->Parameters.QueryFile.Length = ulLength;
	pIrpSp->FileObject = pFileObject;
	pIrpSp->DeviceObject = pDeviceObject;

	//设置完成例程: IoSetCompletionRoutine(pIrp,FileOperationCompletion,NULL,TRUE,TRUE,TRUE);
	pIrpSp->CompletionRoutine = FileOperationCompletion;
	pIrpSp->Context = NULL;
	pIrpSp->Control = SL_INVOKE_ON_SUCCESS | SL_INVOKE_ON_ERROR | SL_INVOKE_ON_CANCEL;

	//下发IRP请求
	ntStatus = IoCallDriver(
		pDeviceObject,
		pIrp);

	//等候请求完成
	KeWaitForSingleObject(
		(PVOID)&kEvent,
		Executive,
		KernelMode,
		FALSE,
		NULL);

	//得到完成状态
	*pIoStatusBlock = userIosb;
	ntStatus = pIoStatusBlock->Status;

	return ntStatus;
}

NTSTATUS FSCtrlIrp::IrpQueryDirectoryFile(IN PDEVICE_OBJECT pDeviceObject, IN PFILE_OBJECT pFileObject, OUT PIO_STATUS_BLOCK pIoStatusBlock, OUT PVOID pFileInformation, IN ULONG ulLength, IN FILE_INFORMATION_CLASS FileInformationClass, IN BOOLEAN bReturnSingleEntry, IN PUNICODE_STRING punsFileName OPTIONAL, IN BOOLEAN bRestartScan)
{
	PIRP pIrp;
	PIO_STACK_LOCATION pIrpSp;
	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
	KEVENT kEvent;
	IO_STATUS_BLOCK userIosb;

	//分配IRP
	pIrp = IoAllocateIrp(pDeviceObject->StackSize,
		FALSE);
	RtlZeroMemory(pIoStatusBlock,
		sizeof(IO_STATUS_BLOCK));
	if (pIrp == NULL) {
		pIoStatusBlock->Status = STATUS_INSUFFICIENT_RESOURCES;
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//填写IRP
	pIrp->MdlAddress = NULL;
	pIrp->AssociatedIrp.SystemBuffer = NULL;
	pIrp->UserBuffer = pFileInformation;
	pIrp->Flags = IRP_SYNCHRONOUS_API;
	pIrp->RequestorMode = KernelMode;
	pIrp->UserIosb = &userIosb;
	KeInitializeEvent(&kEvent,
		NotificationEvent,
		FALSE);
	pIrp->UserEvent = &kEvent;
	pIrp->Tail.Overlay.Thread = PsGetCurrentThread();
	pIrp->Tail.Overlay.OriginalFileObject = pFileObject;

	//得到下层栈空间
	pIrpSp = IoGetNextIrpStackLocation(pIrp);
	ASSERT(pIrpSp != NULL);

	//填写下层栈空间
	pIrpSp->MajorFunction = IRP_MJ_DIRECTORY_CONTROL;
	pIrpSp->MinorFunction = IRP_MN_QUERY_DIRECTORY;
	pIrpSp->Flags = 0;
	if (bReturnSingleEntry) pIrpSp->Flags |= SL_RETURN_SINGLE_ENTRY;
	if (bRestartScan) pIrpSp->Flags |= SL_RESTART_SCAN;
	pIrpSp->Parameters.QueryDirectory.Length = ulLength;
	pIrpSp->Parameters.QueryDirectory.FileName = punsFileName;
	pIrpSp->Parameters.QueryDirectory.FileInformationClass = FileInformationClass;
	pIrpSp->DeviceObject = pDeviceObject;
	pIrpSp->FileObject = pFileObject;

	//设置完成例程: IoSetCompletionRoutine(pIrp, FileOperationCompletion, NULL, TRUE, TRUE, TRUE);
	pIrpSp->CompletionRoutine = FileOperationCompletion;
	pIrpSp->Context = NULL;
	pIrpSp->Control = SL_INVOKE_ON_SUCCESS | SL_INVOKE_ON_ERROR | SL_INVOKE_ON_CANCEL;

	//下发IO请求
	ntStatus = IoCallDriver(pDeviceObject,
		pIrp);

	//等候请求完成
	KeWaitForSingleObject((PVOID)&kEvent,
		Executive,
		KernelMode,
		FALSE,
		NULL);

	//得到完成状态
	*pIoStatusBlock = userIosb;
	ntStatus = pIoStatusBlock->Status;

	return ntStatus;
}

NTSTATUS FSCtrlIrp::RootkitFindFileItem(IN PDEVICE_OBJECT pDeviceObject, IN PFILE_OBJECT pFileObject, IN BOOLEAN bRestartScan, OUT PFIND_FILE_OUTPUT pFindFileOut)
{
	PFILE_BOTH_DIR_INFORMATION pFileBothDirInfo;
	IO_STATUS_BLOCK iosb;
	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;

	PAGED_CODE();
	ASSERT(pDeviceObject != NULL && pFileObject != NULL && pFindFileOut != NULL);

	//分配缓冲区
	pFileBothDirInfo = (PFILE_BOTH_DIR_INFORMATION)ExAllocatePool(PagedPool,
		PAGE_SIZE);
	if (pFileBothDirInfo == NULL) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	RtlZeroMemory(pFileBothDirInfo,
		PAGE_SIZE);

	//发送IRP请求查询目录下的1项
	ntStatus = IrpQueryDirectoryFile(pDeviceObject,
		pFileObject,
		&iosb,
		(PVOID)pFileBothDirInfo,
		PAGE_SIZE,
		FileBothDirectoryInformation,
		TRUE,
		NULL,
		bRestartScan);
	if (NT_SUCCESS(ntStatus)) {
		//成功了就进行保存
		RtlZeroMemory(pFindFileOut,
			sizeof(FIND_FILE_OUTPUT));
		pFindFileOut->CreationTime.QuadPart = pFileBothDirInfo->CreationTime.QuadPart;
		pFindFileOut->LastAccessTime.QuadPart = pFileBothDirInfo->LastAccessTime.QuadPart;
		pFindFileOut->LastWriteTime.QuadPart = pFileBothDirInfo->LastWriteTime.QuadPart;
		pFindFileOut->ChangeTime.QuadPart = pFileBothDirInfo->ChangeTime.QuadPart;
		pFindFileOut->EndOfFile.QuadPart = pFileBothDirInfo->EndOfFile.QuadPart;
		pFindFileOut->AllocationSize.QuadPart = pFileBothDirInfo->AllocationSize.QuadPart;
		pFindFileOut->ulFileAttributes = pFileBothDirInfo->FileAttributes;
		if (pFileBothDirInfo->ShortNameLength > 0) {
			RtlCopyMemory(pFindFileOut->wShortFileName,
				pFileBothDirInfo->ShortName,
				(SIZE_T)pFileBothDirInfo->ShortNameLength);
			pFindFileOut->wShortFileName[sizeof(pFindFileOut->wShortFileName) / sizeof(WCHAR) - 1] = L'\0';
		}
		if (pFileBothDirInfo->FileNameLength > 0) {
			if (pFileBothDirInfo->FileNameLength > sizeof(pFindFileOut->wFileName)) {
				pFileBothDirInfo->FileNameLength = sizeof(pFindFileOut->wFileName);
			}
			RtlCopyMemory(pFindFileOut->wFileName,
				pFileBothDirInfo->FileName,
				(SIZE_T)pFileBothDirInfo->FileNameLength);
			pFindFileOut->wFileName[sizeof(pFindFileOut->wFileName) / sizeof(WCHAR) - 1] = L'\0';
		}
	}

	//释放缓冲区
	ExFreePool((PVOID)pFileBothDirInfo);

	return ntStatus;
}

NTSTATUS FSCtrlIrp::RootkitFindFirstFileForIoctl(IN LPCWSTR wDirPath, OUT PFIND_FIRST_FILE_OUTPUT pFindFirstFileOutput)
{
	PDEVICE_OBJECT pDeviceObject = NULL;
	PFILE_OBJECT pFileObject = NULL;
	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL, ntLocStatus;
	UNICODE_STRING unsDirPath;
	IO_STATUS_BLOCK iosb;

	PAGED_CODE();
	ASSERT(wDirPath != NULL && pFindFirstFileOutput != NULL);

	do {
		//检查目录路径是否以"\??\"打头
		RtlInitUnicodeString(&unsDirPath,
			wDirPath);
		if (unsDirPath.Length >= 4 * sizeof(WCHAR)) {
			if (unsDirPath.Buffer[0] == L'\\'
				&& unsDirPath.Buffer[1] == L'?'
				&& unsDirPath.Buffer[2] == L'?'
				&& unsDirPath.Buffer[3] == L'\\') {
				//如果以"\??\"打头，传入IrpCreateFile前需要去掉头部"\??\"
				unsDirPath.Buffer += 4;
				unsDirPath.Length -= 4 * sizeof(WCHAR);
				unsDirPath.MaximumLength -= 4 * sizeof(WCHAR);
			}
		}

		//自己发送IRP请求,打开目标目录
		ntStatus = IrpCreateFile(&pFileObject,
			&pDeviceObject,
			FILE_LIST_DIRECTORY | FILE_TRAVERSE | SYNCHRONIZE,
			&unsDirPath,
			&iosb,
			NULL,
			FILE_ATTRIBUTE_NORMAL,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			FILE_OPEN,
			FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
			NULL,
			0);
		if (!NT_SUCCESS(ntStatus)) {
			break;
		}

		//自己发送IRP请求,查询目录下的第1条
		ntStatus = RootkitFindFileItem(pDeviceObject,
			pFileObject,
			TRUE,
			&pFindFirstFileOutput->stFindFileItem);
		if (NT_SUCCESS(ntStatus)) {
			pFindFirstFileOutput->stFileFileHandleInfo.pDeviceObject = (PVOID)pDeviceObject;
			pFindFirstFileOutput->stFileFileHandleInfo.pFileObject = (PVOID)pFileObject;
		}
	} while (FALSE);

	if (!NT_SUCCESS(ntStatus)) {
		//失败时的处理
		if (pFileObject != NULL) {
			ASSERT(pDeviceObject != NULL);

			//自己发送IRP请求关闭查询目录
			ntLocStatus = IrpCleanupFile(pDeviceObject,
				pFileObject);
			ASSERT(NT_SUCCESS(ntLocStatus));

			ntLocStatus = IrpCloseFile(pDeviceObject,
				pFileObject);
			ASSERT(NT_SUCCESS(ntLocStatus));
		}
	}

	return ntStatus;
}

NTSTATUS FSCtrlIrp::RootkitFindNextFileForIoctl(IN PFIND_FILE_HANDLE_INFO pFileFirstFileHandleInfo, OUT PFIND_FILE_OUTPUT pFindFileOutput)
{
	NTSTATUS ntStatus;

	PAGED_CODE();
	ASSERT(pFileFirstFileHandleInfo != NULL && pFindFileOutput != NULL);

	//检查参数合法性
	if (pFileFirstFileHandleInfo->pDeviceObject == NULL || pFileFirstFileHandleInfo->pFileObject == NULL) {
		return STATUS_INVALID_PARAMETER;
	}

	//自己发送IRP请求,查询目录下的1条
	ntStatus = RootkitFindFileItem(
		(PDEVICE_OBJECT)pFileFirstFileHandleInfo->pDeviceObject,
		(PFILE_OBJECT)pFileFirstFileHandleInfo->pFileObject,
		FALSE,
		pFindFileOutput);

	return ntStatus;
}

NTSTATUS FSCtrlIrp::RootkitFindCloseForIoctl(IN PFIND_FILE_HANDLE_INFO pFileFirstFileHandleInfo)
{
	NTSTATUS ntStatus = STATUS_INVALID_PARAMETER;

	PAGED_CODE();
	ASSERT(pFileFirstFileHandleInfo != NULL);

	if (pFileFirstFileHandleInfo->pFileObject != NULL && pFileFirstFileHandleInfo->pDeviceObject != NULL) {
		//自己发送IRP请求关闭目标目录
		ntStatus = IrpCleanupFile((PDEVICE_OBJECT)pFileFirstFileHandleInfo->pDeviceObject,
			(PFILE_OBJECT)pFileFirstFileHandleInfo->pFileObject);
		if (NT_SUCCESS(ntStatus)) {
			ntStatus = IrpCloseFile((PDEVICE_OBJECT)pFileFirstFileHandleInfo->pDeviceObject,
				(PFILE_OBJECT)pFileFirstFileHandleInfo->pFileObject);
		}
	}

	return ntStatus;
}

NTSTATUS FSCtrlIrp::RootkitDeleteFile(IN PDEVICE_OBJECT pDeviceObject, IN PFILE_OBJECT pFileObject)
{
	PFILE_BASIC_INFORMATION pFileBasicInfo = NULL;
	PFILE_DISPOSITION_INFORMATION pFileDispInfo = NULL;
	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
	IO_STATUS_BLOCK iosb;
	SECTION_OBJECT_POINTERS stSectionObjPointers;

	PAGED_CODE();

	ASSERT(pDeviceObject != NULL && pFileObject != NULL);
	do {
		//分配属性缓冲区
		pFileBasicInfo = (PFILE_BASIC_INFORMATION)ExAllocatePool(PagedPool,
			sizeof(FILE_BASIC_INFORMATION));
		if (pFileBasicInfo == NULL) {
			ntStatus = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		//下发IRP_MJ_QUERY_INFORMATION请求查询文件属性
		ntStatus = IrpQueryInformationFile(pDeviceObject,
			pFileObject,
			&iosb,
			(PVOID)pFileBasicInfo,
			sizeof(FILE_BASIC_INFORMATION),
			FileBasicInformation);
		if (!NT_SUCCESS(ntStatus)) {
			break;
		}

		//检查并去掉只读属性
		if (pFileBasicInfo->FileAttributes & FILE_ATTRIBUTE_READONLY) {
			pFileBasicInfo->FileAttributes &= ~FILE_ATTRIBUTE_READONLY;

			//下发IRP_MJ_SET_INFORMATION请求去掉文件只读属性
			ntStatus = IrpSetInformationFile(pDeviceObject,
				pFileObject,
				NULL,
				&iosb,
				(PVOID)pFileBasicInfo,
				sizeof(FILE_BASIC_INFORMATION),
				FileBasicInformation);
			if (!NT_SUCCESS(ntStatus)) {
				break;
			}
		}
		ExFreePool((PVOID)pFileBasicInfo);
		pFileBasicInfo = NULL;

		//分配删除操作缓冲区
		pFileDispInfo = (PFILE_DISPOSITION_INFORMATION)ExAllocatePool(PagedPool,
			sizeof(FILE_DISPOSITION_INFORMATION));
		if (pFileDispInfo == NULL) {
			ntStatus = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}
		pFileDispInfo->DeleteFile = TRUE; //关闭时删除

		//检查并备份文件对象的SectionObjectPointer所指的内容,然后各域置空
		if (pFileObject->SectionObjectPointer != NULL) {
			stSectionObjPointers.DataSectionObject = pFileObject->SectionObjectPointer->DataSectionObject;
			stSectionObjPointers.SharedCacheMap = pFileObject->SectionObjectPointer->SharedCacheMap;
			stSectionObjPointers.ImageSectionObject = pFileObject->SectionObjectPointer->ImageSectionObject;

			pFileObject->SectionObjectPointer->DataSectionObject = NULL;
			pFileObject->SectionObjectPointer->SharedCacheMap = NULL;
			pFileObject->SectionObjectPointer->ImageSectionObject = NULL;
		}

		//下发IRP_MJ_SET_INFORMATION请求,设置关闭时删除
		ntStatus = IrpSetInformationFile(pDeviceObject,
			pFileObject,
			NULL,
			&iosb,
			(PVOID)pFileDispInfo,
			sizeof(FILE_DISPOSITION_INFORMATION),
			FileDispositionInformation);

		//检查并恢复文件对象的SectionObjectPointer所指的内容
		if (pFileObject->SectionObjectPointer != NULL) {
			pFileObject->SectionObjectPointer->DataSectionObject = stSectionObjPointers.DataSectionObject;
			pFileObject->SectionObjectPointer->SharedCacheMap = stSectionObjPointers.SharedCacheMap;
			pFileObject->SectionObjectPointer->ImageSectionObject = stSectionObjPointers.ImageSectionObject;
		}
	} while (FALSE);

	//检查并释放缓冲区
	if (pFileDispInfo != NULL) {
		ExFreePool((PVOID)pFileDispInfo);
	}
	if (pFileBasicInfo != NULL) {
		ExFreePool((PVOID)pFileBasicInfo);
	}
	return ntStatus;
}

NTSTATUS FSCtrlIrp::RootkitDeleteFileForIoctl(IN LPCWSTR wFilePath)
{
	PDEVICE_OBJECT pDeviceObject = NULL;
	PFILE_OBJECT pFileObject = NULL;
	NTSTATUS ntStatus = STATUS_NOT_SUPPORTED, ntLocStatus;
	IO_STATUS_BLOCK iosb;
	UNICODE_STRING unsFilePath;

	PAGED_CODE();
	ASSERT(wFilePath != NULL);

	//检查文件路径
	RtlInitUnicodeString(&unsFilePath,
		wFilePath);
	if (unsFilePath.Length < 4 * sizeof(WCHAR)) {
		return STATUS_INVALID_PARAMETER;
	}

	//判断是否以"\??\"打头
	if (wFilePath[0] == L'\\'
		&& wFilePath[1] == L'?'
		&& wFilePath[2] == L'?'
		&& wFilePath[3] == L'\\') {
		//以"\??\"打头,传入IrpCreateFile前需要去掉首部"\??\"
		unsFilePath.Buffer += 4;
		unsFilePath.Length -= 4 * sizeof(WCHAR);
		unsFilePath.MaximumLength -= 4 * sizeof(WCHAR);
	}

	//自己发送IRP_MJ_CREATE请求打开文件
	ntStatus = IrpCreateFile(&pFileObject,
		&pDeviceObject,
		FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES | DELETE | SYNCHRONIZE,
		&unsFilePath,
		&iosb,
		NULL,
		FILE_ATTRIBUTE_NORMAL,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		FILE_OPEN,
		FILE_SYNCHRONOUS_IO_NONALERT,
		NULL,
		0);
	if (NT_SUCCESS(ntStatus)) {
		//强删文件
		ntStatus = RootkitDeleteFile(pDeviceObject,
			pFileObject);

		//自己发送请求IRP_MJ_CLEANUP/IRP_MJ_CLOSE关闭文件
		ntLocStatus = IrpCleanupFile(pDeviceObject,
			pFileObject);
		ASSERT(NT_SUCCESS(ntLocStatus));

		if (NT_SUCCESS(ntLocStatus)) {
			ntLocStatus = IrpCloseFile(pDeviceObject,
				pFileObject);
			ASSERT(NT_SUCCESS(ntLocStatus));
		}
	}

	return ntStatus;
}

NTSTATUS FSCtrlIrp::RootkitRenameFile(IN PDEVICE_OBJECT pDeviceObject, /*如果指定为pFileObject->Vpb->DeviceObject,可绕过文件系统过滤驱动 */ IN PFILE_OBJECT pFileObject, IN LPCWSTR wNewFileNameOrPath, IN BOOLEAN bReplaceIfExists)
{
	LPWSTR lpwNewFileNameOrPath = NULL;
	PFILE_OBJECT pRootFileObject = NULL, pTargetFileObject = NULL;
	PDEVICE_OBJECT pTargetDeviceObject = NULL;
	PFILE_RENAME_INFORMATION pFileRenameInfo = NULL;
	HANDLE hRoot = NULL, hTargetFile = NULL;
	SIZE_T nRenameInfoLen;
	NTSTATUS ntStatus = STATUS_NOT_SUPPORTED;
	UNICODE_STRING unsNewFileNameOrPath, unsRootPath;
	OBJECT_ATTRIBUTES objAttrs;
	IO_STATUS_BLOCK iosb;
	BOOLEAN bNeedPrefix = TRUE;

	PAGED_CODE();
	ASSERT(pDeviceObject != NULL && pFileObject != NULL && wNewFileNameOrPath != NULL);

	do {
		//检查文件对象卷参数块
		if (pFileObject->Vpb == NULL || pFileObject->Vpb->DeviceObject == NULL) {
			ntStatus = STATUS_INVALID_PARAMETER;
			break;
		}

		//检查更名后的文件名或路径
		RtlInitUnicodeString(&unsNewFileNameOrPath,
			wNewFileNameOrPath);
		if (unsNewFileNameOrPath.Length < sizeof(WCHAR)) {
			ntStatus = STATUS_INVALID_PARAMETER;
			break;
		}

		//检查wNewFileNameOrPath是否只包含文件名,如果是文件路径，则表示: 把文件移动到相同卷的不同的目录下,比如：移到回收站是最常见的操作
		if (wcsrchr(wNewFileNameOrPath, L'\\') != NULL) {
			//这是文件路径，文件移动到相同卷的不同的目录下,因此需要创建移动后的文件对象: pTargetFileObject
			//检查是否以"\??\"打头
			if (unsNewFileNameOrPath.Length >= 4 * sizeof(WCHAR)) {
				if (wNewFileNameOrPath[0] == L'\\'
					&& wNewFileNameOrPath[1] == L'?'
					&& wNewFileNameOrPath[2] == L'?'
					&& wNewFileNameOrPath[3] == L'\\') {
					bNeedPrefix = FALSE;
				}
			}

			if (bNeedPrefix) {
				//不以"\??\"打头,需要分配缓冲区并以"\??\"打头
				lpwNewFileNameOrPath = (LPWSTR)ExAllocatePool(PagedPool,
					(MAX_PATH_ + 4) * sizeof(WCHAR));
				if (lpwNewFileNameOrPath == NULL) {
					ntStatus = STATUS_INSUFFICIENT_RESOURCES;
					break;
				}
				RtlZeroMemory(lpwNewFileNameOrPath,
					(MAX_PATH_ + 4) * sizeof(WCHAR));
				RtlInitEmptyUnicodeString(&unsNewFileNameOrPath,
					lpwNewFileNameOrPath,
					(MAX_PATH_ + 4) * sizeof(WCHAR));

				ntStatus = RtlAppendUnicodeToString(&unsNewFileNameOrPath,
					L"\\??\\");
				ASSERT(NT_SUCCESS(ntStatus));

				ntStatus = RtlAppendUnicodeToString(&unsNewFileNameOrPath,
					wNewFileNameOrPath);
				if (!NT_SUCCESS(ntStatus)) {
					break;
				}
			}
			else {
				lpwNewFileNameOrPath = (LPWSTR)wNewFileNameOrPath;
			}
			ASSERT(unsNewFileNameOrPath.Length >= 4 * sizeof(WCHAR));
			//"\??\C:\"
			if (unsNewFileNameOrPath.Length < 7 * sizeof(WCHAR)) {
				ntStatus = STATUS_INVALID_PARAMETER;
				break;
			}

			//打开根目录
			unsRootPath.Buffer = lpwNewFileNameOrPath;
			unsRootPath.MaximumLength = unsRootPath.Length = 7 * sizeof(WCHAR); //"\??\C:\"
			InitializeObjectAttributes(&objAttrs,
				&unsRootPath,
				OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
				NULL,
				NULL);
			ntStatus = IoCreateFile(&hRoot,
				FILE_READ_ATTRIBUTES | SYNCHRONIZE,
				&objAttrs,
				&iosb,
				NULL,
				FILE_ATTRIBUTE_NORMAL,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
				FILE_OPEN,
				FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
				NULL,
				0,
				CreateFileTypeNone,
				NULL,
				IO_NO_PARAMETER_CHECKING);
			if (!NT_SUCCESS(ntStatus)) {
				break;
			}

			//得到根目录文件对象
			ntStatus = ObReferenceObjectByHandle(hRoot,
				FILE_READ_ATTRIBUTES,
				*IoFileObjectType,
				KernelMode,
				(PVOID*)&pRootFileObject,
				NULL);
			if (!NT_SUCCESS(ntStatus)) {
				break;
			}
			if (pRootFileObject->Vpb == NULL || pRootFileObject->Vpb->DeviceObject == NULL) {
				ntStatus = STATUS_INVALID_PARAMETER;
				break;
			}

			//卷校验: 移动前的文件与移动后的文件所在的卷必须一致
			if (pFileObject->Vpb->DeviceObject != pRootFileObject->Vpb->DeviceObject) {
				ntStatus = STATUS_INVALID_PARAMETER;
				break;
			}
			pTargetDeviceObject = pRootFileObject->Vpb->DeviceObject; //卷设备
			ObDereferenceObject((PVOID)pRootFileObject);
			pRootFileObject = NULL;
			ZwClose(hRoot);
			hRoot = NULL;

			InitializeObjectAttributes(&objAttrs,
				&unsNewFileNameOrPath,
				OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
				NULL,
				NULL);

			//绕过过滤层，打开移动后的文件从而创建移动后的文件对象
			//由于需要绕过文件系统过滤驱动,这里直接向最底层的卷设备pTargetDeviceObject发送请求
			//除了IoCreateFileEx外， 用: IoCreateFileSpecifyDeviceObjectHint最后一个参数指定为pTargetDeviceObject也能达到这一目的
			//这与CreateFileEx中的: DriverContext->DeviceObjectHint指定为设备栈最底层的卷设备pTargetDeviceObject的作用是一样的
			/*
				NtSetInformationFile => IopOpenLinkOrRenameTarget
				...
				PAGE:00000001405C04E7                 lea     rcx, [rbp+90h+Dst]
				PAGE:00000001405C04EB                 mov     [rsp+158h+Dst], rcx ; DriverContext: &driverContext
				PAGE:00000001405C04F8                 lea     rcx, [rbp+90h+Handle] ; &FileHandle
				PAGE:00000001405C04FC                 mov     edx, esi        ; DesiredAccess: 目录为FILE_ADD_SUBDIRECTORY,否则FILE_WRITE_DATA
				PAGE:00000001405C04FE                 mov     al, [r13-46h]   ; (pIrp->CurrentStackLocation-1)->Flags
				PAGE:00000001405C0502                 bts     edx, 14h        ; 权限: DisiredAccess | SYNCHRONIZE
				PAGE:00000001405C0506                 not     al
				PAGE:00000001405C0508                 and     eax, 1
				PAGE:00000001405C050B                 or      eax, 104h       ; 如果: 下层栈空间中的: (pIrp->CurrentStackLocation-1)->Flags不含标志位: SL_KEY_SPECIFIED(0x1),则options需要添加: IO_FORCE_ACCESS_CHECK(0x0001)
				PAGE:00000001405C0510                 mov     dword ptr [rsp+158h+var_F0], eax ; Options: 0x104 or 0x105 <=>(IO_NO_PARAMETER_CHECKING|IO_OPEN_TARGET_DIRECTORY) or (IO_NO_PARAMETER_CHECKING|IO_OPEN_TARGET_DIRECTORY|IO_FORCE_ACCESS_CHECK)
				PAGE:00000001405C0514                 and     [rsp+158h+var_F8], r8 ; InternalParameters: NULL
				PAGE:00000001405C0519                 and     dword ptr [rsp+158h+var_100], r8d ; CreateFileType: CreateFileTypeNone
				PAGE:00000001405C051E                 and     dword ptr [rsp+158h+Handle], r8d ; EaLength: 0
				PAGE:00000001405C0523                 and     [rsp+158h+var_110], r8 ; EaBuffer: NULL
				PAGE:00000001405C0528                 mov     [rsp+158h+var_118], 4000h ; CreateOptions: 0x4000 <=> FILE_OPEN_FOR_BACKUP_INTENT
				PAGE:00000001405C0530                 mov     [rsp+158h+var_120], 1 ; Disposition: FILE_OPEN
				PAGE:00000001405C0538                 mov     [rsp+158h+var_128], 3 ; ShareAccess: FILE_SHARE_READ|FILE_SHARE_WRITE
				PAGE:00000001405C0540                 and     dword ptr [rsp+158h+HandleInformation], r8d ; FileAttributes: 0
				PAGE:00000001405C0545                 and     [rsp+158h+Object], r8 ; AllocationSize: NULL
				PAGE:00000001405C054A                 lea     r8, [rbp+90h+var_B8] ; ObjectAttributes
				PAGE:00000001405C054E                 call    IoCreateFileEx
				...
			*/
			//#define FILE_APPEND_DATA          ( 0x0004 )    // file
			//#define FILE_ADD_SUBDIRECTORY     ( 0x0004 )    // directory
			//根据逆向分析的内容进行填写
			ntStatus = IoCreateFileSpecifyDeviceObjectHint(&hTargetFile,
				FILE_WRITE_DATA | FILE_APPEND_DATA | SYNCHRONIZE, //also corret
				&objAttrs,
				&iosb,
				NULL,
				0,
				FILE_SHARE_READ | FILE_SHARE_WRITE, //0x3
				FILE_OPEN, //0x1
				FILE_OPEN_FOR_BACKUP_INTENT, //0x4000
				NULL,
				0,
				CreateFileTypeNone,
				NULL,
				IO_NO_PARAMETER_CHECKING | IO_OPEN_TARGET_DIRECTORY, //0x104
				pTargetDeviceObject);
			if (!NT_SUCCESS(ntStatus)) {
				break;
			}

			//得到移动后的文件对象
			ntStatus = ObReferenceObjectByHandle(hTargetFile,
				FILE_WRITE_DATA | FILE_APPEND_DATA,
				*IoFileObjectType,
				KernelMode,
				(PVOID*)&pTargetFileObject,
				NULL);
			if (!NT_SUCCESS(ntStatus)) {
				break;
			}
		}//end if (wcsrchr(wNewFileNameOrPath, L'\\') != NULL)

		//分配文件更名缓冲区并进行填写
		nRenameInfoLen = sizeof(FILE_RENAME_INFORMATION) + (SIZE_T)unsNewFileNameOrPath.Length;
		pFileRenameInfo = (PFILE_RENAME_INFORMATION)ExAllocatePool(PagedPool,
			nRenameInfoLen);
		if (pFileRenameInfo == NULL) {
			ntStatus = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}
		RtlZeroMemory(pFileRenameInfo,
			nRenameInfoLen);
		pFileRenameInfo->ReplaceIfExists = bReplaceIfExists;
		pFileRenameInfo->RootDirectory = NULL;
		pFileRenameInfo->FileNameLength = (ULONG)unsNewFileNameOrPath.Length;
		RtlCopyMemory(&pFileRenameInfo->FileName[0],
			unsNewFileNameOrPath.Buffer,
			unsNewFileNameOrPath.Length);

		//自己发送IRP请求实现文件更名
		ntStatus = IrpSetInformationFile(pDeviceObject,
			pFileObject,
			pTargetFileObject, //如果不移动文件则为NULL，否则为先前创建的移动后的文件对象
			&iosb,
			(PVOID)pFileRenameInfo,
			(ULONG)nRenameInfoLen,
			FileRenameInformation);
	} while (FALSE);

	//检查并解引文件对象，关闭句柄
	if (pRootFileObject != NULL) {
		ObDereferenceObject((PVOID)pRootFileObject);
	}
	if (hRoot != NULL) {
		ZwClose(hRoot);
	}
	if (pTargetFileObject != NULL) {
		ObDereferenceObject((PVOID)pTargetFileObject);
	}
	if (hTargetFile != NULL) {
		ZwClose(hTargetFile);
	}

	//检查并释放缓冲区
	if (pFileRenameInfo != NULL) {
		ExFreePool((PVOID)pFileRenameInfo);
	}
	if (lpwNewFileNameOrPath != NULL && lpwNewFileNameOrPath != (LPWSTR)wNewFileNameOrPath) {
		ExFreePool((PVOID)lpwNewFileNameOrPath);
	}

	return ntStatus;
}

NTSTATUS FSCtrlIrp::RootkitRenameFileForIoctl(LPCWSTR wSrcPath, LPCWSTR wDestPath, BOOLEAN bReplaceIfExists)
{
	PFILE_OBJECT pFileObject = NULL;
	PDEVICE_OBJECT pDeviceObject = NULL;
	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL, ntLocStatus;
	UNICODE_STRING unsOldFilePath;
	IO_STATUS_BLOCK iosb;

	PAGED_CODE();

	do {
		//检查更名前的文件路径
		RtlInitUnicodeString(&unsOldFilePath,
			wSrcPath);
		if (unsOldFilePath.Length < 3 * sizeof(WCHAR)) {
			ntStatus = STATUS_INVALID_PARAMETER;
			break;
		}

		//检查路径是否以"\??\"打头
		if (unsOldFilePath.Length >= 4 * sizeof(WCHAR)) {
			if (unsOldFilePath.Buffer[0] == L'\\'
				&& unsOldFilePath.Buffer[1] == L'?'
				&& unsOldFilePath.Buffer[2] == L'?'
				&& unsOldFilePath.Buffer[3] == L'\\') {
				//如果是以"\??\"打头，则传入IrpCreateFile前需要去掉头部"\??\"
				unsOldFilePath.Buffer += 4;
				unsOldFilePath.Length -= 4 * sizeof(WCHAR);
				unsOldFilePath.MaximumLength -= 4 * sizeof(WCHAR);
			}
		}

		//自己发送IRP请求打开更名前的文件
		ntStatus = IrpCreateFile(&pFileObject,
			&pDeviceObject,
			DELETE | SYNCHRONIZE,
			&unsOldFilePath, //必须以"盘符:\"开头, 比如: "C:\..."
			&iosb,
			NULL,
			FILE_ATTRIBUTE_NORMAL,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			FILE_OPEN,
			FILE_SYNCHRONOUS_IO_NONALERT,
			NULL,
			0);
	} while (FALSE);

	if (NT_SUCCESS(ntStatus)) {
		//自己发送IRP请求实现文件更名
		ntStatus = RootkitRenameFile(pDeviceObject,
			pFileObject,
			wDestPath,
			bReplaceIfExists);

		//自己发送IRP请求： IRP_MJ_CLEANUP，IRP_MJ_CLOSE 关闭文件
		ntLocStatus = IrpCleanupFile(pDeviceObject,
			pFileObject);
		ASSERT(NT_SUCCESS(ntLocStatus));

		if (NT_SUCCESS(ntLocStatus)) {
			IrpCloseFile(pDeviceObject,
				pFileObject);
			ASSERT(NT_SUCCESS(ntLocStatus));
		}
	}

	return ntStatus;
}

NTSTATUS FSCtrlIrp::RootkitCopyFile(IN PDEVICE_OBJECT pSrcDeviceObject, IN PFILE_OBJECT pSrcFileObject, IN PDEVICE_OBJECT pDestDeviceObject, IN PFILE_OBJECT pDestFileObject, IN BOOLEAN bDeleteSrcFile)
{
	PVOID lpTransBuf;
	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
	IO_STATUS_BLOCK iosb;
	LARGE_INTEGER liBytesOffset;

	PAGED_CODE();

	//分配文件读写缓冲区
	lpTransBuf = ExAllocatePool(NonPagedPool,
		FILE_TRANS_BUF_MAXLEN);
	if (lpTransBuf == NULL) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	liBytesOffset.QuadPart = 0;
	do {
		//自己发送IRP请求读取源文件
		ntStatus = IrpReadOrWriteFile(pSrcDeviceObject,
			pSrcFileObject,
			&iosb,
			lpTransBuf,
			FILE_TRANS_BUF_MAXLEN,
			&liBytesOffset,
			TRUE);
		if (ntStatus == STATUS_END_OF_FILE) {
			//已到达文件尾
			ntStatus = STATUS_SUCCESS;
			break;
		}
		if (!NT_SUCCESS(ntStatus)) {
			break;
		}

		//自己发送IRP请求写目标文件
		ntStatus = IrpReadOrWriteFile(pDestDeviceObject,
			pDestFileObject,
			&iosb,
			lpTransBuf,
			(ULONG)iosb.Information,
			&liBytesOffset,
			FALSE);
		if (!NT_SUCCESS(ntStatus)) {
			break;
		}

		//调整读写偏移量
		liBytesOffset.QuadPart += (LONGLONG)iosb.Information;
	} while (NT_SUCCESS(ntStatus));

	//释放文件读写缓冲区
	ExFreePool(lpTransBuf);

	if (NT_SUCCESS(ntStatus)) {
		if (bDeleteSrcFile) {
			//拷贝成功后需要删除源文件
			RootkitDeleteFile(pSrcDeviceObject,
				pSrcFileObject);
		}
	}

	return ntStatus;
}

NTSTATUS FSCtrlIrp::RootkitCopyFileForIoctl(LPCWSTR wSrcPath, LPCWSTR wDestPath, BOOLEAN bDeleteSrcFile)
{
	PDEVICE_OBJECT pSrcDeviceObject = NULL, pDestDeviceObject = NULL;
	PFILE_OBJECT pSrcFileObject = NULL, pDestFileObject = NULL;
	ACCESS_MASK ulDesiredAccess;
	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL, ntLocStatus;
	UNICODE_STRING unsFilePath;
	IO_STATUS_BLOCK iosb;

	PAGED_CODE();

	do {
		//检查源文件路径是否以"\??\"打头
		RtlInitUnicodeString(&unsFilePath,
			wSrcPath);
		if (unsFilePath.Length >= 4 * sizeof(WCHAR)) {
			if (unsFilePath.Buffer[0] == L'\\'
				&& unsFilePath.Buffer[1] == L'?'
				&& unsFilePath.Buffer[2] == L'?'
				&& unsFilePath.Buffer[3] == L'\\') {
				//如果以"\??\"打头,传入IrpCreateFile前需要去掉头部"\??\"
				unsFilePath.Buffer += 4;
				unsFilePath.Length -= 4 * sizeof(WCHAR);
				unsFilePath.MaximumLength -= 4 * sizeof(WCHAR);
			}
		}

		//自己发送IRP请求打开源文件
		ulDesiredAccess = FILE_READ_DATA | SYNCHRONIZE;
		if (bDeleteSrcFile) ulDesiredAccess |= DELETE;
		ntStatus = IrpCreateFile(&pSrcFileObject,
			&pSrcDeviceObject,
			ulDesiredAccess,
			&unsFilePath,
			&iosb,
			NULL,
			FILE_ATTRIBUTE_NORMAL,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			FILE_OPEN,
			FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
			NULL,
			0);
		if (!NT_SUCCESS(ntStatus)) {
			break;
		}

		//检查目标文件路径是否以"\??\"打头
		RtlInitUnicodeString(&unsFilePath,
			wDestPath);
		if (unsFilePath.Length >= 4 * sizeof(WCHAR)) {
			if (unsFilePath.Buffer[0] == L'\\'
				&& unsFilePath.Buffer[1] == L'?'
				&& unsFilePath.Buffer[2] == L'?'
				&& unsFilePath.Buffer[3] == L'\\') {
				//如果以"\??\"打头,传入IrpCreateFile前需要去掉头部"\??\"
				unsFilePath.Buffer += 4;
				unsFilePath.Length -= 4 * sizeof(WCHAR);
				unsFilePath.MaximumLength -= 4 * sizeof(WCHAR);
			}
		}

		//自己发送IRP请求打开或创建目标文件
		ntStatus = IrpCreateFile(&pDestFileObject,
			&pDestDeviceObject,
			FILE_WRITE_DATA | SYNCHRONIZE,
			&unsFilePath,
			&iosb,
			NULL,
			FILE_ATTRIBUTE_ARCHIVE,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			FILE_OVERWRITE_IF,
			FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
			NULL,
			0);
		if (!NT_SUCCESS(ntStatus)) {
			break;
		}

		//拷贝文件
		ntStatus = RootkitCopyFile(pSrcDeviceObject,
			pSrcFileObject,
			pDestDeviceObject,
			pDestFileObject,
			bDeleteSrcFile);
	} while (FALSE);

	//自己发送IRP请求关闭目标文件
	if (pDestFileObject != NULL) {
		ASSERT(pDestDeviceObject != NULL);

		ntLocStatus = IrpCleanupFile(pDestDeviceObject,
			pDestFileObject);
		ASSERT(NT_SUCCESS(ntLocStatus));

		if (NT_SUCCESS(ntLocStatus)) {
			ntLocStatus = IrpCloseFile(pDestDeviceObject,
				pDestFileObject);
			ASSERT(NT_SUCCESS(ntLocStatus));
		}
	}

	//自己发送IRP请求关闭源文件
	if (pSrcFileObject != NULL) {
		ASSERT(pSrcDeviceObject != NULL);

		ntLocStatus = IrpCleanupFile(pSrcDeviceObject,
			pSrcFileObject);
		ASSERT(NT_SUCCESS(ntLocStatus));

		if (NT_SUCCESS(ntLocStatus)) {
			ntLocStatus = IrpCloseFile(pSrcDeviceObject,
				pSrcFileObject);
			ASSERT(NT_SUCCESS(ntLocStatus));
		}
	}

	return ntStatus;
}
