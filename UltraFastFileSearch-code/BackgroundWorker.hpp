#pragma once

#ifndef BACKGROUNDWORKER_HPP
#define BACKGROUNDWORKER_HPP

#include <limits.h>
#include <process.h>
#include <deque>
#include <utility>
#include <Windows.h>
#include <Objbase.h>

class BackgroundWorker
{
	BackgroundWorker(BackgroundWorker const &);
	BackgroundWorker &operator =(BackgroundWorker const &);
protected:
	struct DECLSPEC_NOVTABLE Thunk
	{
		virtual bool operator()() { return false; }
		virtual ~Thunk() { }
	};
	class CSLock
	{
	public:
		CRITICAL_SECTION& cs;
		CSLock(CRITICAL_SECTION& criticalSection) : cs(criticalSection) { EnterCriticalSection(&this->cs); }
		~CSLock() { LeaveCriticalSection(&this->cs); }
	};
	mutable CRITICAL_SECTION criticalSection;
	mutable int refs;
	friend void intrusive_ptr_add_ref(BackgroundWorker const volatile *p) { ++p->refs; }
	friend void intrusive_ptr_release(BackgroundWorker const volatile *p) { if (!--p->refs) { delete p; } }

private:
	virtual void add(Thunk *pThunk, long const insert_before_timestamp) = 0;

public:
	BackgroundWorker() : refs() { InitializeCriticalSection(&this->criticalSection); }
	virtual ~BackgroundWorker() throw(std::exception) { DeleteCriticalSection(&this->criticalSection); }

	virtual void clear() = 0;

	static BackgroundWorker *create(bool coInitialize = true, long exception_handler(struct _EXCEPTION_POINTERS *) = NULL);

	template<typename Func>
	void add(Func const &func, long const insert_before_timestamp)
	{
		CSLock lock(this->criticalSection);
		struct FThunk : public Thunk
		{
			Func func;
			FThunk(Func const &func) : func(func) { }
			bool operator()() { return this->func() != 0; }
		};
		std::auto_ptr<Thunk> pThunk(new FThunk(func));
		this->add(pThunk.get(), insert_before_timestamp);
		pThunk.release();
	}
};

class BackgroundWorkerImpl : public BackgroundWorker
{
	class CoInit
	{
		CoInit(CoInit const &) : hr(S_FALSE) { }
	public:
		HRESULT hr;
		CoInit(bool initialize = true) : hr(initialize ? CoInitialize(NULL) : S_FALSE) { }
		~CoInit() { if (this->hr == S_OK) { CoUninitialize(); } }
	};

	std::deque<std::pair<long, Thunk *> > todo;
	unsigned int tid;
	bool coInitialize;
	HANDLE hThread;
	HANDLE hSemaphore;
	volatile bool stop;
	long (*exception_handler)(struct _EXCEPTION_POINTERS *);
	static unsigned int CALLBACK entry(void *arg)
	{
		BackgroundWorkerImpl *const me = (BackgroundWorkerImpl *)arg;
		unsigned int result = 0;
		__try
		{
			result = me->process();
		}
		__except (me->exception_handler ? me->exception_handler(static_cast<struct _EXCEPTION_POINTERS *>(GetExceptionInformation())) : EXCEPTION_CONTINUE_SEARCH)
		{
			result = GetExceptionCode();
		}
		return result;
	}
public:
	BackgroundWorkerImpl(bool coInitialize, long exception_handler(struct _EXCEPTION_POINTERS *))
		: tid(0), coInitialize(coInitialize), hThread(), hSemaphore(NULL), stop(false), exception_handler(exception_handler)
	{
		this->hThread = (HANDLE)_beginthreadex(NULL, 0, entry, this, CREATE_SUSPENDED, &tid);
		this->hSemaphore = CreateSemaphore(NULL, 0, LONG_MAX, NULL);
		ResumeThread(this->hThread);
	}

	~BackgroundWorkerImpl() throw(std::exception)
	{
		// Clear all the tasks
		this->stop = true;
		CSLock lock(this->criticalSection);
		this->todo.clear();
		LONG prev;
		if (!ReleaseSemaphore(this->hSemaphore, 1, &prev) || WaitForSingleObject(this->hThread, INFINITE) != WAIT_OBJECT_0)
		{ throw new std::runtime_error("The background thread did not terminate properly."); }
		CloseHandle(this->hSemaphore);
		CloseHandle(this->hThread);
	}

	void clear()
	{
		CSLock lock(this->criticalSection);
		this->todo.clear();
	}

	bool empty() const
	{
		CSLock lock(this->criticalSection);
		return this->todo.empty();
	}

	unsigned int process()
	{
		// SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
		// SetThreadPriority(GetCurrentThread(), 0x00010000 /*THREAD_MODE_BACKGROUND_BEGIN*/);
		DWORD result = 0;
		CoInit const com(this->coInitialize);
		while ((result = WaitForSingleObject(this->hSemaphore, INFINITE)) == WAIT_OBJECT_0)
		{
			if (this->stop)
			{
				result = ERROR_CANCELLED;
				break;
			}
			Thunk *pThunk = NULL;
			{
				CSLock lock(this->criticalSection);
				if (!this->todo.empty())
				{
					std::auto_ptr<Thunk> next(this->todo.front().second);
					this->todo.pop_front();
					pThunk = next.release();
				}
			}
			if (pThunk)
			{
				std::auto_ptr<Thunk> thunk(pThunk);
				pThunk = NULL;
				if (!(*thunk)())
				{
					result = ERROR_REQUEST_ABORTED;
					break;
				}
			}
		}
		if (result == WAIT_FAILED) { result = GetLastError(); }
		return result;
	}

	void add(Thunk *pThunk, long const insert_before_timestamp)
	{
		DWORD exitCode;
		if (GetExitCodeThread(this->hThread, &exitCode) && exitCode == STILL_ACTIVE)
		{
			size_t i = 0;
			while (i < this->todo.size() && insert_before_timestamp <= this->todo[i].first)
			{
				++i;
			}
			this->todo.insert(this->todo.begin() + static_cast<ptrdiff_t>(i), std::make_pair(insert_before_timestamp, pThunk));
			LONG prev;
			if (!ReleaseSemaphore(this->hSemaphore, 1, &prev))
			{
				throw std::runtime_error("Unexpected error when releasing semaphore.");
			}
		}
		else
		{
			throw std::runtime_error("The background thread has terminated, probably because the callback told it to stop.");
		}
	}
};
BackgroundWorker *BackgroundWorker::create(bool coInitialize, long exception_handler(struct _EXCEPTION_POINTERS *))
{
	return new BackgroundWorkerImpl(coInitialize, exception_handler);
}

#endif