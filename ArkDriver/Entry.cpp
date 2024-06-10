#include "Entry.h"
#include "typedef.h"
#include "ProcessTools.h"
#include "DriverTools.h"
#include "FSTools.h"

Offset gOffset;

#define DEVICE_NAME L"\\Device\\MyArkTools202403"
//r3 �� "\\\\.\\MyArkTools202403"
#define SYMBOLICLINK_NAME L"\\??\\MyArkTools202403"

//���ݾ�����ؾ����Ϣ
#define OPER_CMD CTL_CODE(FILE_DEVICE_UNKNOWN, 0x900, METHOD_IN_DIRECT, FILE_ANY_ACCESS)


UNICODE_STRING deviceName = { 0 };
UNICODE_STRING symbolicLinkName = { 0 };
PDEVICE_OBJECT pDeviceObj = NULL;

// ��������ж�غ���
extern "C" VOID UnloadDriver(
	_In_ PDRIVER_OBJECT DriverObject
)
{
	UNREFERENCED_PARAMETER(DriverObject);
	// ɾ���������� ɾ���豸
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

	// ��ȡ��ǰ��I/O��ջλ��
	stackLocation = IoGetCurrentIrpStackLocation(pIrp);

	// Ϊ�ļ������ķ����ڴ�
	fileContext = (PHandleContext)ExAllocatePool(NonPagedPool, sizeof(HandleContext));
	if (fileContext == NULL) {
		status = STATUS_INSUFFICIENT_RESOURCES;
	}
	else {
		// ��ʼ���ļ�������
		RtlZeroMemory(fileContext, sizeof(HandleContext));
		fileContext = new (fileContext) HandleContext(); // �ڷ�����ڴ��ϵ��ù��캯��
		// ����FileObject��FsContext
		stackLocation->FileObject->FsContext = fileContext;
	}

	// ���IRP
	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = 0;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	return status;
}

NTSTATUS IrpCloseProc(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
	//����
	KdPrint(("close device success!\n"));

	PIO_STACK_LOCATION stackLocation;
	
	// ��ȡ��ǰ��I/O��ջλ��
	stackLocation = IoGetCurrentIrpStackLocation(pIrp);

	void* fileContext = stackLocation->FileObject->FsContext;
	if (fileContext != NULL)
	{
		ExFreePool(fileContext);
		stackLocation->FileObject->FsContext = NULL;
	}

	//���÷���״̬
	pIrp->IoStatus.Status = STATUS_SUCCESS;	//getlasterror()�õ��ľ������ֵ
	pIrp->IoStatus.Information = 0;		//���ظ�3���������� û����0
	IoCompleteRequest(pIrp, IO_NO_INCREMENT); //���´���
	return STATUS_SUCCESS;
}

// IRP_MJ_DEVICE_CONTROL������ ����������Ring3����
NTSTATUS IrpDeviceControlProc(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
	NTSTATUS status = STATUS_UNSUCCESSFUL;
	
	ULONG handle;

	// ������ʱ������ֵ
	PIO_STACK_LOCATION pIrpStack = IoGetCurrentIrpStackLocation(pIrp);
	// ��ȡ������
	ULONG uIoControlCode = pIrpStack->Parameters.DeviceIoControl.IoControlCode;
	// ��ȡ���뻺����
	PVOID pInBuffer = pIrp->AssociatedIrp.SystemBuffer;
	// ���뻺��������
	ULONG InLength = pIrpStack->Parameters.DeviceIoControl.InputBufferLength;

	PHandleContext handleContext = (PHandleContext)pIrpStack->FileObject->FsContext;

	// �����ַ
	PVOID pOutBuffer = NULL;
	//KdBreakPoint();
	if (MmIsAddressValid(pIrp->MdlAddress))
	{
		pOutBuffer = MmGetSystemAddressForMdlSafe(pIrp->MdlAddress, NormalPagePriority);;
	}
	// �������������
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
	
	// ���÷���״̬
	pIrp->IoStatus.Status = status;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	return status;
	
}

// ����������ڵ�
extern "C" NTSTATUS DriverEntry(
	_In_ PDRIVER_OBJECT   pDriver,
	_In_ PUNICODE_STRING  RegistryPath
)
{
	UNREFERENCED_PARAMETER(RegistryPath);
	KdPrint(("DriverEntry called\n"));

	// �������������ж�غ���
	pDriver->DriverUnload = UnloadDriver;

	//��ʼ���豸����
	RtlInitUnicodeString(&deviceName, DEVICE_NAME);

	//�����豸
	NTSTATUS ntStatus = IoCreateDevice(pDriver, 0, &deviceName, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &pDeviceObj);
	if (ntStatus != STATUS_SUCCESS)
	{
		KdPrint(("�豸����ʧ��:%x\n", ntStatus));
		return ntStatus;
	}

	//������r3ͨ�ŷ�ʽ(��������ʽ��д),DO_DIRECT_IOΪֱ�Ӷ�д(ӳ��r3��ַ������ҳ��Ȼ����ס)
	pDriver->Flags |= DO_DIRECT_IO;

	//��ʼ��������������
	RtlInitUnicodeString(&symbolicLinkName, SYMBOLICLINK_NAME);

	//������������
	ntStatus = IoCreateSymbolicLink(&symbolicLinkName, &deviceName);
	if (ntStatus != STATUS_SUCCESS)
	{
		KdPrint(("�豸��������ʧ��:%x\n", ntStatus));
		IoDeleteDevice(pDeviceObj);
		return ntStatus;
	}

	//������ǲ����
	pDriver->MajorFunction[IRP_MJ_CREATE] = IrpCreateProc;
	pDriver->MajorFunction[IRP_MJ_CLOSE] = IrpCloseProc;
	pDriver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = IrpDeviceControlProc;

	KdPrint(("DriverEntry completed\n"));
	return STATUS_SUCCESS;
}