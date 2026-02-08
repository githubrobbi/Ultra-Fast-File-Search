/**
 * @file ntfs_index_matcher.hpp
 * @brief Callback-based recursive directory traversal for NtfsIndex search operations.
 *
 * This file contains the Matcher template struct, which implements depth-first
 * traversal of the NTFS directory tree with user-defined callbacks for matching
 * files, streams, and attributes.
 *
 * ## Architecture Overview
 *
 * The Matcher provides a flexible traversal mechanism that:
 * 1. Walks the directory tree starting from any FRS (typically root, FRS 5)
 * 2. Calls a user-provided callback for each file/stream encountered
 * 3. Uses the callback's return value to control recursion depth
 * 4. Supports three matching modes: paths, streams, and attributes
 *
 * ## Traversal Order
 *
 * ```
 *                    Root (FRS 5)
 *                         │
 *          ┌──────────────┼──────────────┐
 *          ▼              ▼              ▼
 *       Users/        Program Files/   Windows/
 *          │              │              │
 *     ┌────┴────┐    ┌────┴────┐    ┌────┴────┐
 *     ▼         ▼    ▼         ▼    ▼         ▼
 *   Admin/   Public/ App1/   App2/ System32/ ...
 *
 * Traversal: Depth-first, left-to-right
 * Each node: Process all hard links → Process all streams → Recurse children
 * ```
 *
 * ## Callback Semantics
 *
 * The callback function `F` must have the signature:
 * ```cpp
 * ptrdiff_t callback(
 *     TCHAR const* name,    // File/stream name (or full path if match_paths)
 *     size_t length,        // Length of name in characters
 *     bool ascii,           // True if name is ASCII-compressed
 *     key_type key,         // Key identifying this file/stream
 *     size_t depth          // Current recursion depth (0 = root)
 * );
 * ```
 *
 * Return value semantics:
 * - **> 0**: Continue traversing into children (for directories)
 * - **≤ 0**: Skip children, continue with siblings
 *
 * ## Matching Modes
 *
 * | Mode | Description | Example Output |
 * |------|-------------|----------------|
 * | match_paths | Include full path | `C:\Users\file.txt` |
 * | match_streams | Include stream names | `file.txt:Zone.Identifier` |
 * | match_attributes | Include non-$DATA attrs | `$MFT::$BITMAP` |
 *
 * ## Special Handling
 *
 * - **Root Directory (FRS 5)**: Processed specially to handle volume label
 * - **Volume Label (FRS 6)**: Child of root, processed before root's children
 * - **System Metadata (FRS 0-15)**: Skipped unless match_attributes is true
 *
 * ## Thread Safety
 *
 * The Matcher is designed for single-threaded use. It maintains mutable state
 * (path buffer, depth counter) during traversal. For concurrent searches,
 * create separate Matcher instances per thread.
 *
 * ## Usage Example
 *
 * ```cpp
 * // Find all .txt files
 * std::tvstring path;
 * size_t count = 0;
 * auto callback = [&count](TCHAR const* name, size_t len, bool ascii,
 *                          NtfsIndex::key_type key, size_t depth) -> ptrdiff_t {
 *     std::tvstring_view sv(name, len);
 *     if (sv.size() >= 4 && sv.substr(sv.size() - 4) == _T(".txt")) {
 *         ++count;
 *     }
 *     return 1;  // Continue into children
 * };
 *
 * NtfsIndex::Matcher<decltype(callback)> matcher{
 *     &index,           // me
 *     callback,         // func
 *     true,             // match_paths
 *     false,            // match_streams
 *     false,            // match_attributes
 *     &path,            // path buffer
 *     0,                // basename_index_in_path
 *     {},               // name
 *     0                 // depth
 * };
 * matcher(NtfsIndex::kRootFRS);
 * ```
 *
 * @note This file is included by ntfs_index_impl.hpp.
 *       Do not include this file directly.
 *
 * @see ntfs_index.hpp for the NtfsIndex class declaration
 * @see ntfs_record_types.hpp for Record, LinkInfo, StreamInfo, ChildInfo
 * @see append_directional.hpp for path building utilities
 */

#ifndef UFFS_NTFS_INDEX_MATCHER_HPP
#define UFFS_NTFS_INDEX_MATCHER_HPP

#ifndef UFFS_NTFS_INDEX_IMPL_HPP
#error "Do not include ntfs_index_matcher.hpp directly. Include ntfs_index.hpp instead."
#endif

