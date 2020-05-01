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
#ifndef _ULTRA_MACHINE_H_
#define _ULTRA_MACHINE_H_

#include "folder.h"

//
struct ultra_mach_conf
{
	size_t  threads;
	size_t  scanner_buf_size;
	bool    deleter_ntapi;
	size_t  deleter_batch;

	ultra_mach_conf();
};

struct ultra_mach_info
{
	size_t    d_found, d_deleted;
	size_t    f_found, f_deleted;
	uint64_t  b_found, b_deleted;

	api_error_vec * scanner_err;
	api_error_vec * deleter_err;

	size_t  folders_togo;
	bool    done;

	ultra_mach_info();
};

//
struct ultra_mach_cb
{
	__interface(ultra_mach_cb);

	virtual bool on_ultra_mach_tick(const ultra_mach_info & info) = 0;
};

//
bool ultra_mach_scan(folder & root, const ultra_mach_conf & conf, ultra_mach_cb * cb);

bool ultra_mach_delete(folder & root, bool prescanned, const ultra_mach_conf & conf, ultra_mach_cb * cb);

#endif
