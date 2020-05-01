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
#include "ultra_machine.h"
#include "ultra_machine_internals.h"

#include "delete_file.h"

#include "libp/assert.h"
#include "libp/atomic.h"

#include "libp/_elpify.h"
#include "libp/_cpu_info.h"
#include "libp/_simple_work_queue.h"

//
ultra_mach_conf::ultra_mach_conf()
{
	threads = 0;
	scanner_buf_size = 0;
	deleter_ntapi = false;
	deleter_batch = 128;
}

//
ultra_mach_info::ultra_mach_info()
{
	d_found = d_deleted = 0;
	f_found = f_deleted = 0;
	b_found = b_deleted = 0;

	scanner_err = NULL;
	deleter_err = NULL;

	folders_togo = 0;
	done = false;
}

/*
 *	ultra_task
 */
ultra_task::ultra_task(ultra_mach * _mach)
{
	mach = _mach;
	phase = -1;
	ph2_first = 0;
	ph2_count = -1;
}

//
void ultra_task::execute()
{
	assert(curr && errors.empty());

	path = curr->get_path();

	if (phase == 1)
	{
		// scan folder
		scan_folder_nt(path, mach->conf.scanner_buf_size, this, this);
	}
	else
	if (phase == 2)
	{
		// delete files

		if (ph2_first == 0 && ph2_count == -1)
			ph2_count = curr->files.size();

		assert(ph2_first + ph2_count <= curr->files.size());

		for (size_t i = 0; i < ph2_count && ! mach->enough; i++)
		{
			do_delete_file( curr->files[ph2_first+i] );
		}
	}
	else
	if (phase == 3)
	{
		do_delete_self();
	}
	else
	{
		assert(false);
	}

	path.clear();
}

void ultra_task::do_delete_file(const fsi_item & f)
{
	wstring file = path + L'\\' + f.name;

	if (delete_file(file, f.info.attrs, mach->conf.deleter_ntapi, this))
	{
		atomic_inc(&mach->info.f_deleted);
		atomic_add(&mach->info.b_deleted, f.info.bytes);
	}

	atomic_dec(&curr->items);
}

void ultra_task::do_delete_self()
{
	if (delete_folder(path, curr->self.info.attrs, this))
		atomic_inc(&mach->info.d_deleted);

	if (curr->parent)
		atomic_dec(&curr->parent->items);
}

//
bool ultra_task::on_fsi_scan_f(const fsi_name & name, const fsi_info & info, const api_error & e)
{
	curr->files.push_back( fsi_item(name, info) );
	curr->items++;

	if (e.code) errors.push_back(e);

	atomic_inc(&mach->info.f_found);
	atomic_add(&mach->info.b_found, info.bytes);
	return true;
}

bool ultra_task::on_fsi_scan_d(const fsi_name & name, const fsi_info & info, const api_error & e)
{
	folder * sub;

	sub = new folder();
	sub->parent = curr;
	sub->self = fsi_item(name, info);

	curr->folders.push_back(sub);
	curr->items++;

	if (e.code) errors.push_back(e);

	atomic_inc(&mach->info.d_found);
	return true;
}

//
void ultra_task::on_api_error_x(const api_error & e)
{
	errors.push_back(e);
}

/*
 *	ultra_task_pool
 */
ultra_task_pool::ultra_task_pool()
{
	mach = NULL;
	allocated = 0;
}

ultra_task_pool::~ultra_task_pool()
{
	assert( unused() );
	for (auto & x : cache) delete x;
}

ultra_task * ultra_task_pool::get(folder * d, int phase)
{
	ultra_task * w;

	if (cache.size()) { w = cache.back(); cache.pop_back(); }
	else              { w = new ultra_task(mach); allocated++; }

	w->phase = phase;
	w->curr = d;
	return w;
}

void ultra_task_pool::put(ultra_task * w)
{
	w->curr = NULL;
	w->phase = -1;
	w->errors.clear();

	cache.push_back(w);
}

bool ultra_task_pool::unused() const
{
	return cache.size() == allocated;
}

/*
 *	ultra_mach
 */
ultra_mach::ultra_mach()
{
	ph1_only = false;
	enough = false;
	ph1_work = ph2_work = ph3_work = 0;
	ph1_done = ph2_done = ph3_done = 0;
}

ultra_mach::~ultra_mach()
{
	term();
}

//
bool ultra_mach::init(const ultra_mach_conf & _conf, ultra_mach_cb * _cb)
{
	conf = _conf;
	cb = _cb;

	if (! conf.scanner_buf_size)
		conf.scanner_buf_size = 8*1024;

	if (! conf.deleter_batch)
		conf.deleter_batch = -1;

	if (conf.threads == 0 || conf.threads == -1)
		conf.threads = get_cpu_count();

	pool.mach = this;

	return swq.init(conf.threads, NULL);
}

void ultra_mach::term()
{
	work_item_vec out;

	swq.cancel(out);

	for (auto & wi : out) 
		pool.put( (ultra_task*)wi );
}

