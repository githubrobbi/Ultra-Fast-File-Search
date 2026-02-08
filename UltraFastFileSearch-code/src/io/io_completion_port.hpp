#pragma once
/**
 * @file io_completion_port.hpp
 * @brief Windows I/O Completion Port wrapper classes for high-performance async I/O
 *
 * @details
 * This file provides IoCompletionPort and OleIoCompletionPort classes that wrap
 * the Windows I/O Completion Port (IOCP) mechanism for efficient asynchronous I/O.
 *
 * ## What is an I/O Completion Port?
 *
 * IOCP is a Windows kernel object that provides the most scalable mechanism for
 * handling asynchronous I/O on Windows. Key benefits:
 *
 * - Thread Pool Integration: Multiple worker threads wait on a single IOCP
 * - Efficient Scheduling: The kernel wakes only the optimal number of threads
 * - Priority Support: I/O operations can be prioritized
 * - Scalability: Handles thousands of concurrent I/O operations efficiently
 *
 * ## Completion Packet Key Values
 *
 * The IOCP uses a key parameter to distinguish packet types:
 *
 * | Key Value | Meaning                              |
 * |-----------|--------------------------------------|
 * | 0         | Shutdown signal - worker exits loop  |
 * | 1         | Pending task available for dispatch  |
 * | > 1       | I/O completion for associated file   |
 *
 * ## Thread Safety
 *
 * - All public methods are thread-safe (marked volatile)
 * - Internal state protected by _mutex (recursive mutex)
 * - Atomic flags for _initialized and _terminated
 *
 * @see io_completion_port_impl.hpp - Detailed implementation documentation
 * @see Overlapped - Base class for async I/O completion callbacks
 * @see IoPriority - I/O priority query/set utilities
 */

#ifndef UFFS_IO_COMPLETION_PORT_HPP
#define UFFS_IO_COMPLETION_PORT_HPP

// Windows headers
#include <Windows.h>
#include <process.h>

// Standard library
#include <vector>

// Utility headers
#include "util/atomic_compat.hpp"
#include "util/lock_ptr.hpp"
#include "util/handle.hpp"
#include "util/error_utils.hpp"
#include "util/com_init.hpp"

// I/O headers
#include "overlapped.hpp"
#include "winnt_types.hpp"
#include "io_priority.hpp"

namespace winnt = uffs::winnt;

// global_exception_handler is provided by exception_handler.hpp
// (included before this file in UltraFastFileSearch.cpp)
// Forward declaration for standalone compilation:
#ifndef UFFS_EXCEPTION_HANDLER_HPP
long global_exception_handler(struct _EXCEPTION_POINTERS* ExceptionInfo);
#endif

/**
 * @brief High-performance I/O Completion Port wrapper for async file operations
 *
 * IoCompletionPort manages a Windows IOCP kernel object and a pool of worker
 * threads that process completed I/O operations. It provides priority-based
 * scheduling for pending I/O requests.
 */
class IoCompletionPort
{
	typedef IoCompletionPort this_type;
protected:
	/**
	 * @brief Pending I/O task descriptor
	 *
	 * Represents an I/O operation that has been requested but not yet issued
	 * to the operating system. Tasks are queued in _pending and processed
	 * by worker threads based on I/O priority.
	 */
	struct Task
	{
		HANDLE file;                         ///< Handle to the file being read
		unsigned long issuing_thread_id;     ///< Thread that requested the I/O
		void* buffer;                        ///< Destination buffer for read data
		unsigned long cb;                    ///< Number of bytes to read
		intrusive_ptr<Overlapped> overlapped; ///< Overlapped structure with callback
	};

	typedef std::vector<Task> Pending;

	/**
	 * @brief RAII wrapper for a worker thread handle
	 *
	 * Manages the lifetime of a worker thread. The destructor waits for the
	 * thread to complete before returning, ensuring clean shutdown.
	 */
	struct WorkerThread
	{
		Handle handle;
		WorkerThread() = default;
		WorkerThread(const WorkerThread&) = delete;
		WorkerThread& operator=(const WorkerThread&) = delete;
		WorkerThread(WorkerThread&& other) noexcept : handle(std::move(other.handle)) {}
		WorkerThread& operator=(WorkerThread&& other) noexcept
		{
			if (this != &other)
			{
				if (Handle::valid(this->handle))
				{
					WaitForSingleObject(this->handle, INFINITE);
				}
				handle = std::move(other.handle);
			}
			return *this;
		}
		~WorkerThread()
		{
			if (Handle::valid(this->handle))
			{
				WaitForSingleObject(this->handle, INFINITE);
			}
		}
	};

	/// Static thread entry point for _beginthreadex
	static unsigned int __stdcall iocp_worker(void* me)
	{
		return static_cast<IoCompletionPort volatile*>(me)->worker();
	}

	/// Get the number of CPU cores for thread pool sizing
	static int get_num_threads()
	{
		SYSTEM_INFO sysinfo;
		GetSystemInfo(&sysinfo);
		return static_cast<int>(sysinfo.dwNumberOfProcessors);
	}

	/// Worker thread entry point (infinite timeout)
	virtual unsigned int worker() volatile
	{
		return this->worker(INFINITE);
	}

