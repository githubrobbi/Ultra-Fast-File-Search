/**
 * @file ntfs_index_path.hpp
 * @brief Path reconstruction via ParentIterator state machine.
 *
 * This file contains the ParentIterator::operator++() implementation, which
 * uses a Duff's device pattern to iterate through path components from a file
 * up to the root directory.
 *
 * ## Architecture Overview
 *
 * The ParentIterator is a coroutine-like iterator that yields path components
 * one at a time, allowing the caller to build a full path by accumulating
 * components and reversing them at the end.
 *
 * ## State Machine Diagram
 *
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                    ParentIterator State Machine                         │
 * └─────────────────────────────────────────────────────────────────────────┘
 *
 *   ┌──────────┐
 *   │ State 0  │ Initialize: resolve key to file pointers
 *   │  INIT    │
 *   └────┬─────┘
 *        │ (if directory and not root)
 *        ▼
 *   ┌──────────┐
 *   │ State 1  │ Yield: "\" (directory separator)
 *   │ DIR_SEP  │
 *   └────┬─────┘
 *        │ (first iteration only, if has attribute)
 *        ▼
 *   ┌──────────┐
 *   │ State 2  │ Yield: "$ATTRIBUTE_TYPE" (e.g., "$INDEX_ALLOCATION")
 *   │ ATTR_TYPE│
 *   └────┬─────┘
 *        │
 *        ▼
 *   ┌──────────┐
 *   │ State 3  │ Yield: ":" (separator after attribute type)
 *   │ ATTR_SEP │
 *   └────┬─────┘
 *        │ (first iteration only, if has stream name)
 *        ▼
 *   ┌──────────┐
 *   │ State 4  │ Yield: "streamname" (e.g., "Zone.Identifier")
 *   │ STREAM   │
 *   └────┬─────┘
 *        │
 *        ▼
 *   ┌──────────┐
 *   │ State 5  │ Yield: ":" (separator before stream name)
 *   │ STRM_SEP │
 *   └────┬─────┘
 *        │
 *        ▼
 *   ┌──────────┐
 *   │ State 6  │ Yield: "filename" (file or directory name)
 *   │ FILENAME │
 *   └────┬─────┘
 *        │
 *        ▼
 *   ┌──────────┐     ┌──────────┐
 *   │ is_root? │─Yes─►│  DONE    │ Set index = nullptr
 *   └────┬─────┘     └──────────┘
 *        │ No
 *        ▼
 *   Move to parent (key = parent_key)
 *   ++iteration
 *   Go to State 0
 * ```
 *
 * ## Example Path Reconstruction
 *
 * For the path `C:\Users\file.txt:Zone.Identifier`:
 *
 * | Iteration | State | Yields | Accumulated (reversed) |
 * |-----------|-------|--------|------------------------|
 * | 0 | 4 | "Zone.Identifier" | "Zone.Identifier" |
 * | 0 | 5 | ":" | ":Zone.Identifier" |
 * | 0 | 6 | "file.txt" | "file.txt:Zone.Identifier" |
 * | 1 | 1 | "\" | "\file.txt:Zone.Identifier" |
 * | 1 | 6 | "Users" | "Users\file.txt:Zone.Identifier" |
 * | 2 | 1 | "\" | "\Users\file.txt:Zone.Identifier" |
 * | 2 | 6 | "C:" | "C:\Users\file.txt:Zone.Identifier" |
 * | 3 | - | (root reached) | Final: reverse to get path |
 *
 * ## Duff's Device Pattern
 *
 * The switch/case labels are interleaved with a for(;;) loop, allowing
 * the function to resume execution at the exact point where it left off.
 * This is a coroutine-like pattern that predates C++20 coroutines.
 *
 * ## Thread Safety
 *
 * ParentIterator is NOT thread-safe. Each thread should use its own iterator
 * instance. The underlying NtfsIndex must be fully constructed before iteration.
 *
 * @note This file is included by ntfs_index_impl.hpp.
 *       Do not include this file directly.
 *
 * @see ntfs_index.hpp for ParentIterator class declaration
 * @see ntfs_index_accessors.hpp for get_file_pointers() used by this iterator
 */

#ifndef UFFS_NTFS_INDEX_PATH_HPP
#define UFFS_NTFS_INDEX_PATH_HPP

#ifndef UFFS_NTFS_INDEX_IMPL_HPP
#error "Do not include ntfs_index_path.hpp directly. Include ntfs_index.hpp instead."
#endif

