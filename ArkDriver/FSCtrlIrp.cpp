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

	//�û��������״̬
	RtlCopyMemory(
		pIrp->UserIosb,
		&pIrp->IoStatus,
		sizeof(IO_STATUS_BLOCK)
	);

	//��鲢�ͷ�MDL
	if (pIrp->MdlAddress != NULL) {
		FreeMdl(pIrp->MdlAddress);
		pIrp->MdlAddress = NULL;
	}

	//�ͷ�IRP
	IoFreeIrp(pIrp);

	//�����¼�
	KeSetEvent(pkEvent,
		IO_NO_INCREMENT,
		FALSE
	);

	return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS FSCtrlIrp::IrpCreateFile(OUT PFILE_OBJECT* ppFileObject, OUT PDEVICE_OBJECT* ppDeviceObject, /*����ɹ������ﱣ��:(*ppFileObject)->Vpb->DeviceObject */ IN ACCESS_MASK DesiredAccess, IN PUNICODE_STRING punsFilePath, /*������"�̷�:\"��ͷ, ����: "C:\..." */ OUT PIO_STATUS_BLOCK pIoStatusBlock, IN PLARGE_INTEGER AllocationSize OPTIONAL, IN ULONG FileAttributes, IN ULONG ShareAccess, IN ULONG Disposition, IN ULONG CreateOptions, IN PVOID EaBuffer OPTIONAL, IN ULONG EaLength)
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

	//·�����ȼ��(���: "C:\")
	if (punsFilePath->Length < 3 * sizeof(CHAR)) {
		return STATUS_INVALID_PARAMETER;
	}

	RtlZeroMemory(pIoStatusBlock, sizeof(IO_STATUS_BLOCK));

	//��Ŀ¼��������
	ntStatus = RtlStringCbPrintfW(
		wRootPath,
		sizeof(wRootPath),
		L"\\??\\%c:\\",
		punsFilePath->Buffer[0]);
	ASSERT(NT_SUCCESS(ntStatus));
	RtlInitUnicodeString(&unsRootPath, (PCWSTR)wRootPath);

	do {
		//�򿪸�Ŀ¼
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

		//�õ���Ŀ¼�ļ�����
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

		//�õ����豸
		pDeviceObject = pRootObject->Vpb->DeviceObject;  //�ļ�ϵͳ���豸
		pRealDeviceObject = pRootObject->Vpb->RealDevice; //����������ɵľ��豸(�����豸)
		ObDereferenceObject((PVOID)pRootObject);
		pRootObject = NULL;
		ZwClose(hRoot);
		hRoot = NULL;

		//�����ļ�����
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

		//��д�ļ���������
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

		//����AUX_ACCESS_DATA������
		pAuxData = (PAUX_ACCESS_DATA)ExAllocatePool(NonPagedPool,
			sizeof(AUX_ACCESS_DATA));
		if (pAuxData == NULL) {
			ntStatus = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		//���ð�ȫ״̬
		ntStatus = SeCreateAccessState(&AccessState,
			pAuxData,
			DesiredAccess,
			IoGetFileObjectGenericMapping());
		if (!NT_SUCCESS(ntStatus)) {
			break;
		}

		//����IRP
		pIrp = IoAllocateIrp(pDeviceObject->StackSize,
			FALSE);
		if (pIrp == NULL) {
			ntStatus = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		//��дIRP
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

		//�õ��²�ջ�ռ�
		pIrpSp = IoGetNextIrpStackLocation(pIrp);
		ASSERT(pIrpSp != NULL);

		//��д�²�ջ�ռ�
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

		//�����������: IoSetCompletionRoutine(pIrp,FileOperationCompletion,NULL,TRUE,TRUE,TRUE);
		pIrpSp->CompletionRoutine = FileOperationCompletion;
		pIrpSp->Context = NULL;
		pIrpSp->Control = SL_INVOKE_ON_SUCCESS | SL_INVOKE_ON_ERROR | SL_INVOKE_ON_CANCEL;

		//�·�IRP����
		ntStatus = IoCallDriver(pDeviceObject,
			pIrp);

		//�Ⱥ����
		(VOID)KeWaitForSingleObject(&kEvent,
			Executive,
			KernelMode,
			FALSE,
			NULL);

		//�õ����״̬
		*pIoStatusBlock = userIosb;
		ntStatus = pIoStatusBlock->Status;
	} while (FALSE);

	if (NT_SUCCESS(ntStatus)) {
		//�ɹ���Ĵ���
		ASSERT(pFileObject != NULL && pFileObject->DeviceObject != NULL);
		InterlockedIncrement(&pFileObject->DeviceObject->ReferenceCount);
		if (pFileObject->Vpb != NULL) {
			InterlockedIncrement((PLONG)&pFileObject->Vpb->ReferenceCount);
		}
		*ppFileObject = pFileObject;
		*ppDeviceObject = pDeviceObject;
	}
	else {
		//ʧ�ܺ�Ĵ���
		if (pFileObject != NULL) {
			pFileObject->DeviceObject = NULL;
			ObDereferenceObject(pFileObject);
		}
		pIoStatusBlock->Status = ntStatus;
	}

	//��鲢�ͷ������Դ
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

	//���������
	if (pFileObject->Vpb == NULL || pFileObject->Vpb->DeviceObject == NULL) {
		return STATUS_INVALID_PARAMETER;
	}

	//
	// IRP_MJ_CLEANUP
	//
	//����IRP
	pIrp = IoAllocateIrp(pDeviceObject->StackSize,
		FALSE);
	if (pIrp == NULL) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//��дIRP
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

	//�õ��²�ջ�ռ�
	pIrpSp = IoGetNextIrpStackLocation(pIrp);
	ASSERT(pIrpSp != NULL);

	//��д�²�ջ�ռ�
	pIrpSp->MajorFunction = IRP_MJ_CLEANUP;
	pIrpSp->FileObject = pFileObject;
	pIrpSp->DeviceObject = pDeviceObject;

	//�����������: IoSetCompletionRoutine(pIrp,FileOperationCompletion,NULL,TRUE,TRUE,TRUE);
	pIrpSp->CompletionRoutine = FileOperationCompletion;
	pIrpSp->Context = NULL;
	pIrpSp->Control = SL_INVOKE_ON_SUCCESS | SL_INVOKE_ON_ERROR | SL_INVOKE_ON_CANCEL;

	//�·�����
	ntStatus = IoCallDriver(pDeviceObject,
		pIrp);

	//�ȴ��������
	KeWaitForSingleObject(&kEvent,
		Executive,
		KernelMode,
		FALSE,
		NULL);

	//�õ����״̬
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

	//��鲢�����ļ���ȡ����־
	if (pFileObject->Vpb != NULL && !(pFileObject->Flags & FO_DIRECT_DEVICE_OPEN)) {
		InterlockedDecrement((volatile LONG*)&pFileObject->Vpb->ReferenceCount);
		pFileObject->Flags |= FO_FILE_OPEN_CANCELLED;
	}

	//����IRP
	pIrp = IoAllocateIrp(pDeviceObject->StackSize,
		FALSE);
	if (pIrp == NULL) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//��дIRP
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

	//�õ�IRP�²�ջ�ռ�
	pIrpSp = IoGetNextIrpStackLocation(pIrp);
	ASSERT(pIrpSp != NULL);

	//��дIRP�²�ջ�ռ�
	pIrpSp->MajorFunction = IRP_MJ_CLOSE;
	pIrpSp->FileObject = pFileObject;
	pIrpSp->DeviceObject = pDeviceObject;

	//�����������: IoSetCompletionRoutine(pIrp,FileOperationCompletion,NULL,TRUE,TRUE,TRUE);
	pIrpSp->CompletionRoutine = FileOperationCompletion;
	pIrpSp->Context = NULL;
	pIrpSp->Control = SL_INVOKE_ON_SUCCESS | SL_INVOKE_ON_ERROR | SL_INVOKE_ON_CANCEL;

	//�·�IRP����
	ntStatus = IoCallDriver(pDeviceObject,
		pIrp);

	//�ȴ��������
	KeWaitForSingleObject(&kEvent,
		Executive,
		KernelMode,
		FALSE,
		NULL);

	//�õ����״̬
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

	//����IRP
	pIrp = IoAllocateIrp(
		pDeviceObject->StackSize,
		FALSE);
	RtlZeroMemory(pIoStatusBlock,
		sizeof(IO_STATUS_BLOCK));
	if (pIrp == NULL) {
		pIoStatusBlock->Status = STATUS_INSUFFICIENT_RESOURCES;
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//��дIRP
	pIrp->UserBuffer = NULL;
	pIrp->MdlAddress = NULL;
	pIrp->AssociatedIrp.SystemBuffer = NULL;
	if (pDeviceObject->Flags & DO_BUFFERED_IO) {
		//�����д��ʽ
		pIrp->AssociatedIrp.SystemBuffer = pBuffer;
	}
	else if (pDeviceObject->Flags & DO_DIRECT_IO) {
		//ֱ�Ӷ�д��ʽ
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
		//�ǻ����ֱ�Ӷ�д��ʽ: ���ļ���д�����бȽϳ���
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

	//��ȡ�²�ջ�ռ�
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

	//�����������: IoSetCompletionRoutine(pIrp,FileOperationCompletion,NULL,TRUE,TRUE,TRUE);
	pIrpSp->CompletionRoutine = FileOperationCompletion;
	pIrpSp->Context = NULL;
	pIrpSp->Control = SL_INVOKE_ON_SUCCESS | SL_INVOKE_ON_ERROR | SL_INVOKE_ON_CANCEL;

	//�·�IRP����
	ntStatus = IoCallDriver(pDeviceObject,
		pIrp);

	//�Ⱥ�IRP���
	KeWaitForSingleObject((PVOID)&kEvent,
		Executive,
		KernelMode,
		FALSE,
		NULL);

	//�������״̬
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

	//����IRP
	pIrp = IoAllocateIrp(
		pDeviceObject->StackSize,
		FALSE);
	RtlZeroMemory(pIoStatusBlock,
		sizeof(IO_STATUS_BLOCK));
	if (pIrp == NULL) {
		pIoStatusBlock->Status = STATUS_INSUFFICIENT_RESOURCES;
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//��дIRP
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

	//�õ��²�ջ�ռ�
	pIrpSp = IoGetNextIrpStackLocation(pIrp);
	ASSERT(pIrpSp != NULL);

	//��дջ�ռ�
	pIrpSp->MajorFunction = IRP_MJ_SET_INFORMATION;
	pIrpSp->Parameters.SetFile.Length = ulLength;
	pIrpSp->Parameters.SetFile.FileInformationClass = FileInformationClass;
	pIrpSp->Parameters.SetFile.FileObject = pTargetFileObject;
	//�����ļ�������,����Ӳ������Ҫ����: ReplaceIfExists
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

	//�����������: IoSetCompletionRoutine(pIrp,FileOperationCompletion,NULL,TRUE,TRUE,TRUE);
	pIrpSp->CompletionRoutine = FileOperationCompletion;
	pIrpSp->Context = NULL;
	pIrpSp->Control = SL_INVOKE_ON_SUCCESS | SL_INVOKE_ON_ERROR | SL_INVOKE_ON_CANCEL;

	//�·�IRP����
	ntStatus = IoCallDriver(pDeviceObject,
		pIrp);

	//�Ⱥ�IRP���
	KeWaitForSingleObject((PVOID)&kEvent,
		Executive,
		KernelMode,
		FALSE,
		NULL);

	//�������״̬
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

	//����IRP
	pIrp = IoAllocateIrp(pDeviceObject->StackSize,
		FALSE);
	RtlZeroMemory(pIoStatusBlock,
		sizeof(IO_STATUS_BLOCK));
	if (pIrp == NULL) {
		pIoStatusBlock->Status = STATUS_INSUFFICIENT_RESOURCES;
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//��дIRP
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

	//��ȡ�²�ջ�ռ�
	pIrpSp = IoGetNextIrpStackLocation(pIrp);
	ASSERT(pIrpSp != NULL);

	//��дջ�ռ�
	pIrpSp->MajorFunction = IRP_MJ_QUERY_INFORMATION;
	pIrpSp->Parameters.QueryFile.FileInformationClass = FileInformationClass;
	pIrpSp->Parameters.QueryFile.Length = ulLength;
	pIrpSp->FileObject = pFileObject;
	pIrpSp->DeviceObject = pDeviceObject;

	//�����������: IoSetCompletionRoutine(pIrp,FileOperationCompletion,NULL,TRUE,TRUE,TRUE);
	pIrpSp->CompletionRoutine = FileOperationCompletion;
	pIrpSp->Context = NULL;
	pIrpSp->Control = SL_INVOKE_ON_SUCCESS | SL_INVOKE_ON_ERROR | SL_INVOKE_ON_CANCEL;

	//�·�IRP����
	ntStatus = IoCallDriver(
		pDeviceObject,
		pIrp);

	//�Ⱥ��������
	KeWaitForSingleObject(
		(PVOID)&kEvent,
		Executive,
		KernelMode,
		FALSE,
		NULL);

	//�õ����״̬
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

	//����IRP
	pIrp = IoAllocateIrp(pDeviceObject->StackSize,
		FALSE);
	RtlZeroMemory(pIoStatusBlock,
		sizeof(IO_STATUS_BLOCK));
	if (pIrp == NULL) {
		pIoStatusBlock->Status = STATUS_INSUFFICIENT_RESOURCES;
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//��дIRP
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

	//�õ��²�ջ�ռ�
	pIrpSp = IoGetNextIrpStackLocation(pIrp);
	ASSERT(pIrpSp != NULL);

	//��д�²�ջ�ռ�
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

	//�����������: IoSetCompletionRoutine(pIrp, FileOperationCompletion, NULL, TRUE, TRUE, TRUE);
	pIrpSp->CompletionRoutine = FileOperationCompletion;
	pIrpSp->Context = NULL;
	pIrpSp->Control = SL_INVOKE_ON_SUCCESS | SL_INVOKE_ON_ERROR | SL_INVOKE_ON_CANCEL;

	//�·�IO����
	ntStatus = IoCallDriver(pDeviceObject,
		pIrp);

	//�Ⱥ��������
	KeWaitForSingleObject((PVOID)&kEvent,
		Executive,
		KernelMode,
		FALSE,
		NULL);

	//�õ����״̬
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

	//���仺����
	pFileBothDirInfo = (PFILE_BOTH_DIR_INFORMATION)ExAllocatePool(PagedPool,
		PAGE_SIZE);
	if (pFileBothDirInfo == NULL) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	RtlZeroMemory(pFileBothDirInfo,
		PAGE_SIZE);

	//����IRP�����ѯĿ¼�µ�1��
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
		//�ɹ��˾ͽ��б���
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

	//�ͷŻ�����
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
		//���Ŀ¼·���Ƿ���"\??\"��ͷ
		RtlInitUnicodeString(&unsDirPath,
			wDirPath);
		if (unsDirPath.Length >= 4 * sizeof(WCHAR)) {
			if (unsDirPath.Buffer[0] == L'\\'
				&& unsDirPath.Buffer[1] == L'?'
				&& unsDirPath.Buffer[2] == L'?'
				&& unsDirPath.Buffer[3] == L'\\') {
				//�����"\??\"��ͷ������IrpCreateFileǰ��Ҫȥ��ͷ��"\??\"
				unsDirPath.Buffer += 4;
				unsDirPath.Length -= 4 * sizeof(WCHAR);
				unsDirPath.MaximumLength -= 4 * sizeof(WCHAR);
			}
		}

		//�Լ�����IRP����,��Ŀ��Ŀ¼
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

		//�Լ�����IRP����,��ѯĿ¼�µĵ�1��
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
		//ʧ��ʱ�Ĵ���
		if (pFileObject != NULL) {
			ASSERT(pDeviceObject != NULL);

			//�Լ�����IRP����رղ�ѯĿ¼
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

	//�������Ϸ���
	if (pFileFirstFileHandleInfo->pDeviceObject == NULL || pFileFirstFileHandleInfo->pFileObject == NULL) {
		return STATUS_INVALID_PARAMETER;
	}

	//�Լ�����IRP����,��ѯĿ¼�µ�1��
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
		//�Լ�����IRP����ر�Ŀ��Ŀ¼
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
		//�������Ի�����
		pFileBasicInfo = (PFILE_BASIC_INFORMATION)ExAllocatePool(PagedPool,
			sizeof(FILE_BASIC_INFORMATION));
		if (pFileBasicInfo == NULL) {
			ntStatus = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		//�·�IRP_MJ_QUERY_INFORMATION�����ѯ�ļ�����
		ntStatus = IrpQueryInformationFile(pDeviceObject,
			pFileObject,
			&iosb,
			(PVOID)pFileBasicInfo,
			sizeof(FILE_BASIC_INFORMATION),
			FileBasicInformation);
		if (!NT_SUCCESS(ntStatus)) {
			break;
		}

		//��鲢ȥ��ֻ������
		if (pFileBasicInfo->FileAttributes & FILE_ATTRIBUTE_READONLY) {
			pFileBasicInfo->FileAttributes &= ~FILE_ATTRIBUTE_READONLY;

			//�·�IRP_MJ_SET_INFORMATION����ȥ���ļ�ֻ������
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

		//����ɾ������������
		pFileDispInfo = (PFILE_DISPOSITION_INFORMATION)ExAllocatePool(PagedPool,
			sizeof(FILE_DISPOSITION_INFORMATION));
		if (pFileDispInfo == NULL) {
			ntStatus = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}
		pFileDispInfo->DeleteFile = TRUE; //�ر�ʱɾ��

		//��鲢�����ļ������SectionObjectPointer��ָ������,Ȼ������ÿ�
		if (pFileObject->SectionObjectPointer != NULL) {
			stSectionObjPointers.DataSectionObject = pFileObject->SectionObjectPointer->DataSectionObject;
			stSectionObjPointers.SharedCacheMap = pFileObject->SectionObjectPointer->SharedCacheMap;
			stSectionObjPointers.ImageSectionObject = pFileObject->SectionObjectPointer->ImageSectionObject;

			pFileObject->SectionObjectPointer->DataSectionObject = NULL;
			pFileObject->SectionObjectPointer->SharedCacheMap = NULL;
			pFileObject->SectionObjectPointer->ImageSectionObject = NULL;
		}

		//�·�IRP_MJ_SET_INFORMATION����,���ùر�ʱɾ��
		ntStatus = IrpSetInformationFile(pDeviceObject,
			pFileObject,
			NULL,
			&iosb,
			(PVOID)pFileDispInfo,
			sizeof(FILE_DISPOSITION_INFORMATION),
			FileDispositionInformation);

		//��鲢�ָ��ļ������SectionObjectPointer��ָ������
		if (pFileObject->SectionObjectPointer != NULL) {
			pFileObject->SectionObjectPointer->DataSectionObject = stSectionObjPointers.DataSectionObject;
			pFileObject->SectionObjectPointer->SharedCacheMap = stSectionObjPointers.SharedCacheMap;
			pFileObject->SectionObjectPointer->ImageSectionObject = stSectionObjPointers.ImageSectionObject;
		}
	} while (FALSE);

	//��鲢�ͷŻ�����
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

	//����ļ�·��
	RtlInitUnicodeString(&unsFilePath,
		wFilePath);
	if (unsFilePath.Length < 4 * sizeof(WCHAR)) {
		return STATUS_INVALID_PARAMETER;
	}

	//�ж��Ƿ���"\??\"��ͷ
	if (wFilePath[0] == L'\\'
		&& wFilePath[1] == L'?'
		&& wFilePath[2] == L'?'
		&& wFilePath[3] == L'\\') {
		//��"\??\"��ͷ,����IrpCreateFileǰ��Ҫȥ���ײ�"\??\"
		unsFilePath.Buffer += 4;
		unsFilePath.Length -= 4 * sizeof(WCHAR);
		unsFilePath.MaximumLength -= 4 * sizeof(WCHAR);
	}

	//�Լ�����IRP_MJ_CREATE������ļ�
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
		//ǿɾ�ļ�
		ntStatus = RootkitDeleteFile(pDeviceObject,
			pFileObject);

		//�Լ���������IRP_MJ_CLEANUP/IRP_MJ_CLOSE�ر��ļ�
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

NTSTATUS FSCtrlIrp::RootkitRenameFile(IN PDEVICE_OBJECT pDeviceObject, /*���ָ��ΪpFileObject->Vpb->DeviceObject,���ƹ��ļ�ϵͳ�������� */ IN PFILE_OBJECT pFileObject, IN LPCWSTR wNewFileNameOrPath, IN BOOLEAN bReplaceIfExists)
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
		//����ļ�����������
		if (pFileObject->Vpb == NULL || pFileObject->Vpb->DeviceObject == NULL) {
			ntStatus = STATUS_INVALID_PARAMETER;
			break;
		}

		//����������ļ�����·��
		RtlInitUnicodeString(&unsNewFileNameOrPath,
			wNewFileNameOrPath);
		if (unsNewFileNameOrPath.Length < sizeof(WCHAR)) {
			ntStatus = STATUS_INVALID_PARAMETER;
			break;
		}

		//���wNewFileNameOrPath�Ƿ�ֻ�����ļ���,������ļ�·�������ʾ: ���ļ��ƶ�����ͬ��Ĳ�ͬ��Ŀ¼��,���磺�Ƶ�����վ������Ĳ���
		if (wcsrchr(wNewFileNameOrPath, L'\\') != NULL) {
			//�����ļ�·�����ļ��ƶ�����ͬ��Ĳ�ͬ��Ŀ¼��,�����Ҫ�����ƶ�����ļ�����: pTargetFileObject
			//����Ƿ���"\??\"��ͷ
			if (unsNewFileNameOrPath.Length >= 4 * sizeof(WCHAR)) {
				if (wNewFileNameOrPath[0] == L'\\'
					&& wNewFileNameOrPath[1] == L'?'
					&& wNewFileNameOrPath[2] == L'?'
					&& wNewFileNameOrPath[3] == L'\\') {
					bNeedPrefix = FALSE;
				}
			}

			if (bNeedPrefix) {
				//����"\??\"��ͷ,��Ҫ���仺��������"\??\"��ͷ
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

			//�򿪸�Ŀ¼
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

			//�õ���Ŀ¼�ļ�����
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

			//��У��: �ƶ�ǰ���ļ����ƶ�����ļ����ڵľ����һ��
			if (pFileObject->Vpb->DeviceObject != pRootFileObject->Vpb->DeviceObject) {
				ntStatus = STATUS_INVALID_PARAMETER;
				break;
			}
			pTargetDeviceObject = pRootFileObject->Vpb->DeviceObject; //���豸
			ObDereferenceObject((PVOID)pRootFileObject);
			pRootFileObject = NULL;
			ZwClose(hRoot);
			hRoot = NULL;

			InitializeObjectAttributes(&objAttrs,
				&unsNewFileNameOrPath,
				OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
				NULL,
				NULL);

			//�ƹ����˲㣬���ƶ�����ļ��Ӷ������ƶ�����ļ�����
			//������Ҫ�ƹ��ļ�ϵͳ��������,����ֱ������ײ�ľ��豸pTargetDeviceObject��������
			//����IoCreateFileEx�⣬ ��: IoCreateFileSpecifyDeviceObjectHint���һ������ָ��ΪpTargetDeviceObjectҲ�ܴﵽ��һĿ��
			//����CreateFileEx�е�: DriverContext->DeviceObjectHintָ��Ϊ�豸ջ��ײ�ľ��豸pTargetDeviceObject��������һ����
			/*
				NtSetInformationFile => IopOpenLinkOrRenameTarget
				...
				PAGE:00000001405C04E7                 lea     rcx, [rbp+90h+Dst]
				PAGE:00000001405C04EB                 mov     [rsp+158h+Dst], rcx ; DriverContext: &driverContext
				PAGE:00000001405C04F8                 lea     rcx, [rbp+90h+Handle] ; &FileHandle
				PAGE:00000001405C04FC                 mov     edx, esi        ; DesiredAccess: Ŀ¼ΪFILE_ADD_SUBDIRECTORY,����FILE_WRITE_DATA
				PAGE:00000001405C04FE                 mov     al, [r13-46h]   ; (pIrp->CurrentStackLocation-1)->Flags
				PAGE:00000001405C0502                 bts     edx, 14h        ; Ȩ��: DisiredAccess | SYNCHRONIZE
				PAGE:00000001405C0506                 not     al
				PAGE:00000001405C0508                 and     eax, 1
				PAGE:00000001405C050B                 or      eax, 104h       ; ���: �²�ջ�ռ��е�: (pIrp->CurrentStackLocation-1)->Flags������־λ: SL_KEY_SPECIFIED(0x1),��options��Ҫ���: IO_FORCE_ACCESS_CHECK(0x0001)
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
			//����������������ݽ�����д
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

			//�õ��ƶ�����ļ�����
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

		//�����ļ�������������������д
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

		//�Լ�����IRP����ʵ���ļ�����
		ntStatus = IrpSetInformationFile(pDeviceObject,
			pFileObject,
			pTargetFileObject, //������ƶ��ļ���ΪNULL������Ϊ��ǰ�������ƶ�����ļ�����
			&iosb,
			(PVOID)pFileRenameInfo,
			(ULONG)nRenameInfoLen,
			FileRenameInformation);
	} while (FALSE);

	//��鲢�����ļ����󣬹رվ��
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

	//��鲢�ͷŻ�����
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
		//������ǰ���ļ�·��
		RtlInitUnicodeString(&unsOldFilePath,
			wSrcPath);
		if (unsOldFilePath.Length < 3 * sizeof(WCHAR)) {
			ntStatus = STATUS_INVALID_PARAMETER;
			break;
		}

		//���·���Ƿ���"\??\"��ͷ
		if (unsOldFilePath.Length >= 4 * sizeof(WCHAR)) {
			if (unsOldFilePath.Buffer[0] == L'\\'
				&& unsOldFilePath.Buffer[1] == L'?'
				&& unsOldFilePath.Buffer[2] == L'?'
				&& unsOldFilePath.Buffer[3] == L'\\') {
				//�������"\??\"��ͷ������IrpCreateFileǰ��Ҫȥ��ͷ��"\??\"
				unsOldFilePath.Buffer += 4;
				unsOldFilePath.Length -= 4 * sizeof(WCHAR);
				unsOldFilePath.MaximumLength -= 4 * sizeof(WCHAR);
			}
		}

		//�Լ�����IRP����򿪸���ǰ���ļ�
		ntStatus = IrpCreateFile(&pFileObject,
			&pDeviceObject,
			DELETE | SYNCHRONIZE,
			&unsOldFilePath, //������"�̷�:\"��ͷ, ����: "C:\..."
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
		//�Լ�����IRP����ʵ���ļ�����
		ntStatus = RootkitRenameFile(pDeviceObject,
			pFileObject,
			wDestPath,
			bReplaceIfExists);

		//�Լ�����IRP���� IRP_MJ_CLEANUP��IRP_MJ_CLOSE �ر��ļ�
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

	//�����ļ���д������
	lpTransBuf = ExAllocatePool(NonPagedPool,
		FILE_TRANS_BUF_MAXLEN);
	if (lpTransBuf == NULL) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	liBytesOffset.QuadPart = 0;
	do {
		//�Լ�����IRP�����ȡԴ�ļ�
		ntStatus = IrpReadOrWriteFile(pSrcDeviceObject,
			pSrcFileObject,
			&iosb,
			lpTransBuf,
			FILE_TRANS_BUF_MAXLEN,
			&liBytesOffset,
			TRUE);
		if (ntStatus == STATUS_END_OF_FILE) {
			//�ѵ����ļ�β
			ntStatus = STATUS_SUCCESS;
			break;
		}
		if (!NT_SUCCESS(ntStatus)) {
			break;
		}

		//�Լ�����IRP����дĿ���ļ�
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

		//������дƫ����
		liBytesOffset.QuadPart += (LONGLONG)iosb.Information;
	} while (NT_SUCCESS(ntStatus));

	//�ͷ��ļ���д������
	ExFreePool(lpTransBuf);

	if (NT_SUCCESS(ntStatus)) {
		if (bDeleteSrcFile) {
			//�����ɹ�����Ҫɾ��Դ�ļ�
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
		//���Դ�ļ�·���Ƿ���"\??\"��ͷ
		RtlInitUnicodeString(&unsFilePath,
			wSrcPath);
		if (unsFilePath.Length >= 4 * sizeof(WCHAR)) {
			if (unsFilePath.Buffer[0] == L'\\'
				&& unsFilePath.Buffer[1] == L'?'
				&& unsFilePath.Buffer[2] == L'?'
				&& unsFilePath.Buffer[3] == L'\\') {
				//�����"\??\"��ͷ,����IrpCreateFileǰ��Ҫȥ��ͷ��"\??\"
				unsFilePath.Buffer += 4;
				unsFilePath.Length -= 4 * sizeof(WCHAR);
				unsFilePath.MaximumLength -= 4 * sizeof(WCHAR);
			}
		}

		//�Լ�����IRP�����Դ�ļ�
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

		//���Ŀ���ļ�·���Ƿ���"\??\"��ͷ
		RtlInitUnicodeString(&unsFilePath,
			wDestPath);
		if (unsFilePath.Length >= 4 * sizeof(WCHAR)) {
			if (unsFilePath.Buffer[0] == L'\\'
				&& unsFilePath.Buffer[1] == L'?'
				&& unsFilePath.Buffer[2] == L'?'
				&& unsFilePath.Buffer[3] == L'\\') {
				//�����"\??\"��ͷ,����IrpCreateFileǰ��Ҫȥ��ͷ��"\??\"
				unsFilePath.Buffer += 4;
				unsFilePath.Length -= 4 * sizeof(WCHAR);
				unsFilePath.MaximumLength -= 4 * sizeof(WCHAR);
			}
		}

		//�Լ�����IRP����򿪻򴴽�Ŀ���ļ�
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

		//�����ļ�
		ntStatus = RootkitCopyFile(pSrcDeviceObject,
			pSrcFileObject,
			pDestDeviceObject,
			pDestFileObject,
			bDeleteSrcFile);
	} while (FALSE);

	//�Լ�����IRP����ر�Ŀ���ļ�
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

	//�Լ�����IRP����ر�Դ�ļ�
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