// ============================================================================
// SECTION: Matcher Template
// ============================================================================
//
// The Matcher template implements recursive file system traversal for search
// operations. It walks the directory tree depth-first, calling a user-provided
// function for each file/stream that matches the search criteria.
//
// ## Template Parameter
//
// @tparam F Callable type with signature:
//           ptrdiff_t(TCHAR const* name, size_t length, bool ascii,
//                     key_type key, size_t depth)
//           Returns >0 to continue traversing into children, 0 to skip.
//
// ## Traversal Algorithm
//
// 1. Start at a given FRS (typically root directory, FRS 5)
// 2. For each hard link (name) of the file:
//    a. Build the path prefix
//    b. For each stream of the file:
//       - Build full name (path + stream name + attribute type)
//       - Call func() with the name and key
//       - If func() returns >0, recurse into children
// 3. For each child of the directory, repeat from step 2
//
// ## Matching Modes
//
// - match_paths: Include full path in match (e.g., "C:\Users\file.txt")
// - match_streams: Include stream names (e.g., "file.txt:Zone.Identifier")
// - match_attributes: Include non-data attributes (e.g., "$INDEX_ALLOCATION")
//

template <class F>
struct NtfsIndex::Matcher
{
	NtfsIndex const* me;           ///< The index being searched
	F func;                        ///< User callback function
	bool match_paths;              ///< Include full paths in matching
	bool match_streams;            ///< Include stream names in matching
	bool match_attributes;         ///< Include non-data attributes
	std::tvstring* path;           ///< Buffer for building paths
	size_t basename_index_in_path; ///< Index where file name starts in path
	NameInfo name;                 ///< Current file name info
	size_t depth;                  ///< Current recursion depth

	/**
	 * @brief Entry point: process all names of a file record.
	 *
	 * Iterates through all hard links (names) of the file and processes
	 * each one with its streams.
	 *
	 * @param frs File Record Segment number to process
	 */
	void operator()(key_type::frs_type const frs)
	{
		if (frs < me->records_lookup.size())
		{
			TCHAR const dirsep = getdirsep();
			std::tvstring temp;
			Records::value_type const* const i = me->find(frs);

			// Process each hard link (name) of this file
			unsigned short ji = 0;
			for (LinkInfos::value_type const* j = me->nameinfo(i); j; j = me->nameinfo(j->next_entry), ++ji)
			{
				size_t const old_basename_index_in_path = basename_index_in_path;
				basename_index_in_path = path->size();

				// Build path prefix: directory separator + name
				temp.clear();
				append_directional(temp, &dirsep, 1, 0);
				if (!(match_paths && frs == kRootFRS))
				{
					append_directional(temp, &me->names[j->name.offset()], j->name.length, j->name.ascii() ? -1 : 0);
				}

				// Process this name with all its streams
				this->operator()(frs, ji, temp.data(), temp.size());
				basename_index_in_path = old_basename_index_in_path;
			}
		}
	}