// ============================================================================
// SECTION: ParentIterator::operator++() Implementation
// ============================================================================
//
// This method implements a state machine using Duff's device pattern to
// iterate through path components from a file up to the root directory.
//
// ## State Machine Overview
//
// The iterator produces path components in this order (for a file with
// a named stream and non-default attribute type):
//
//   State 0: Initialize, get file pointers
//   State 1: Directory separator "\" (if not root and is directory)
//   State 2: Attribute type name (e.g., "$INDEX_ALLOCATION")
//   State 3: Colon separator ":" after attribute type
//   State 4: Stream name (e.g., "Zone.Identifier")
//   State 5: Colon separator ":" before stream name
//   State 6: File/directory name
//   Then: Move to parent directory and repeat from state 0
//
// ## Duff's Device Pattern
//
// The switch/case labels are interleaved with a for(;;) loop, allowing
// the function to resume execution at the exact point where it left off.
// This is a coroutine-like pattern that predates C++20 coroutines.
//
// Example path reconstruction for "C:\Users\file.txt:Zone.Identifier":
//   Iteration 0: "Zone.Identifier" -> ":" -> "file.txt"
//   Iteration 1: "\" -> "Users"
//   Iteration 2: "\" -> "C:"
//   Iteration 3: (root reached, stop)
//
// The caller reverses the accumulated components to get the final path.
//

inline NtfsIndex::ParentIterator& NtfsIndex::ParentIterator::operator++()
{
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wimplicit-fallthrough"
#endif
	switch (state)
	{
		// Outer loop: iterate through parent directories
		for (;;)
		{
	case 0:
		// State 0: Initialize - resolve key to file pointers
		ptrs = index->get_file_pointers(key);

		// Add directory separator if not at root and this is a directory
		if (!is_root())
		{
			if (!ptrs.stream->type_name_id)  // type_name_id == 0 means directory
			{
				result.first = _T("\\");
				result.second = 1;
				result.ascii = false;
				if (result.second)
				{
					state = 1;
					break;  // Yield "\"
				}
				[[fallthrough]];
	case 1:;
			}
		}

		// First iteration only: handle stream/attribute components
		if (!this->iteration)
		{
			// State 2-3: Attribute type name (for non-data attributes)
			if (is_attribute() && ptrs.stream->type_name_id < sizeof(ntfs::attribute_names) / sizeof(*ntfs::attribute_names))
			{
				result.first = ntfs::attribute_names[ptrs.stream->type_name_id].data;
				result.second = ntfs::attribute_names[ptrs.stream->type_name_id].size;
				result.ascii = false;
				if (result.second)
				{
					state = 2;
					break;  // Yield attribute type name
				}
				[[fallthrough]];
	case 2:
				// Colon separator after attribute type
				result.first = _T(":");
				result.second = 1;
				result.ascii = false;
				if (result.second)
				{
					state = 3;
					break;  // Yield ":"
				}
				[[fallthrough]];
	case 3:;
			}

			// State 4: Named stream (e.g., "Zone.Identifier")
			if (ptrs.stream->name.length)
			{
				result.first = &index->names[ptrs.stream->name.offset()];
				result.second = ptrs.stream->name.length;
				result.ascii = ptrs.stream->name.ascii();
				if (result.second)
				{
					state = 4;
					break;  // Yield stream name
				}
				[[fallthrough]];
	case 4:;
			}

			// State 5: Colon before stream name or after attribute
			if (ptrs.stream->name.length || is_attribute())
			{
				result.first = _T(":");
				result.second = 1;
				result.ascii = false;
				if (result.second)
				{
					state = 5;
					break;  // Yield ":"
				}
				[[fallthrough]];
	case 5:;
			}
		}

		// State 6: File/directory name
		if (!this->iteration || !is_root())
		{
			result.first = &index->names[ptrs.link->name.offset()];
			result.second = ptrs.link->name.length;
			result.ascii = ptrs.link->name.ascii();
			if (result.second)
			{
				state = 6;
				break;  // Yield file/directory name
			}
		}
		[[fallthrough]];
	case 6:
		// Check if we've reached the root
		if (is_root())
		{
			this->index = nullptr;  // Signal iteration complete
			break;
		}

		// Move to parent directory and continue
		state = 0;
		key = ptrs.parent();
		++this->iteration;
		}

	default:
		break;
	}
#ifdef __clang__
#pragma clang diagnostic pop
#endif
	return *this;
}

#endif // UFFS_NTFS_INDEX_PATH_HPP

