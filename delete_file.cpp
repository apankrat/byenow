/*
 *	This file is a part of the source code of "byenow" program.
 *
 *	Copyright (c) 2020 Alexander Pankratov and IO Bureau SA.
 *	All rights reserved.
 *
 *	The source code is distributed under the terms of 2-clause 
 *	BSD license with the Commons Clause condition. See LICENSE
 *	file for details.
 */
#include "delete_file.h"

#include "libp/_elpify.h"
#include "libp/_system_api.h"
#include "libp/_ntstatus.h"

#define HSRO (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN)

//
static
bool delete_file_win32(const wstring & file, dword attrs, api_error_cb * err)
{
	if (elp->DeleteFile(file.c_str()))
		return true;

	if (GetLastError() == ERROR_FILE_NOT_FOUND)
		return true;

	__on_api_error("DeleteFile", file);
	return false;
}

static
bool delete_file_ntapi(const wstring & file, dword attrs, api_error_cb * err)
{
	UNICODE_STRING     name;
	OBJECT_ATTRIBUTES  attr;
	NTSTATUS           status;

	ntdll.RtlDosPathNameToNtPathName_U(file.c_str(), &name, NULL, NULL);
	InitializeObjectAttributes(&attr, &name, OBJ_CASE_INSENSITIVE, NULL, NULL);
 
	status = ntdll.NtDeleteFile(&attr);
	if (status == STATUS_SUCCESS)
		return true;

	if (status == 0xC0000034) // STATUS_OBJECT_NAME_NOT_FOUND
		return false;

	__on_api_error_ex("NtDeleteFile", status, file);
	return false;
}

bool delete_file(const wstring & file, dword attrs, bool ntapi, api_error_cb * err)
{
	if ( (attrs & HSRO) &&
	     ! elp->SetFileAttributes(file.c_str(), attrs & ~HSRO))
	{
		__on_api_error("SetFileAttributes", file);
	}

	return ntapi ? delete_file_ntapi(file, attrs, err)
	             : delete_file_win32(file, attrs, err);
}

//
bool delete_folder(const wstring & folder, dword attrs, api_error_cb * err)
{
	if ( (attrs & HSRO) &&
	     ! elp->SetFileAttributes(folder.c_str(), attrs & ~HSRO))
	{
		__on_api_error("SetFileAttributes", folder);
	}

	if (elp->RemoveDirectory(folder.c_str()))
		return true;

	if (GetLastError() == ERROR_FILE_NOT_FOUND ||
	    GetLastError() == ERROR_PATH_NOT_FOUND)
		return true;

	__on_api_error("RemoveDirectory", folder);
	return false;
}
