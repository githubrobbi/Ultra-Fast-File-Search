#pragma once
/**
 * @file io_completion_port_impl.hpp
 * @brief Implementation details for Windows I/O Completion Port wrapper
 *
 * @details
 * This file contains the implementation of IoCompletionPort and OleIoCompletionPort
 * classes. It is included at the end of io_completion_port.hpp and should not be
 * included directly.
 *
 * ## Windows I/O Completion Ports (IOCP) Overview
 *
 * IOCP is a Windows kernel object that provides an efficient mechanism for
 * handling asynchronous I/O operations. Key concepts:
 *
 * 1. **Completion Port**: A kernel queue that receives notifications when
 *    async I/O operations complete
 *
 * 2. **Worker Threads**: A pool of threads that wait on the completion port
 *    and process completed I/O operations
 *
 * 3. **Overlapped I/O**: Windows async I/O mechanism where operations are
 *    initiated and completion is signaled later
 *
 * ## Architecture Diagram
 *
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                         IoCompletionPort                                │
 * │                                                                         │
 * │  ┌─────────────────┐     ┌─────────────────────────────────────────┐   │
 * │  │  Main Thread    │     │           Worker Thread Pool            │   │
 * │  │                 │     │  ┌─────────┐ ┌─────────┐ ┌─────────┐   │   │
 * │  │  read_file() ───┼──┐  │  │Worker 0 │ │Worker 1 │ │Worker N │   │   │
 * │  │  associate()    │  │  │  └────┬────┘ └────┬────┘ └────┬────┘   │   │
 * │  │  post()         │  │  │       │           │           │        │   │
 * │  └─────────────────┘  │  │       ▼           ▼           ▼        │   │
 * │                       │  │  ┌─────────────────────────────────┐   │   │
 * │                       │  │  │   GetQueuedCompletionStatus()   │   │   │
 * │                       │  │  │         (blocking wait)         │   │   │
 * │                       │  │  └───────────────┬─────────────────┘   │   │
 * │                       │  │                  │                     │   │
 * │                       │  └──────────────────┼─────────────────────┘   │
 * │                       │                     │                         │
 * │                       ▼                     ▼                         │
 * │  ┌─────────────────────────────────────────────────────────────────┐ │
 * │  │                    IOCP Kernel Object                           │ │
 * │  │  ┌─────────────────────────────────────────────────────────┐   │ │
 * │  │  │              Completion Queue                            │   │ │
 * │  │  │  ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐            │   │ │
 * │  │  │  │Packet 1│ │Packet 2│ │Packet 3│ │  ...   │            │   │ │
 * │  │  │  └────────┘ └────────┘ └────────┘ └────────┘            │   │ │
 * │  │  └─────────────────────────────────────────────────────────┘   │ │
 * │  └─────────────────────────────────────────────────────────────────┘ │
 * │                       ▲                                              │
 * │                       │                                              │
 * │  ┌────────────────────┴────────────────────────────────────────────┐ │
 * │  │                    Pending Queue                                 │ │
 * │  │  (Tasks waiting to be issued - priority-based scheduling)        │ │
 * │  │  ┌──────┐ ┌──────┐ ┌──────┐                                     │ │
 * │  │  │Task 1│ │Task 2│ │Task 3│                                     │ │
 * │  │  └──────┘ └──────┘ └──────┘                                     │ │
 * │  └─────────────────────────────────────────────────────────────────┘ │
 * └─────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * ## Completion Packet Key Values
 *
 * The IOCP uses a `key` value to distinguish between different types of
 * completion packets:
 *
 * | Key | Meaning                | Action                                    |
 * |-----|------------------------|-------------------------------------------|
 * | 0   | Termination signal     | Worker thread exits the processing loop   |
 * | 1   | Pending task available | Dequeue and issue a pending I/O operation |
 * | >1  | I/O completion         | Process the completed overlapped I/O      |
 *
 * ## Priority-Based Task Scheduling
 *
 * When multiple I/O operations are pending, the worker thread selects the
 * highest-priority task to issue next. This prevents low-priority bulk reads
 * from starving interactive operations.
 *
 * The priority is queried from the file handle using `NtQueryInformationFile`
 * with `FileIoPriorityHintInformation`.
 *
 * ## Thread Safety
 *
 * - The `_pending` queue is protected by `_mutex`
 * - Atomic flags (`_initialized`, `_terminated`) use memory ordering
 * - The `volatile` qualifier on methods indicates they can be called from
 *   any thread (the method handles synchronization internally)
 *
 * @see io_completion_port.hpp - Public API header
 * @see Overlapped - Base class for async I/O operations
 * @see IoPriority - I/O priority management
 */

