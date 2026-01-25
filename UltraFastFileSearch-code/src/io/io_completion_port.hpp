#pragma once
/**
 * @file io_completion_port.hpp
 * @brief Windows I/O Completion Port wrapper classes
 *
 * Provides IoCompletionPort and OleIoCompletionPort classes for managing
 * asynchronous I/O operations using Windows IOCP.
 *
 * Dependencies (must be defined before including this header):
 * - Handle class
 * - IoPriority::query
 * - CheckAndThrow, CppRaiseException
 * - CStructured_Exception
 * - global_exception_handler
 * - CoInit
 * - winnt::IO_PRIORITY_HINT
 */

#ifndef UFFS_IO_COMPLETION_PORT_HPP
#define UFFS_IO_COMPLETION_PORT_HPP

#include <Windows.h>
#include <process.h>
#include <vector>

#include "../util/atomic_compat.hpp"
#include "../util/lock_ptr.hpp"
#include "overlapped.hpp"
#include "winnt_types.hpp"

namespace winnt = uffs::winnt;  // Alias for winnt:: prefix usage

class IoCompletionPort
{
	typedef IoCompletionPort this_type;
protected:
	struct Task
	{
		HANDLE file;
		unsigned long issuing_thread_id;
		void* buffer;
		unsigned long cb;
		intrusive_ptr<Overlapped> overlapped;
	};

	typedef std::vector<Task> Pending;

	struct WorkerThread
	{
		Handle handle;
		WorkerThread() = default;
		WorkerThread(const WorkerThread&) = delete;
		WorkerThread& operator=(const WorkerThread&) = delete;
		~WorkerThread()
		{
			if (Handle::valid(this->handle))
			{
				WaitForSingleObject(this->handle, INFINITE);
			}
		}
	};

	static unsigned int __stdcall iocp_worker(void* me)
	{
		return static_cast<IoCompletionPort volatile*>(me)->worker();
	}

	static int get_num_threads()
	{
		SYSTEM_INFO sysinfo;
		GetSystemInfo(&sysinfo);
		return static_cast<int>(sysinfo.dwNumberOfProcessors);
	}

	virtual unsigned int worker() volatile
	{
		return this->worker(INFINITE);
	}

	virtual unsigned int worker(unsigned long const timeout) volatile
	{
		unsigned int result = 0;
		ULONG_PTR key;
		OVERLAPPED* overlapped_ptr;
		Overlapped* p;
		try
		{
			for (unsigned long nr; GetQueuedCompletionStatus(this->_handle, &nr, &key, &overlapped_ptr, timeout);)
			{
				p = static_cast<Overlapped*>(overlapped_ptr);
				intrusive_ptr<Overlapped> overlapped(p, false);
				if (overlapped.get())
				{
					int r = (*overlapped)(static_cast<size_t>(nr), key);
					if (r > 0)
					{
						r = this->try_post(nr, key, overlapped) ? 0 : -1;
					}

					if (r >= 0)
					{
						overlapped.detach();
					}
				}
				else if (key == 1)
				{
					size_t found = ~size_t();
					Task task;
					{
						lock_ptr<this_type> const me_lock(this);
						size_t& pending_scan_offset = const_cast<size_t&>(this->_pending_scan_offset);
						Pending& pending = const_cast<Pending&>(this->_pending);
						winnt::IO_PRIORITY_HINT found_priority = winnt::MaxIoPriorityTypes;
						for (size_t o = 0; o != pending.size(); ++o)
						{
							if (pending_scan_offset == 0 || pending_scan_offset > pending.size())
							{
								pending_scan_offset = pending.size();
							}

							--pending_scan_offset;
							size_t const i = pending_scan_offset;
							winnt::IO_PRIORITY_HINT const curr_priority = IoPriority::query(reinterpret_cast<uintptr_t>(pending[i].file));
							if (found_priority == winnt::MaxIoPriorityTypes || curr_priority > found_priority)
							{
								found = i;
								found_priority = curr_priority;
							}
						}

						if (found < pending.size())
						{
							pending_scan_offset = found;
							task = pending[found];
							pending.erase(pending.begin() + static_cast<ptrdiff_t>(found));
						}
					}

					if (~found)
					{
						this->enqueue(task);
					}
				}
				else if (key == 0)
				{
					this->_terminated.store(true, atomic_namespace::memory_order_release);
					this->cancel_thread_ios();
					break;
				}
			}
		}
		catch (CStructured_Exception& ex)
		{
			result = ex.GetSENumber();
			if (result != ERROR_CANCELLED
#ifndef _DEBUG
				&& IsDebuggerPresent()
#endif
				)
			{
				throw;
			}
		}

		return result;
	}