	/**
	 * @brief Process a specific name of a file with all its streams.
	 *
	 * For each stream, builds the full name (including stream name and
	 * attribute type if applicable), calls the user function, and
	 * recursively processes children if the function returns >0.
	 *
	 * @param frs File Record Segment number
	 * @param name_info Index of the hard link being processed
	 * @param stream_prefix Path prefix to prepend
	 * @param stream_prefix_size Length of prefix
	 */
	void operator()(key_type::frs_type const frs, key_type::name_info_type const name_info,
		TCHAR const stream_prefix[], size_t const stream_prefix_size)
	{
		bool const match_paths_or_streams = match_paths || match_streams || match_attributes;
		bool const buffered_matching = stream_prefix_size || match_paths_or_streams;

		// Skip system metadata records (except root and user files)
		if (frs < me->records_lookup.size() && (frs == kRootFRS || frs >= kFirstUserFRS || this->match_attributes))
		{
			Records::value_type const* const fr = me->find(frs);
			key_type new_key(frs, name_info, 0);
			ptrdiff_t traverse = 0;

			// Process each stream of this file
			for (StreamInfos::value_type const* k = me->streaminfo(fr); k;
				k = me->streaminfo(k->next_entry), new_key.stream_info(new_key.stream_info() + 1))
			{
				assert(k->name.offset() <= me->names.size());

				// Check if this is a non-data attribute
				bool const is_attribute = k->type_name_id &&
					(k->type_name_id << (CHAR_BIT / 2)) != static_cast<int>(ntfs::AttributeTypeCode::AttributeData);
				if (!match_attributes && is_attribute)
				{
					continue;  // Skip non-data attributes unless requested
				}

				size_t const old_size = path->size();

				// Append stream prefix (directory separator + name)
				if (stream_prefix_size)
				{
					path->append(stream_prefix, stream_prefix_size);
				}

				// Add trailing backslash for directories
				if (match_paths_or_streams)
				{
					if ((fr->stdinfo.attributes() & FILE_ATTRIBUTE_DIRECTORY) && frs != kRootFRS)
					{
						path->push_back(_T('\\'));
					}
				}

				// Append stream name and attribute type
				if (match_streams || match_attributes)
				{
					// Named stream: ":streamname"
					if (k->name.length)
					{
						path->push_back(_T(':'));
						append_directional(*path, k->name.length ? &me->names[k->name.offset()] : nullptr,
							k->name.length, k->name.ascii() ? -1 : 0);
					}

					// Non-data attribute: "::$ATTRIBUTE_TYPE"
					if (is_attribute)
					{
						if (!k->name.length)
						{
							path->push_back(_T(':'));
						}
						path->push_back(_T(':'));
						path->append(ntfs::attribute_names[k->type_name_id].data,
							ntfs::attribute_names[k->type_name_id].size);
					}
				}

				// Determine what to pass to the callback
				bool ascii;
				size_t name_offset, name_length;
				if (buffered_matching)
				{
					// Use buffered path
					name_offset = match_paths ? 0 : static_cast<unsigned int>(basename_index_in_path);
					name_length = path->size() - name_offset;
					ascii = false;
				}
				else
				{
					// Use direct name reference (faster)
					name_offset = name.offset();
					name_length = name.length;
					ascii = name.ascii();
				}

				// Call user function (skip root's default stream at top level)
				if (frs != kRootFRS || ((depth > 0) ^ (k->type_name_id == 0)))
				{
					traverse += func(
						(buffered_matching ? path->data() : &*me->names.begin()) + static_cast<ptrdiff_t>(name_offset),
						name_length, ascii, new_key, depth);
				}

				// Restore path buffer
				if (buffered_matching)
				{
					path->erase(old_size, path->size() - old_size);
				}
			}

			// --------------------------------------------------------
			// Recursive Descent into Children
			// --------------------------------------------------------
			// If the callback returned >0, traverse into child directories.
			// Skip root directory's children at depth 0 (they're processed
			// separately to handle the volume label).
			//
			if ((frs != kRootFRS || depth == 0) && traverse > 0)
			{
				// Save state for restoration after recursion
				size_t const old_size = path->size();
				NameInfo const old_name = name;
				size_t const old_basename_index_in_path = basename_index_in_path;
				++depth;

				if (buffered_matching)
				{
					if (match_paths_or_streams)
					{
						path->push_back(_T('\\'));  // Add directory separator
					}
					basename_index_in_path = path->size();
				}

				// Iterate through all children of this directory
				unsigned short ii = 0;
				for (ChildInfos::value_type const* i = me->childinfo(fr);
					i && ~i->record_number;
					i = me->childinfo(i->next_entry), ++ii)
				{
					unsigned short name_index = i->name_index;
					unsigned int record_number = i->record_number;

					// Special handling: process root directory after volume label
					// The volume label (FRS 6) is a child of root, but root itself
					// needs to be processed at depth 1 for proper path building.
					bool process_root_after = false;
					do
					{
						Records::value_type const* const fr2 = me->find(record_number);
						unsigned short ji_target = name_index;
						unsigned short ji = 0;

						// Find the hard link that points to this parent
						for (LinkInfos::value_type const* j = me->nameinfo(fr2); j;
							j = me->nameinfo(j->next_entry), ++ji)
						{
							if (j->parent == frs && ji == ji_target)
							{
								// Append child name to path
								if (buffered_matching)
								{
									append_directional(*path, &me->names[j->name.offset()],
										j->name.length, j->name.ascii() ? -1 : 0);
								}
								name = j->name;

								// Recurse into child
								this->operator()(static_cast<key_type::frs_type>(record_number), ji, nullptr, 0);

								// Remove child name from path
								if (buffered_matching)
								{
									path->erase(path->end() - static_cast<ptrdiff_t>(j->name.length), path->end());
								}
							}
						}

						// After processing volume label, also process root directory
						if (record_number == kVolumeFRS && depth == 1)
						{
							name_index = 0;
							record_number = kRootFRS;
							process_root_after = true;
						}
						else
						{
							process_root_after = false;
						}
					} while (process_root_after);
				}

				// Restore state after recursion
				--depth;
				basename_index_in_path = old_basename_index_in_path;
				name = old_name;
				if (buffered_matching)
				{
					path->erase(old_size, path->size() - old_size);
				}
			}
		}
	}
};

#endif // UFFS_NTFS_INDEX_MATCHER_HPP

