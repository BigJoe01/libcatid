/*
	Copyright (c) 2009 Christopher A. Taylor.  All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	* Redistributions of source code must retain the above copyright notice,
	  this list of conditions and the following disclaimer.
	* Redistributions in binary form must reproduce the above copyright notice,
	  this list of conditions and the following disclaimer in the documentation
	  and/or other materials provided with the distribution.
	* Neither the name of LibCat nor the names of its contributors may be used
	  to endorse or promote products derived from this software without
	  specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
	ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
	LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
	CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/

#include <cat/threads/RWLock.hpp>
#include <cat/threads/Atomic.hpp>
using namespace cat;

//// RWLock

RWLock::RWLock()
{
	_locked = 0;
	_rd_count = 0;
	_wr_count = 0;
}

RWLock::~RWLock()
{
}

void RWLock::ReadLock()
{
	for (;;)
	{
		while (_wr_count != 0)
		{
			SwitchToThread();
		}

		Atomic::Add(&_rd_count, 1);

		if (_wr_count != 0)
		{
			Atomic::Add(&_rd_count, -1);
		}
		else
		{
			return;
		}
	}
}

void RWLock::ReadUnlock()
{
	Atomic::Add(&_rd_count, -1);
}

void RWLock::WriteLock()
{
	_wr_lock.Enter();

	_wr_count = 1;

	while (_rd_count != 0)
	{
		SwitchToThread();
	}
}

void RWLock::WriteUnlock()
{
	_wr_count = 0;

	_wr_lock.Leave();
}


//// AutoReadLock

AutoReadLock::AutoReadLock(RWLock &lock)
{
	_lock = &lock;
	lock.ReadLock();
}

AutoReadLock::~AutoReadLock()
{
	Release();
}

bool AutoReadLock::Release()
{
	if (_lock)
	{
		_lock->ReadUnlock();
		_lock = 0;
	}

	return true;
}


//// AutoWriteLock

AutoWriteLock::AutoWriteLock(RWLock &lock)
{
	_lock = &lock;
	lock.WriteLock();
}

AutoWriteLock::~AutoWriteLock()
{
	Release();
}

bool AutoWriteLock::Release()
{
	if (_lock)
	{
		_lock->WriteUnlock();
		_lock = 0;
	}

	return true;
}