	/**
	 * @brief Main worker thread processing loop
	 *
	 * @param timeout Maximum time to wait for a completion packet (milliseconds)
	 * @return Thread exit code (0 for success, error code otherwise)
	 *
	 * Processing flow:
	 * 1. Wait for completion packet via GetQueuedCompletionStatus()
	 * 2. If overlapped != NULL: invoke the callback
	 * 3. If key == 1: find highest-priority pending task and issue it
	 * 4. If key == 0: set terminated flag and exit loop
	 *
	 * @see io_completion_port_impl.hpp for detailed flowchart
	 */
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
					// I/O completion - invoke the callback
					int r = (*overlapped)(static_cast<size_t>(nr), key);
					if (r > 0)
					{
						r = this->try_post(nr, key, overlapped) ? 0 : -1;
					}

					if (r >= 0)
					{
						(void)overlapped.detach();  // Intentionally release ownership
					}
				}
				else if (key == 1)  // Pending task signal
				{
					// Find and issue the highest-priority pending task
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
				else if (key == 0)  // Termination signal
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

	/// Issue a pending I/O operation to the operating system
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
			(void)task.overlapped.detach();  // Intentionally release ownership
		}
	}

	/// Cancel all pending I/O operations initiated by the current thread
	void cancel_thread_ios() volatile
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

	Handle _handle;                                    ///< IOCP kernel object handle
	atomic_namespace::atomic<bool> _initialized;       ///< True after worker threads started
	atomic_namespace::atomic<bool> _terminated;        ///< True when shutdown initiated
	std::vector<WorkerThread> _threads;                ///< Worker thread pool
	mutable atomic_namespace::recursive_mutex _mutex;  ///< Protects _pending
	Pending _pending;                                  ///< Tasks waiting to be issued
	size_t _pending_scan_offset;                       ///< Round-robin scan position
public:
	/// Destructor - closes the IOCP and waits for all threads
	~IoCompletionPort()
	{
		this->close();
	}

	/// Constructor - creates the IOCP kernel object
	IoCompletionPort() : _handle(CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0)), _initialized(false), _terminated(false), _pending_scan_offset() {}

	/// Cast away volatile qualifier (for internal use)
	this_type* unvolatile() volatile
	{
		return const_cast<this_type*>(this);
	}

	/// Cast away volatile qualifier (const version)
	this_type const* unvolatile() const volatile
	{
		return const_cast<this_type*>(this);
	}

	/// Get the mutex for external locking
	atomic_namespace::recursive_mutex& get_mutex() const volatile
	{
		return this->unvolatile()->_mutex;
	}

	/// Initialize the worker thread pool (lazy initialization)
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

	/// Thread-safe version of ensure_initialized
	void ensure_initialized() volatile
	{
		if (!this->_initialized.load(atomic_namespace::memory_order_acquire))
		{
			lock(this)->ensure_initialized();
		}
	}

	/// Post a completion packet to the IOCP
	void post(unsigned long cb, uintptr_t const key, intrusive_ptr<Overlapped> overlapped) volatile
	{
		CheckAndThrow(this->try_post(cb, key, overlapped));
		(void)overlapped.detach();  // Intentionally release ownership
	}

	/// Try to post a completion packet (returns false on failure)
	bool try_post(unsigned long cb, uintptr_t const key, intrusive_ptr<Overlapped>& overlapped) volatile
	{
		this->ensure_initialized();
		if (!(cb == 0 && key == 0 && !overlapped) && this->_terminated.load(atomic_namespace::memory_order_acquire))
		{
			CppRaiseException(ERROR_CANCELLED);
		}

		return !!PostQueuedCompletionStatus(this->_handle, cb, key, overlapped.get());
	}

	/**
	 * @brief Queue an async file read operation
	 *
	 * @param file Handle to the file to read
	 * @param buffer Destination buffer for read data
	 * @param cb Number of bytes to read
	 * @param overlapped Overlapped structure with completion callback
	 *
	 * The read is not issued immediately - it's added to the pending queue
	 * and a worker thread will issue it based on priority.
	 */
	void read_file(HANDLE const file, void* const buffer, unsigned long const cb, intrusive_ptr<Overlapped> const& overlapped) volatile
	{
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

		this->post(0, 1, nullptr);  // Signal pending task available
	}

	/// Associate a file handle with this IOCP
	void associate(HANDLE const file, uintptr_t const key) volatile
	{
		this->ensure_initialized();
		CheckAndThrow(!!CreateIoCompletionPort(file, this->_handle, key, 0));
	}

	/// Close the IOCP and wait for all threads to terminate
	void close()
	{
		for (size_t i = 0; i != this->_threads.size(); ++i)
		{
			this->post(0, 0, nullptr);  // Send termination signal
		}

		this->cancel_thread_ios();
		this->_threads.clear();  // Destructors ensure all threads terminate
		this->worker(0);  // Dequeue all remaining packets
	}
};

/**
 * @brief I/O Completion Port with COM/OLE support
 *
 * Extends IoCompletionPort to initialize COM on each worker thread.
 * This is required when the completion callbacks need to use COM objects
 * (e.g., Shell APIs, WIC, etc.).
 */
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
		CoInit coinit;  // Initialize COM for this thread
		return this->IoCompletionPort::worker(timeout);
	}
};

// Include implementation documentation
#include "io_completion_port_impl.hpp"

#endif // UFFS_IO_COMPLETION_PORT_HPP
