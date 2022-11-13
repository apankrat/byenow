/*
 *	This file is a part of the source code of "byenow" program.
 *
 *	Copyright (c) 2020- Alexander Pankratov and IO Bureau SA.
 *	All rights reserved.
 *
 *	The source code is distributed under the terms of 2-clause 
 *	BSD license with the Commons Clause condition. See LICENSE
 *	file for details.
 */
#ifndef _ULTRA_DELETE_FILE_H_
#define _ULTRA_DELETE_FILE_H_

#include "libp/_windows.h"
#include "libp/api_error.h"

bool delete_file(const wstring & file, dword attrs, bool ntapi, api_error_cb * err);

bool delete_folder(const wstring & folder, dword attrs, api_error_cb * err);

#endif