#ifndef UFFS_IO_COMPLETION_PORT_IMPL_HPP
#define UFFS_IO_COMPLETION_PORT_IMPL_HPP

// This file should only be included from io_completion_port.hpp
#ifndef UFFS_IO_COMPLETION_PORT_HPP
#error "Do not include io_completion_port_impl.hpp directly. Include io_completion_port.hpp instead."
#endif

// ============================================================================
// Key Value Constants
// ============================================================================
/**
 * @brief Completion packet key values for IOCP signaling
 *
 * These constants define the meaning of the `key` parameter in completion
 * packets posted to the I/O completion port.
 */
namespace iocp_keys {
    /**
     * @brief Termination signal - worker thread should exit
     *
     * When a worker thread receives a packet with key=0, it sets the
     * `_terminated` flag and exits the processing loop. This is used
     * during shutdown to gracefully stop all worker threads.
     */
    constexpr ULONG_PTR kTerminate = 0;

    /**
     * @brief Pending task signal - dequeue and issue a pending I/O
     *
     * When a worker thread receives a packet with key=1, it scans the
     * pending queue for the highest-priority task and issues it.
     * This implements the priority-based scheduling mechanism.
     */
    constexpr ULONG_PTR kPendingTask = 1;
}

// ============================================================================
// Task Structure Implementation
// ============================================================================

/**
 * @brief Pending I/O task descriptor
 *
 * @details
 * Represents an I/O operation that has been requested but not yet issued
 * to the operating system. Tasks are queued in the `_pending` vector and
 * processed by worker threads based on priority.
 *
 * ## Lifecycle
 *
 * 1. `read_file()` creates a Task and adds it to `_pending`
 * 2. `read_file()` posts a key=1 packet to wake a worker
 * 3. Worker receives key=1, scans `_pending` for highest priority task
 * 4. Worker calls `enqueue()` to issue the I/O via `ReadFile()`
 * 5. When I/O completes, the overlapped callback is invoked
 *
 * ## Fields
 *
 * | Field              | Description                                      |
 * |--------------------|--------------------------------------------------|
 * | file               | Handle to the file being read                    |
 * | issuing_thread_id  | Thread that requested the I/O (for cancellation) |
 * | buffer             | Destination buffer for read data                 |
 * | cb                 | Number of bytes to read                          |
 * | overlapped         | Overlapped structure with completion callback    |
 */
// Task struct is defined in the class body above

// ============================================================================
// WorkerThread Structure Implementation
// ============================================================================

/**
 * @brief RAII wrapper for a worker thread handle
 *
 * @details
 * Manages the lifetime of a worker thread. The destructor waits for the
 * thread to complete before returning, ensuring clean shutdown.
 *
 * ## Move Semantics
 *
 * WorkerThread is move-only (non-copyable) because each instance owns
 * a unique thread handle. Moving transfers ownership of the handle.
 *
 * ## Destructor Behavior
 *
 * The destructor calls `WaitForSingleObject(INFINITE)` to block until
 * the thread exits. This ensures:
 * - No dangling thread handles
 * - Clean shutdown without race conditions
 * - All I/O operations complete before destruction
 */
// WorkerThread struct is defined in the class body above

// ============================================================================
// Worker Thread Entry Point
// ============================================================================

/**
 * @brief Static thread entry point for worker threads
 *
 * @param me Pointer to the IoCompletionPort instance (cast to void*)
 * @return Thread exit code (0 for success, error code otherwise)
 *
 * @details
 * This is the function passed to `_beginthreadex()`. It casts the
 * `void*` parameter back to `IoCompletionPort volatile*` and calls
 * the virtual `worker()` method.
 *
 * The `volatile` qualifier is used because the IoCompletionPort may
 * be accessed from multiple threads simultaneously.
 */
// iocp_worker is defined in the class body above

// ============================================================================
// Worker Loop Implementation
// ============================================================================

