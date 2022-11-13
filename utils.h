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
#ifndef _NUKE_UTILS_H_
#define _NUKE_UTILS_H_

#include "libp/types.h"
#include "libp/api_error.h"
#include "libp/_windows.h"

//
#define __not_found(err)      ( ((err) == ERROR_FILE_NOT_FOUND) || ((err) == ERROR_PATH_NOT_FOUND) )
#define __not_empty(err)        ((err) == ERROR_DIR_NOT_EMPTY )

//
string format_count(uint64_t val, const char * unit);
string format_bytes(uint64_t bytes);
string format_usecs(uint64_t usecs);

bool get_error_desc(dword code, wstring & mesg);
string error_to_str(const api_error & e);

//
template <class T>
void append(std::vector<T> & dst, const std::vector<T> & src)
{
	if (src.empty())
		return;

	dst.insert(dst.end(), src.begin(), src.end());
}

#endif

