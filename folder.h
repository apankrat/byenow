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
#ifndef _ULTRA_FOLDER_H_
#define _ULTRA_FOLDER_H_

#include "libp/types.h"
#include "libp/api_error.h"
#include "libp/_scan_folder_nt.h"

//
struct folder;

typedef deque<folder *>   folder_deq;
typedef vector<folder *>  folder_vec;

//
struct fsi_item
{
	wstring   name;
	fsi_info  info;

	fsi_item();
	fsi_item(const wc_range & name, const fsi_info & info);
};

typedef vector<fsi_item> fsi_item_vec;

//
struct folder
{
	folder      * parent;
	fsi_item      self;

	folder_vec    folders;
	fsi_item_vec  files;

	uint32_t      items;

	//
	folder();
	~folder();

	wstring get_path() const;
	void census(folder_vec & vec);
	bool ready_for_delete() const;
};

#endif
