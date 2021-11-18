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
#include "folder.h"

//
fsi_item::fsi_item()
{
	info.clear();
}

fsi_item::fsi_item(const wc_range & _name, const fsi_info & _info)
{
	_name.to_str(name);
	info = _info;
}

//
folder::folder()
{
	parent = NULL;
	items = 0;
}

folder::~folder()
{
	for (auto & d : folders)
		delete d;
}

wstring folder::get_path() const
{
	const folder * d = this;
	wstring  path;

	for ( ; d->parent; d = d->parent)
		path = L'\\' + d->self.name + path;

	return d->self.name + path;
}

void folder::census(folder_vec & vec)
{
	for (auto & x : folders)
		x->census(vec);

	vec.push_back(this);
}

bool folder::ready_for_delete() const
{
	return (items == 0);
}