/**
 * @brief Main worker thread processing loop
 *
 * @param timeout Maximum time to wait for a completion packet (milliseconds)
 * @return Thread exit code (0 for success, error code otherwise)
 *
 * @details
 * This is the heart of the IOCP implementation. Each worker thread runs
 * this loop, waiting for and processing completion packets.
 *
 * ## Processing Flow
 *
 * ```
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    Worker Loop Flowchart                        │
 * └─────────────────────────────────────────────────────────────────┘
 *
 *     ┌──────────────────────────────────────┐
 *     │  GetQueuedCompletionStatus()         │
 *     │  (blocks until packet available)     │
 *     └──────────────────┬───────────────────┘
 *                        │
 *                        ▼
 *     ┌──────────────────────────────────────┐
 *     │  Check overlapped pointer            │
 *     └──────────────────┬───────────────────┘
 *                        │
 *          ┌─────────────┼─────────────┐
 *          │             │             │
 *          ▼             ▼             ▼
 *     ┌─────────┐   ┌─────────┐   ┌─────────┐
 *     │overlapped│   │ key==1  │   │ key==0  │
 *     │ != NULL  │   │(pending)│   │(terminate)│
 *     └────┬────┘   └────┬────┘   └────┬────┘
 *          │             │             │
 *          ▼             ▼             ▼
 *     ┌─────────┐   ┌─────────┐   ┌─────────┐
 *     │ Invoke  │   │ Find    │   │ Set     │
 *     │callback │   │ highest │   │terminated│
 *     │(*overlapped)│ │priority │   │ flag    │
 *     └────┬────┘   │ task    │   └────┬────┘
 *          │        └────┬────┘        │
 *          │             │             │
 *          │             ▼             ▼
 *          │        ┌─────────┐   ┌─────────┐
 *          │        │ Issue   │   │  EXIT   │
 *          │        │ ReadFile│   │  LOOP   │
 *          │        └────┬────┘   └─────────┘
 *          │             │
 *          └──────┬──────┘
 *                 │
 *                 ▼
 *          ┌─────────────┐
 *          │ Loop back   │
 *          └─────────────┘
 * ```
 *
 * ## Overlapped Callback Return Values
 *
 * The overlapped callback `operator()` returns an int with these meanings:
 *
 * | Return | Meaning                                              |
 * |--------|------------------------------------------------------|
 * | > 0    | Re-queue the operation (post back to IOCP)           |
 * | = 0    | Operation complete, keep overlapped alive            |
 * | < 0    | Operation complete, destroy overlapped               |
 *
 * ## Priority-Based Task Selection
 *
 * When key==1 (pending task signal), the worker scans all pending tasks
 * and selects the one with the highest I/O priority:
 *
 * ```cpp
 * for each task in pending:
 *     priority = IoPriority::query(task.file)
 *     if priority > found_priority:
 *         found = task
 *         found_priority = priority
 * ```
 *
 * This ensures that interactive I/O (high priority) is processed before
 * bulk background I/O (low priority).
 *
 * ## Exception Handling
 *
 * The loop catches `CStructured_Exception` to handle Windows SEH exceptions.
 * `ERROR_CANCELLED` is expected during shutdown and is silently ignored.
 */
// worker() is defined in the class body above

// ============================================================================
// Enqueue Implementation
// ============================================================================

/**
 * @brief Issue a pending I/O operation to the operating system
 *
 * @param task The task to issue
 *
 * @details
 * Called by worker threads to actually issue a `ReadFile()` call for a
 * pending task. Handles three cases:
 *
 * 1. **Zero-byte read** (`task.cb == 0`): Post completion immediately
 * 2. **Synchronous completion**: `ReadFile()` returns TRUE, post completion
 * 3. **Async pending**: `ReadFile()` returns FALSE with `ERROR_IO_PENDING`
 *
 * For case 3, the overlapped pointer is detached (ownership transferred to
 * the OS) and the completion will arrive later via IOCP.
 */
// enqueue() is defined in the class body above

// ============================================================================
// Cancel Thread I/Os Implementation
// ============================================================================

/**
 * @brief Cancel all pending I/O operations initiated by the current thread
 *
 * @details
 * Called during shutdown to cancel any I/O operations that were initiated
 * by the current thread but haven't completed yet. This prevents the
 * thread from blocking indefinitely waiting for I/O.
 *
 * Uses `CancelIo()` which cancels all pending I/O for the specified file
 * handle that was initiated by the calling thread.
 */
// cancel_thread_ios() is defined in the class body above

// ============================================================================
// OleIoCompletionPort Implementation
// ============================================================================

/**
 * @brief I/O Completion Port with COM/OLE support
 *
 * @details
 * Extends IoCompletionPort to initialize COM on each worker thread.
 * This is required when the completion callbacks need to use COM objects
 * (e.g., Shell APIs, WIC, etc.).
 *
 * ## COM Initialization
 *
 * Each worker thread calls `CoInitializeEx()` via the `CoInit` RAII class
 * before entering the processing loop. This ensures COM is properly
 * initialized for the thread's lifetime.
 *
 * ## Exception Handling
 *
 * Uses Windows SEH (`__try/__except`) to catch and handle structured
 * exceptions. The `global_exception_handler` function is called to
 * log or report the exception before the thread exits.
 */
// OleIoCompletionPort is defined in the class body above

#endif // UFFS_IO_COMPLETION_PORT_IMPL_HPP

