#include "FSTools.h"


NTSTATUS FSTools::dispatcher(MinorOrder subOrder, PCMD pCmd, PVOID outBuffer, ULONG outBufferSize, PHandleContext context, PULONG retLength)
{
	if (!context) return STATUS_UNSUCCESSFUL;
	NTSTATUS status = STATUS_SUCCESS;
	switch (subOrder)
	{
	case MinorOrder::FindFirstFile:
	{
		if (outBufferSize < sizeof(PFirstFileInfo)) return STATUS_BUFFER_TOO_SMALL;
		status = FindFirstFile(context, (LPCWSTR)pCmd->data, outBuffer, retLength);
		break;
	}
	case MinorOrder::FindNextFile:
	{
		if (outBufferSize < sizeof(PFileInfo)) return STATUS_BUFFER_TOO_SMALL;
		status = FindNextFile(context, pCmd->fd, outBuffer, retLength);
		break;
	}
	case MinorOrder::FindClose:
	{
		status = FindClose(context, pCmd->fd);
		break;
	}
	case MinorOrder::CopyFile:
	{
		status = CopyFile(pCmd->srcPath, pCmd->destPath, pCmd->bDeleteSrcFile);
		break;
	}
	case MinorOrder::DeleteFile:
	{
		status = DeleteFile(pCmd->srcPath);
		break;
	}
	case MinorOrder::RenameFile:
	{
		status = RenameFile(pCmd->srcPath, pCmd->destPath, pCmd->bReplaceIfExists);
		break;
	}
	default:
		break;
	}
	return status;
}

void FSTools::structTransfer(PFIND_FILE_OUTPUT in, PFileInfo out)
{
	out->create_time = (int64_t)in->CreationTime.QuadPart;
	out->last_access_time = (int64_t)in->LastAccessTime.QuadPart;
	out->last_write_time = (int64_t)in->LastWriteTime.QuadPart;
	out->change_time = (int64_t)in->ChangeTime.QuadPart;
	out->size = (int64_t)in->EndOfFile.QuadPart;
	out->allocation_size = (int64_t)in->AllocationSize.QuadPart;
	out->attr = (int32_t)in->ulFileAttributes;

	if (in->wFileName[0] == L'\0')
	{
		RtlCopyMemory(out->filename, in->wShortFileName, sizeof(in->wShortFileName));
	}
	else
	{
		RtlCopyMemory(out->filename, in->wFileName, sizeof(in->wFileName));
	}
}

int32_t FSTools::genFd()
{
	static int32_t fd = 1;
	InterlockedIncrement((PLONG)&fd);
	return fd;

}

NTSTATUS FSTools::FindFirstFile(PHandleContext context, LPCWSTR dirPath, PVOID outBuffer, PULONG retLength)
{
	PFirstFileInfo firstFileInfo = (PFirstFileInfo)outBuffer;
	FIND_FIRST_FILE_OUTPUT output = { 0 };

	NTSTATUS status = FSCtrlIrp::RootkitFindFirstFileForIoctl(dirPath, &output);
	
	if (NT_SUCCESS(status))
	{
		FD fd = { 0 };
		fd.pDevObj = (PDEVICE_OBJECT)output.stFileFileHandleInfo.pDeviceObject;
		fd.pFileObj = (PFILE_OBJECT)output.stFileFileHandleInfo.pFileObject;

		int32_t fdNum = genFd();
		context->vecFD[fdNum] = fd;
		firstFileInfo->fd = fdNum;

		structTransfer(&output.stFindFileItem, &firstFileInfo->file_info);

		*retLength = sizeof(PFirstFileInfo);
	}
	return status;
}

NTSTATUS FSTools::FindNextFile(PHandleContext context, int fd, PVOID outBuffer, PULONG retLength)
{
	PFileInfo fileInfo = (PFileInfo)outBuffer;
	NTSTATUS status = STATUS_SUCCESS;
	auto it = context->vecFD.find(fd);
	if (it != context->vecFD.end())
	{
		FD& fd = it->second;

		FIND_FILE_HANDLE_INFO fileHandleInfo = { 0 };
		fileHandleInfo.pDeviceObject = fd.pDevObj;
		fileHandleInfo.pFileObject = fd.pFileObj;
		FIND_FILE_OUTPUT output = { 0 };

		status = FSCtrlIrp::RootkitFindNextFileForIoctl(&fileHandleInfo, &output);
		if (NT_SUCCESS(status))
		{
			structTransfer(&output, fileInfo);
			*retLength = sizeof(PFileInfo);
		}
		return status;
	}
	return STATUS_UNSUCCESSFUL;
}

NTSTATUS FSTools::FindClose(PHandleContext context, int fd)
{
	NTSTATUS status = STATUS_SUCCESS;
	auto it = context->vecFD.find(fd);
	if (it != context->vecFD.end())
	{
		FD& fd = it->second;

		FIND_FILE_HANDLE_INFO fileHandleInfo = { 0 };
		fileHandleInfo.pDeviceObject = fd.pDevObj;
		fileHandleInfo.pFileObject = fd.pFileObj;

		status = FSCtrlIrp::RootkitFindCloseForIoctl(&fileHandleInfo);

		context->vecFD.erase(it);

		return status;
		
	}
	return STATUS_UNSUCCESSFUL;
}

NTSTATUS FSTools::CreateFile(PHandleContext context, LPCWSTR filepath, PVOID outBuffer, PULONG retLength)
{
	return STATUS_SUCCESS;
	/*PDEVICE_OBJECT pDeviceObject = NULL;
	PFILE_OBJECT pFileObject = NULL;
	IO_STATUS_BLOCK iosb;
	UNICODE_STRING ufilepath;

	RtlInitUnicodeString(&ufilepath, filepath);

	NTSTATUS status = FSCtrlIrp::IrpCreateFile(&pFileObject,
		&pDeviceObject,
		GENERIC_READ | GENERIC_WRITE,
		&ufilepath,
		&iosb,
		0,
		FILE_ATTRIBUTE_NORMAL,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		FILE_OPEN_IF,
		FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
		NULL,
		0);*/
}

NTSTATUS FSTools::DeleteFile(LPCWSTR dirPath)
{
	return  FSCtrlIrp::RootkitDeleteFileForIoctl(dirPath);
}

NTSTATUS FSTools::RenameFile(LPCWSTR srcPath, LPCWSTR destPath, bool bReplaceIfExists)
{
	return  FSCtrlIrp::RootkitRenameFileForIoctl(srcPath, destPath, bReplaceIfExists);
}

NTSTATUS FSTools::CopyFile(LPCWSTR srcPath, LPCWSTR destPath, bool bDeleteSrcFile)
{
	return  FSCtrlIrp::RootkitCopyFileForIoctl(srcPath, destPath, bDeleteSrcFile);
}

NTSTATUS FSTools::WriteFile(PHandleContext context, int fd, PVOID data, PVOID outBuffer, PULONG retLength)
{
	return STATUS_SUCCESS;
}

NTSTATUS FSTools::ReadFile(PHandleContext context, int fd, PVOID data, PVOID outBuffer, PULONG retLength)
{
	return STATUS_SUCCESS;
}

NTSTATUS FSTools::CloseFile(PHandleContext context, int fd, PVOID data, PVOID outBuffer, PULONG retLength)
{
	return STATUS_SUCCESS;
}
