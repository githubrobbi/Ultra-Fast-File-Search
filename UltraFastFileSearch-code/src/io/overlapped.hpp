#pragma once

// ============================================================================
// Overlapped I/O Base Class
// ============================================================================
// This file documents the Overlapped base class used for async I/O operations.
// 
// NOTE: The actual Overlapped class is NOT extracted yet because it depends on:
//   - RefCounted (which depends on atomic_namespace)
//   - The intrusive_ptr infrastructure
//
// The class remains in UltraFastFileSearch.cpp until the atomic_namespace
// and RefCounted dependencies are resolved.
//
// This header is provided for documentation and future extraction.
// ============================================================================

#include <Windows.h>

namespace uffs {

// ============================================================================
// Overlapped Class Documentation
// ============================================================================
// The Overlapped class is a base class for async I/O operations using IOCP.
// It inherits from both Windows OVERLAPPED and RefCounted for reference counting.
//
// Key features:
// - Inherits from OVERLAPPED for use with Windows async I/O APIs
// - Reference counted via RefCounted<Overlapped> for safe memory management
// - Pure virtual operator() for completion callback
// - Helper methods for 64-bit offset management
//
// Usage pattern:
//   class MyOperation : public Overlapped {
//   public:
//       int operator()(size_t size, uintptr_t key) override {
//           // Handle completion
//           // Return > 0 to re-queue, 0 to keep alive, < 0 to destroy
//           return -1;
//       }
//   };
//
//   intrusive_ptr<MyOperation> op(new MyOperation());
//   op->offset(file_offset);
//   ReadFile(handle, buffer, size, nullptr, op.get());
//
// The operator() return value semantics:
//   > 0: Re-queue the operation (for chained reads)
//   = 0: Keep the object alive but don't re-queue
//   < 0: Destroy the object (normal completion)
//
// Location in UltraFastFileSearch.cpp: Lines 3107-3131
// ============================================================================

// Forward declaration for documentation
// class Overlapped : public OVERLAPPED, public RefCounted<Overlapped>
// {
// public:
//     virtual ~Overlapped() {}
//     Overlapped() : OVERLAPPED() {}
//
//     // Completion callback - pure virtual
//     virtual int operator()(size_t size, uintptr_t key) = 0;
//
//     // Get 64-bit file offset
//     long long offset() const;
//
//     // Set 64-bit file offset
//     void offset(long long value);
// };

} // namespace uffs