//
bool ultra_mach::keep_going() const
{
	if (enough)
		return false;

	return (ph1_done < ph1_work) || (ph2_done < ph2_work) || (ph3_done < ph3_work);
}

//
void ultra_mach::enqueue_ph1(folder * x)
{
	swq.enqueue( pool.get(x, 1) );
	ph1_work++;
}

void ultra_mach::enqueue_ph2(folder * x)
{
	ultra_task * w;
	size_t total = x->files.size();

	for (size_t chunk, start = 0; start < total; start += chunk)
	{
		chunk = min(total - start, conf.deleter_batch);

		w = pool.get(x, 2);
		w->ph2_first = start;
		w->ph2_count = chunk;

		swq.enqueue(w);
		ph2_work++;
	}
}

void ultra_mach::enqueue_ph3(folder * x)
{
	assert(x->items == 0);

	x->items = -1; // being deleted

	swq.enqueue( pool.get(x, 3) );
	ph3_work++;
}

//
void ultra_mach::complete_ph1(ultra_task * w)
{
	assert(! enough);
	assert(w->phase == 1);

	// w->curr scanned

	ph1_done++;

	for (auto & x : w->curr->folders)
	{
		if (x->self.info.attrs & FILE_ATTRIBUTE_REPARSE_POINT)
			continue;

		enqueue_ph1(x); // scan subfolders
	}

	if (! ph1_only)
	{
		if (w->curr->files.size())
			enqueue_ph2(w->curr);
		else
		if (w->curr->folders.empty())
			enqueue_ph3(w->curr);

		// ^ same as in ultra_mach_delete()
	}

	//
	info.folders_togo = ph1_work - ph1_done;

	info.scanner_err = &w->errors;
	enough = ! cb->on_ultra_mach_tick(info);
	info.scanner_err = NULL;

	//
	pool.put(w);
}

void ultra_mach::complete_ph2(ultra_task * w)
{
	assert(! enough);
	assert(w->phase == 2);

	// w->curr->files deleted

	ph2_done++;

	// if fully processed
	if (w->curr->items == 0)
	{
		// save some space
		w->curr->files.clear(); 

		// delete the folder 
		enqueue_ph3(w->curr);
	}

	//
	info.deleter_err = &w->errors;
	enough = ! cb->on_ultra_mach_tick(info);
	info.deleter_err = NULL;

	pool.put(w);
}

void ultra_mach::complete_ph3(ultra_task * w)
{
	assert(! enough);
	assert(w->phase == 3);

	// w->curr deleted

	ph3_done++;

	// if parent is fully processed
	if (w->curr->parent && 
	    w->curr->parent->items == 0)
	{
		enqueue_ph3(w->curr->parent);
	}

	pool.put(w);
}

void ultra_mach::loop()
{
	work_item_vec  out;

	while ( keep_going() )
	{
		swq.collect(out, 50);

		for (auto & wi : out)
		{
			ultra_task * w = (ultra_task *)wi;

			if (enough)
			{
				pool.put(w);
				continue;
			}

			switch (w->phase)
			{
			case 1: complete_ph1(w); break;
			case 2: complete_ph2(w); break;
			case 3: complete_ph3(w); break;
			default: assert(false);
			}
		}

		out.clear();
	}

	if (! enough)
	{
		info.done = true;
		cb->on_ultra_mach_tick(info);
	}
}

/*
 *
 */
bool ultra_mach_scan(folder & root, const ultra_mach_conf & conf, ultra_mach_cb * cb)
{
	ultra_mach  mach;

	//
	assert(! root.self.name.empty()); // path is set

	if (! mach.init(conf, cb))
		return false;

	mach.ph1_only = true;

	mach.enqueue_ph1(&root);
	mach.info.d_found = 1;

	mach.loop();
	mach.term();

	return ! mach.enough;
}

//
static
bool ultra_mach_delete(folder & root, const ultra_mach_conf & conf, ultra_mach_cb * cb)
{
	ultra_mach     mach;
	folder_vec     list;
	work_item_vec  out;

	//
	assert(! root.self.name.empty()); // path is set

	if (! mach.init(conf, cb))
		return false;

	root.census(list);

	for (auto & x : list)
	{
		if (x->files.size())
			mach.enqueue_ph2(x);
		else
		if (x->folders.empty())
			mach.enqueue_ph3(x);

		// ^ same as complete_ph1()
	}

	mach.loop();
	mach.term();

	return ! mach.enough;
}

//
static
bool ultra_mach_scan_and_delete(folder & root, const ultra_mach_conf & conf, ultra_mach_cb * cb)
{
	ultra_mach  mach;

	//
	assert(! root.self.name.empty()); // path is set

	if (! mach.init(conf, cb))
		return false;

	mach.ph1_only = false;

	mach.enqueue_ph1(&root);
	mach.info.d_found = 1;

	mach.loop();
	mach.term();

	return ! mach.enough;
}

//
bool ultra_mach_delete(folder & root, bool prescanned, const ultra_mach_conf & conf, ultra_mach_cb * cb)
{
	return prescanned ? ultra_mach_delete(root, conf, cb)
	                  : ultra_mach_scan_and_delete(root, conf, cb);
}