	void enqueue(Task& task) volatile
	{
		bool attempted_reading_file = false;
		if (!task.cb || (!this->_terminated.load(atomic_namespace::memory_order_acquire) && ((attempted_reading_file = true), ReadFile(task.file, task.buffer, task.cb, nullptr, task.overlapped.get()))))
		{
			this->post(task.cb, 0, task.overlapped);
		}
		else if (attempted_reading_file)
		{
			CheckAndThrow(GetLastError() == ERROR_IO_PENDING);
			task.overlapped.detach();
		}
	}

	void cancel_thread_ios() volatile	// Requests cancellation of all I/Os initiated by the current thread
	{
		lock_ptr<this_type> const me_lock(this);
		Pending& pending = const_cast<Pending&>(this->_pending);
		unsigned long const thread_id = GetCurrentThreadId();
		for (size_t i = 0; i != pending.size(); ++i)
		{
			if (pending[i].issuing_thread_id == thread_id)
			{
				CancelIo(pending[i].file);
			}
		}
	}

	Handle _handle;
	atomic_namespace::atomic<bool> _initialized, _terminated;
	std::vector<WorkerThread> _threads;
	mutable atomic_namespace::recursive_mutex _mutex;
	Pending _pending;
	size_t _pending_scan_offset;
public:
	~IoCompletionPort()
	{
		this->close();
	}

	IoCompletionPort() : _handle(CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0)), _initialized(false), _terminated(false), _pending_scan_offset() {}

	this_type* unvolatile() volatile
	{
		return const_cast<this_type*>(this);
	}

	this_type const* unvolatile() const volatile
	{
		return const_cast<this_type*>(this);
	}

	atomic_namespace::recursive_mutex& get_mutex() const volatile
	{
		return this->unvolatile()->_mutex;
	}

	void ensure_initialized()
	{
		if (this->_threads.empty())
		{
			size_t const nthreads = static_cast<size_t>(get_num_threads());
			this->_threads.resize(nthreads);
			for (size_t i = nthreads; i != 0 && ((void)--i, true);)
			{
				unsigned int id;
				Handle(reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, iocp_worker, this, 0, &id))).swap(this->_threads.at(i).handle);
			}

			this->_initialized.store(true, atomic_namespace::memory_order_release);
		}
	}

	void ensure_initialized() volatile
	{
		if (!this->_initialized.load(atomic_namespace::memory_order_acquire))
		{
			lock(this)->ensure_initialized();
		}
	}

	void post(unsigned long cb, uintptr_t const key, intrusive_ptr<Overlapped> overlapped) volatile
	{
		CheckAndThrow(this->try_post(cb, key, overlapped));
		overlapped.detach();
	}

	bool try_post(unsigned long cb, uintptr_t const key, intrusive_ptr<Overlapped>& overlapped) volatile
	{
		this->ensure_initialized();
		if (!(cb == 0 && key == 0 && !overlapped) && this->_terminated.load(atomic_namespace::memory_order_acquire))
		{
			CppRaiseException(ERROR_CANCELLED);
		}

		return !!PostQueuedCompletionStatus(this->_handle, cb, key, overlapped.get());
	}

	void read_file(HANDLE const file, void* const buffer, unsigned long const cb, intrusive_ptr<Overlapped> const& overlapped) volatile
	{
		// This part needs a lock
		{
			lock_ptr<this_type> const me_lock(this);
			Pending& pending = const_cast<Pending&>(this->_pending);
			pending.push_back(Task());
			Task& task = pending.back();
			task.file = file;
			task.issuing_thread_id = GetCurrentThreadId();
			task.buffer = buffer;
			task.cb = cb;
			task.overlapped = overlapped;
		}

		this->post(0, 1, nullptr);
	}

	void associate(HANDLE const file, uintptr_t const key) volatile
	{
		this->ensure_initialized();
		CheckAndThrow(!!CreateIoCompletionPort(file, this->_handle, key, 0));
	}

	void close()
	{
		for (size_t i = 0; i != this->_threads.size(); ++i)
		{
			this->post(0, 0, nullptr);
		}

		this->cancel_thread_ios();
		this->_threads.clear();	// Destructors ensure all threads terminate
		this->worker(0 /*dequeue all packets */);
	}
};

class OleIoCompletionPort : public IoCompletionPort
{
	virtual unsigned int worker() volatile override
	{
		unsigned int result = 0;
		__try
		{
			result = this->IoCompletionPort::worker();
		}
		__except (global_exception_handler(static_cast<struct _EXCEPTION_POINTERS*>(GetExceptionInformation())))
		{
			result = GetExceptionCode();
		}

		return result;
	}

	virtual unsigned int worker(unsigned long const timeout) volatile override
	{
		CoInit coinit;
		return this->IoCompletionPort::worker(timeout);
	}
};

#endif // UFFS_IO_COMPLETION_PORT_HPP
