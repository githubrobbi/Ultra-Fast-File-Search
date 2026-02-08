/**
 * @file ntfs_index_accessors.hpp
 * @brief Accessor methods, lifecycle management, and data structure navigation for NtfsIndex.
 *
 * This file contains the encapsulated access layer for NtfsIndex internals, including:
 * - Constructor, destructor, and initialization lifecycle
 * - Key resolution and path reconstruction
 * - Progress and statistics accessors (with volatile overloads for thread safety)
 * - Linked list navigation for names, streams, and children
 * - Volume configuration getters/setters
 *
 * ## Architecture Overview
 *
 * NtfsIndex stores file metadata in a compact, cache-friendly layout using linked
 * lists embedded in contiguous vectors. This file provides the navigation layer
 * that abstracts the underlying storage from higher-level operations.
 *
 * ## Data Structure Navigation
 *
 * The index uses a two-level lookup with linked lists for variable-length data:
 *
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                         RECORD LOOKUP                                   │
 * │  records_lookup[FRS] ──► slot index ──► records_data[slot]              │
 * └─────────────────────────────────────────────────────────────────────────┘
 *                                              │
 *                                              ▼
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                         RECORD STRUCTURE                                │
 * │  Record {                                                               │
 * │    stdinfo      ──► StandardInfo (timestamps, attributes)               │
 * │    first_name   ──► LinkInfo (inline, first hard link)                  │
 * │    first_stream ──► StreamInfo (inline, default $DATA stream)           │
 * │    first_child  ──► index into childinfos[] (for directories)           │
 * │  }                                                                      │
 * └─────────────────────────────────────────────────────────────────────────┘
 *                    │              │              │
 *                    ▼              ▼              ▼
 * ┌──────────────────┬──────────────┬──────────────────────────────────────┐
 * │   LINKED LISTS   │              │                                      │
 * │                  │              │                                      │
 * │  nameinfos[]     │ streaminfos[]│ childinfos[]                         │
 * │  ┌─────────┐     │ ┌─────────┐  │ ┌─────────┐                          │
 * │  │LinkInfo │     │ │StreamInfo│ │ │ChildInfo│                          │
 * │  │.next ───┼──►  │ │.next ───┼─►│ │.next ───┼──►                       │
 * │  └─────────┘     │ └─────────┘  │ └─────────┘                          │
 * │       │          │      │       │      │                               │
 * │       ▼          │      ▼       │      ▼                               │
 * │  ┌─────────┐     │ ┌─────────┐  │ ┌─────────┐                          │
 * │  │LinkInfo │     │ │StreamInfo│ │ │ChildInfo│                          │
 * │  │.next=~0 │     │ │.next=~0 │  │ │.next=~0 │                          │
 * │  └─────────┘     │ └─────────┘  │ └─────────┘                          │
 * └──────────────────┴──────────────┴──────────────────────────────────────┘
 *
 * Legend:
 *   ~0 (all bits set) = end of linked list (nullptr equivalent)
 *   fast_subscript()  = optimized indexing avoiding imul instruction
 * ```
 *
 * ## Key Concepts
 *
 * - **fast_subscript**: Performance optimization that avoids the `imul` instruction
 *   by using pointer arithmetic directly. Critical for hot paths.
 *
 * - **Volatile accessors**: Methods with `volatile` qualifier use `memory_order_acquire`
 *   for cross-thread visibility during concurrent MFT reading. Non-volatile versions
 *   use `memory_order_relaxed` for better performance when thread safety isn't needed.
 *
 * - **~0 sentinel**: The value `~0` (all bits set) is used as a null/end marker
 *   throughout the linked lists, checked via `!~value` idiom.
 *
 * ## Thread Safety
 *
 * - Progress accessors (`records_so_far`, `total_names_and_streams`, etc.) are
 *   thread-safe via atomic operations with appropriate memory ordering.
 * - `unvolatile()` methods allow safe casting from volatile pointers during
 *   concurrent access from the I/O completion port threads.
 * - Volume configuration setters are NOT thread-safe; they must be called
 *   during initialization before concurrent access begins.
 *
 * ## Usage Example
 *
 * ```cpp
 * // Navigate from a record to all its hard links
 * for (auto* link = index.nameinfo(record); link; link = index.nameinfo(link->next_entry)) {
 *     // Process each hard link name
 * }
 *
 * // Get progress during indexing (from another thread)
 * volatile NtfsIndex* vindex = &index;
 * size_t progress = vindex->records_so_far();  // Uses acquire semantics
 * ```
 *
 * @note This file is included by ntfs_index_impl.hpp.
 *       Do not include this file directly.
 *
 * @see ntfs_index.hpp for the public API and class declaration
 * @see ntfs_record_types.hpp for Record, LinkInfo, StreamInfo, ChildInfo definitions
 * @see type_traits_ext.hpp for fast_subscript and propagate_const
 */

#ifndef UFFS_NTFS_INDEX_ACCESSORS_HPP
#define UFFS_NTFS_INDEX_ACCESSORS_HPP

#ifndef UFFS_NTFS_INDEX_IMPL_HPP
#error "Do not include ntfs_index_accessors.hpp directly. Include ntfs_index.hpp instead."
#endif

// ============================================================================
// SECTION: Key Resolution and Path Reconstruction
// ============================================================================

/**
 * @brief Resolves a key to pointers to the record, link, and stream.
 *
 * Given a key (FRS + name_info + stream_info), returns pointers to the
 * corresponding data structures. This is the primary method for accessing
 * file information from a search result.
 *
 * ## Key Structure
 *
 * A key encodes three pieces of information:
 * - FRS: File Record Segment number (identifies the file)
 * - name_info: Which hard link (0 = first, USHRT_MAX = any)
 * - stream_info: Which data stream (0 = first, USHRT_MAX = default)
 *
 * @param key The key to resolve
 * @return file_pointers Structure containing record, link, and stream pointers
 * @throws std::logic_error if the key cannot be resolved
 */
inline NtfsIndex::file_pointers NtfsIndex::get_file_pointers(key_type key) const
{
	bool wait_for_finish = false;  // has performance penalty
	if (wait_for_finish && WaitForSingleObject(this->_finished_event, 0) == WAIT_TIMEOUT)
	{
		throw std::logic_error("Need to wait for indexing to be finished");
	}

	file_pointers result;
	result.record = nullptr;

	if (~key.frs())  // Check for valid FRS (not ~0)
	{
		Records::value_type const* const fr = this->find(key.frs());

		// Iterate through hard links to find matching name_info
		unsigned short ji = 0;
		for (LinkInfos::value_type const* j = this->nameinfo(fr);
			 j;
			 j = this->nameinfo(j->next_entry), ++ji)
		{
			key_type::name_info_type const name_info = key.name_info();
			if (name_info == USHRT_MAX || ji == name_info)
			{
				// Iterate through streams to find matching stream_info
				unsigned short ki = 0;
				for (StreamInfos::value_type const* k = this->streaminfo(fr);
					 k;
					 k = this->streaminfo(k->next_entry), ++ki)
				{
					key_type::stream_info_type const stream_info = key.stream_info();

					// Match: USHRT_MAX means "default stream" (type_name_id == 0)
					if (stream_info == USHRT_MAX ? !k->type_name_id : ki == stream_info)
					{
						result.record = fr;
						result.link = j;
						result.stream = k;
						goto STOP;
					}
				}
			}
		}

	STOP:
		if (!result.record)
		{
			throw std::logic_error("could not find a file attribute");
		}
	}

	return result;
}

/**
 * @brief Reconstructs the full path for a file from its key.
 *
 * Uses ParentIterator to walk up the directory tree, collecting path
 * components and assembling them into a full path string.
 *
 * @param key        The key identifying the file
 * @param result     Output string to append the path to
 * @param name_only  If true, return only the file name (not full path)
 * @param attributes Optional output for file attributes
 * @return Number of characters added to result
 *
 * @see ParentIterator for the path traversal implementation
 */
inline size_t NtfsIndex::get_path(key_type key,
	std::tvstring& result,
	bool const name_only,
	unsigned int* attributes) const
{
	size_t const old_size = result.size();

	// Walk up the directory tree using ParentIterator
	for (ParentIterator pi(this, key); pi.next() && !(name_only && pi.icomponent());)
	{
		// Capture attributes from the first component (the file itself)
		if (attributes)
		{
			*attributes = pi.attributes();
			attributes = nullptr;
		}

		TCHAR const* const s = static_cast<TCHAR const*>(pi->first);

		// Skip single-dot components (current directory) unless name_only
		if (name_only || !(pi->second == 1 && (pi->ascii
			? *static_cast<char const*>(static_cast<void const*>(s))
			: *s) == _T('.')))
		{
			// Append component in reverse (will be reversed at end)
			append_directional(result, s, pi->second, pi->ascii ? -1 : 0, true);
		}
	}

	// Reverse the path (we built it backwards by walking up the tree)
	std::reverse(result.begin() + static_cast<ptrdiff_t>(old_size), result.end());
	return result.size() - old_size;
}

/**
 * @brief Returns size information for a file/stream identified by key.
 *
 * @param key The key identifying the file and stream
 * @return Reference to the StreamInfo containing size data
 */
inline NtfsIndex::size_info const& NtfsIndex::get_sizes(key_type const& key) const
{
	StreamInfos::value_type const* k;
	Records::value_type const* const fr = this->find(key.frs());

	// Find the stream matching stream_info index
	unsigned short ki = 0;
	for (k = this->streaminfo(fr); k; k = this->streaminfo(k->next_entry), ++ki)
	{
		if (ki == key.stream_info())
		{
			break;
		}
	}

	assert(k);
	return *k;
}

/**
 * @brief Returns standard information (timestamps, attributes) for a file.
 *
 * @param frn File Record Number (FRS)
 * @return Reference to the StandardInfo structure
 */
inline NtfsIndex::standard_info const& NtfsIndex::get_stdinfo(unsigned int const frn) const
{
	return this->find(frn)->stdinfo;
}

// ============================================================================
// SECTION: Constructor and Destructor
// ============================================================================

/**
 * @brief Constructs an NtfsIndex for the specified volume.
 *
 * Initializes all data structures but does not start indexing.
 * Call init() followed by the MFT reader to populate the index.
 *
 * @param value Root path of the volume (e.g., "C:\\")
 */
inline NtfsIndex::NtfsIndex(std::tvstring value)
	: _root_path(value)
	, _finished_event(CreateEvent(nullptr, TRUE, FALSE, nullptr))  // Manual reset event
	, _finished()
	, _total_names_and_streams(0)
	, _cancelled(false)
	, _records_so_far(0)
	, _preprocessed_so_far(0)
	, _perf_reports_circ(1 << 6)  // 64-entry circular buffer for speed tracking
	, _perf_avg_speed(Speed())
	, _reserved_clusters(0)
{
}

inline NtfsIndex::~NtfsIndex()
{
}

// ============================================================================
// SECTION: Initialization and Lifecycle
// ============================================================================

/**
 * @brief Checks if init() has been called.
 * @return true if init() was called, false otherwise
 */
inline bool NtfsIndex::init_called() const noexcept
{
	return this->_init_called;
}

/**
 * @brief Initializes the index by opening the volume handle.
 *
 * Opens the volume for direct access and verifies it's an NTFS volume.
 * Must be called before starting the MFT reader.
 *
 * @throws Windows exception if volume cannot be opened or is not NTFS
 */
inline void NtfsIndex::init()
{
	this->_init_called = true;

	// Build device path: "C:" -> "\\.\C:"
	std::tvstring path_name = this->_root_path;
	deldirsep(path_name);
	if (!path_name.empty() && *path_name.begin() != _T('\\') && *path_name.begin() != _T('/'))
	{
		path_name.insert(static_cast<size_t>(0), _T("\\\\.\\"));
	}

	// Open volume for direct access (requires admin privileges)
	HANDLE const h = CreateFile(path_name.c_str(),
		FILE_READ_DATA | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
	CheckAndThrow(h != INVALID_HANDLE_VALUE);
	Handle volume(h);

	// Query file system type to verify it's NTFS
	winnt::IO_STATUS_BLOCK iosb;
	struct : winnt::FILE_FS_ATTRIBUTE_INFORMATION
	{
		unsigned char buf[MAX_PATH];
	} info = {};

	winnt::NTSTATUS status = winnt::NtQueryVolumeInformationFile(
		volume.value, &iosb, &info, sizeof(info), 5);
	if (status != 0)
	{
		CppRaiseException(winnt::RtlNtStatusToDosError(status));
	}

	// Verify file system is NTFS
	if (info.FileSystemNameLength != 4 * sizeof(*info.FileSystemName) ||
		std::char_traits<TCHAR>::compare(info.FileSystemName, _T("NTFS"), 4))
	{
		CppRaiseException(ERROR_UNRECOGNIZED_VOLUME);
	}

	// Optional: Set low I/O priority (disabled by default)
	if (false)
	{
		IoPriority::set(reinterpret_cast<uintptr_t>(volume.value), winnt::IoPriorityLow);
	}

	// Store volume handle and record start time
	volume.swap(this->_volume);
	this->_tbegin = clock();
}

/**
 * @brief Marks indexing as finished with the given result code.
 * @param result Result code (0 = success, non-zero = error)
 */
inline void NtfsIndex::set_finished(unsigned int const& result)
{
	SetEvent(this->_finished_event);
	this->_finished = result;
}

// ============================================================================
// SECTION: Volatile Helpers
// ============================================================================
// These methods allow safe access to the index from volatile pointers,
// which are used during concurrent MFT reading.

/// @brief Casts away volatile qualifier for non-const access.
inline NtfsIndex* NtfsIndex::unvolatile() volatile noexcept
{
	return const_cast<NtfsIndex*>(this);
}

/// @brief Casts away volatile qualifier for const access.
inline NtfsIndex const* NtfsIndex::unvolatile() const volatile noexcept
{
	return const_cast<NtfsIndex const*>(this);
}

// ============================================================================
// SECTION: Progress and Statistics Accessors
// ============================================================================
// These methods provide thread-safe access to progress counters and
// statistics. Volatile overloads use acquire semantics for cross-thread
// visibility; non-volatile overloads use relaxed semantics for performance.

/// @brief Returns total count of (name, stream) pairs indexed.
inline size_t NtfsIndex::total_names_and_streams() const noexcept
{
	return this->_total_names_and_streams.load(atomic_namespace::memory_order_relaxed);
}

/// @brief Returns total count of (name, stream) pairs indexed (volatile).
inline size_t NtfsIndex::total_names_and_streams() const volatile noexcept
{
	return this->_total_names_and_streams.load(atomic_namespace::memory_order_acquire);
}

/// @brief Returns count of hard links (additional names beyond first).
inline size_t NtfsIndex::total_names() const noexcept
{
	return this->nameinfos.size();
}

/// @brief Returns expected number of MFT records (for progress calculation).
inline size_t NtfsIndex::expected_records() const noexcept
{
	return this->_expected_records;
}

/// @brief Returns number of streams preprocessed so far (volatile).
inline size_t NtfsIndex::preprocessed_so_far() const volatile noexcept
{
	return this->_preprocessed_so_far.load(atomic_namespace::memory_order_acquire);
}

/// @brief Returns number of streams preprocessed so far.
inline size_t NtfsIndex::preprocessed_so_far() const noexcept
{
	return this->_preprocessed_so_far.load(atomic_namespace::memory_order_relaxed);
}

/// @brief Returns number of MFT records processed so far (volatile).
inline size_t NtfsIndex::records_so_far() const volatile noexcept
{
	return this->_records_so_far.load(atomic_namespace::memory_order_acquire);
}

/// @brief Returns number of MFT records processed so far.
inline size_t NtfsIndex::records_so_far() const noexcept
{
	return this->_records_so_far.load(atomic_namespace::memory_order_relaxed);
}

/// @brief Returns the volume handle.
inline void* NtfsIndex::volume() const volatile noexcept
{
	return this->_volume.value;
}

/// @brief Returns the mutex for thread-safe access.
inline atomic_namespace::recursive_mutex& NtfsIndex::get_mutex() const volatile noexcept
{
	return this->unvolatile()->_mutex;
}

/// @brief Returns cumulative I/O speed statistics.
inline Speed NtfsIndex::speed() const volatile noexcept
{
	Speed total;
	total = this->_perf_avg_speed.load(atomic_namespace::memory_order_acquire);
	return total;
}

/// @brief Returns the root path of the indexed volume.
inline std::tvstring const& NtfsIndex::root_path() const volatile noexcept
{
	return const_cast<std::tvstring const&>(this->_root_path);
}

/// @brief Returns the finish result code (0 if not finished or success).
inline unsigned int NtfsIndex::get_finished() const volatile noexcept
{
	return this->_finished.load();
}

/// @brief Returns true if indexing was cancelled.
inline bool NtfsIndex::cancelled() const volatile noexcept
{
	this_type const* const me = this->unvolatile();
	return me->_cancelled.load(atomic_namespace::memory_order_acquire);
}

/// @brief Requests cancellation of the indexing operation.
inline void NtfsIndex::cancel() volatile noexcept
{
	this_type* const me = this->unvolatile();
	me->_cancelled.store(true, atomic_namespace::memory_order_release);
}

/// @brief Returns the Windows event handle signaled when indexing finishes.
inline uintptr_t NtfsIndex::finished_event() const noexcept
{
	return reinterpret_cast<uintptr_t>(this->_finished_event.value);
}

// ============================================================================
// SECTION: Capacity Reservation
// ============================================================================

/**
 * @brief Pre-allocates memory for the expected number of MFT records.
 *
 * Called before indexing starts to minimize reallocations during parsing.
 * The reservation sizes are based on empirical analysis of typical NTFS
 * volumes:
 * - nameinfos: records + 6.25% (for hard links)
 * - streaminfos: records / 4 (most files have 1 stream, dirs have 0)
 * - childinfos: records * 1.5 (average directory has ~1.5 children)
 * - names: records * 23 (average file name ~23 characters)
 *
 * @param records Expected number of MFT records
 */
inline void NtfsIndex::reserve(unsigned int records)
{
	this->_expected_records = records;
	try
	{
		if (this->records_lookup.size() < records)
		{
			this->nameinfos.reserve(records + records / 16);
			this->streaminfos.reserve(records / 4);
			this->childinfos.reserve(records + records / 2);
			this->names.reserve(records * 23);
			this->records_lookup.resize(records, ~RecordsLookup::value_type());
			this->records_data.reserve(records + records / 4);
		}
	}
	catch (std::bad_alloc&)
	{
		// Silently continue with smaller allocation
	}
}

// ============================================================================
// SECTION: Private Helper Methods
// ============================================================================
// These methods provide low-level access to the index data structures.
// They use a two-level lookup: records_lookup[FRS] -> index into records_data.
//
// Data Structure Layout:
// ┌─────────────────┐     ┌──────────────────┐
// │ records_lookup  │────>│ records_data     │
// │ [FRS -> index]  │     │ [RecordInfo...]  │
// └─────────────────┘     └──────────────────┘
//                                │
//                    ┌───────────┼───────────┐
//                    v           v           v
//              ┌──────────┐ ┌──────────┐ ┌──────────┐
//              │childinfos│ │nameinfos │ │streaminfos│
//              └──────────┘ └──────────┘ └──────────┘

/**
 * @brief Gets or creates a record entry for the given FRS.
 *
 * If the FRS doesn't exist in the lookup table, creates a new entry.
 * The existing_to_revalidate parameter handles iterator invalidation
 * when the records_data vector is resized.
 *
 * @param frs File Record Segment number
 * @param existing_to_revalidate Optional iterator to revalidate after resize
 * @return Iterator to the record entry
 */
inline NtfsIndex::Records::iterator NtfsIndex::at(size_t const frs,
	Records::iterator* const existing_to_revalidate)
{
	// Expand lookup table if needed
	if (frs >= this->records_lookup.size())
	{
		this->records_lookup.resize(frs + 1, ~RecordsLookup::value_type());
	}

	RecordsLookup::iterator const k = this->records_lookup.begin() + static_cast<ptrdiff_t>(frs);

	// If slot is empty (~0), create new record entry
	if (!~*k)
	{
		// Save iterator offset before potential reallocation
		ptrdiff_t const j = (existing_to_revalidate ? *existing_to_revalidate : this->records_data.end())
			- this->records_data.begin();

		*k = static_cast<unsigned int>(this->records_data.size());
		this->records_data.resize(this->records_data.size() + 1);

		// Restore iterator after reallocation
		if (existing_to_revalidate)
		{
			*existing_to_revalidate = this->records_data.begin() + j;
		}
	}

	return this->records_data.begin() + static_cast<ptrdiff_t>(*k);
}

/**
 * @brief Finds a record by FRS (template implementation).
 *
 * Uses fast_subscript to avoid multiplication instruction for performance.
 * Returns pointer past end if FRS is out of range.
 *
 * @tparam Me NtfsIndex or const NtfsIndex
 * @param me Pointer to the index
 * @param frs File Record Segment number to find
 * @return Pointer to the record, or past-end pointer if not found
 */
template <class Me>
inline typename propagate_const<Me, NtfsIndex::Records::value_type>::type*
NtfsIndex::_find(Me* const me, key_type_internal::frs_type const frs)
{
	typedef typename propagate_const<Me, Records::value_type>::type* pointer_type;
	pointer_type result;

	if (frs < me->records_lookup.size())
	{
		RecordsLookup::value_type const islot = me->records_lookup[frs];
		// fast_subscript avoids 'imul' instruction for better performance
		result = fast_subscript(me->records_data.begin(), islot);
	}
	else
	{
		// Return past-end pointer for out-of-range FRS
		result = me->records_data.empty() ? nullptr : &*(me->records_data.end() - 1) + 1;
	}
	return result;
}

/// @brief Finds a record by FRS (mutable version).
inline NtfsIndex::Records::value_type* NtfsIndex::find(key_type_internal::frs_type const frs)
{
	return this->_find(this, frs);
}

/// @brief Finds a record by FRS (const version).
inline NtfsIndex::Records::value_type const* NtfsIndex::find(key_type_internal::frs_type const frs) const
{
	return this->_find(this, frs);
}

// ============================================================================
// childinfo: Access child entries of a directory
// ============================================================================

/// @brief Gets first child of a record (mutable).
inline NtfsIndex::ChildInfos::value_type* NtfsIndex::childinfo(Records::value_type* const i)
{
	return this->childinfo(i->first_child);
}

/// @brief Gets first child of a record (const).
inline NtfsIndex::ChildInfos::value_type const* NtfsIndex::childinfo(Records::value_type const* const i) const
{
	return this->childinfo(i->first_child);
}

/// @brief Gets child entry by index (mutable). Returns nullptr if index is ~0.
inline NtfsIndex::ChildInfos::value_type* NtfsIndex::childinfo(ChildInfo::next_entry_type const i)
{
	return !~i ? nullptr : fast_subscript(this->childinfos.begin(), i);
}

/// @brief Gets child entry by index (const). Returns nullptr if index is ~0.
inline NtfsIndex::ChildInfos::value_type const* NtfsIndex::childinfo(ChildInfo::next_entry_type const i) const
{
	return !~i ? nullptr : fast_subscript(this->childinfos.begin(), i);
}

// ============================================================================
// nameinfo: Access hard link (name) entries
// ============================================================================
// Files can have multiple names (hard links). The first name is stored
// inline in the record (first_name), additional names are in nameinfos.

/// @brief Gets name entry by index (mutable). Returns nullptr if index is ~0.
inline NtfsIndex::LinkInfos::value_type* NtfsIndex::nameinfo(LinkInfo::next_entry_type const i)
{
	return !~i ? nullptr : fast_subscript(this->nameinfos.begin(), i);
}

/// @brief Gets name entry by index (const). Returns nullptr if index is ~0.
inline NtfsIndex::LinkInfos::value_type const* NtfsIndex::nameinfo(LinkInfo::next_entry_type const i) const
{
	return !~i ? nullptr : fast_subscript(this->nameinfos.begin(), i);
}

/// @brief Gets first name of a record (mutable). Returns nullptr if no name.
inline NtfsIndex::LinkInfos::value_type* NtfsIndex::nameinfo(Records::value_type* const i)
{
	return ~i->first_name.name.offset() ? &i->first_name : nullptr;
}

/// @brief Gets first name of a record (const). Returns nullptr if no name.
inline NtfsIndex::LinkInfos::value_type const* NtfsIndex::nameinfo(Records::value_type const* const i) const
{
	return ~i->first_name.name.offset() ? &i->first_name : nullptr;
}

// ============================================================================
// streaminfo: Access data stream entries
// ============================================================================
// Files can have multiple data streams (named streams). The first/default
// stream is stored inline in the record (first_stream), additional streams
// are in streaminfos.

/// @brief Gets stream entry by index (mutable). Returns nullptr if index is ~0.
inline NtfsIndex::StreamInfos::value_type* NtfsIndex::streaminfo(StreamInfo::next_entry_type const i)
{
	return !~i ? nullptr : fast_subscript(this->streaminfos.begin(), i);
}

/// @brief Gets stream entry by index (const). Returns nullptr if index is ~0.
inline NtfsIndex::StreamInfos::value_type const* NtfsIndex::streaminfo(StreamInfo::next_entry_type const i) const
{
	return !~i ? nullptr : fast_subscript(this->streaminfos.begin(), i);
}

/// @brief Gets first stream of a record (mutable). Returns nullptr if no stream.
inline NtfsIndex::StreamInfos::value_type* NtfsIndex::streaminfo(Records::value_type* const i)
{
	assert(~i->first_stream.name.offset() || (!i->first_stream.name.length && !i->first_stream.length));
	return ~i->first_stream.name.offset() ? &i->first_stream : nullptr;
}

/// @brief Gets first stream of a record (const). Returns nullptr if no stream.
inline NtfsIndex::StreamInfos::value_type const* NtfsIndex::streaminfo(Records::value_type const* const i) const
{
	assert(~i->first_stream.name.offset() || (!i->first_stream.name.length && !i->first_stream.length));
	return ~i->first_stream.name.offset() ? &i->first_stream : nullptr;
}

// ============================================================================
// SECTION: Volume Configuration Accessors
// ============================================================================
// These accessors provide encapsulated access to NTFS volume parameters
// that are set during initialization and used throughout indexing.

/// @brief Returns number of clusters reserved for MFT zone.
inline long long NtfsIndex::reserved_clusters() const volatile noexcept
{
	return _reserved_clusters.load(std::memory_order_relaxed);
}

/// @brief Returns starting cluster of MFT zone.
inline long long NtfsIndex::mft_zone_start() const noexcept
{
	return _mft_zone_start;
}

/// @brief Returns ending cluster of MFT zone.
inline long long NtfsIndex::mft_zone_end() const noexcept
{
	return _mft_zone_end;
}

/// @brief Returns cluster size in bytes (typically 4096).
inline unsigned int NtfsIndex::cluster_size() const noexcept
{
	return _cluster_size;
}

/// @brief Returns MFT record size in bytes (typically 1024).
inline unsigned int NtfsIndex::mft_record_size() const volatile noexcept
{
	return _mft_record_size;
}

/// @brief Returns total capacity of MFT in records.
inline unsigned int NtfsIndex::mft_capacity() const volatile noexcept
{
	return _mft_capacity;
}

/// @brief Sets number of clusters reserved for MFT zone.
inline void NtfsIndex::set_reserved_clusters(long long value) volatile noexcept
{
	_reserved_clusters.store(value, std::memory_order_relaxed);
}

/// @brief Sets starting cluster of MFT zone.
inline void NtfsIndex::set_mft_zone_start(long long value) noexcept
{
	_mft_zone_start = value;
}

/// @brief Sets ending cluster of MFT zone.
inline void NtfsIndex::set_mft_zone_end(long long value) noexcept
{
	_mft_zone_end = value;
}

/// @brief Sets cluster size in bytes.
inline void NtfsIndex::set_cluster_size(unsigned int value) noexcept
{
	_cluster_size = value;
}

/// @brief Sets MFT record size in bytes.
inline void NtfsIndex::set_mft_record_size(unsigned int value) noexcept
{
	_mft_record_size = value;
}

/// @brief Sets total capacity of MFT in records.
inline void NtfsIndex::set_mft_capacity(unsigned int value) noexcept
{
	_mft_capacity = value;
}

// ============================================================================
// SECTION: Performance Tracking
// ============================================================================

/**
 * @brief Reports I/O speed for performance monitoring.
 *
 * Records the speed of an I/O operation for performance tracking and
 * averaging. Uses a circular buffer to maintain recent speed samples.
 *
 * @param size Number of bytes transferred
 * @param tfrom Start time (clock_t)
 * @param tto End time (clock_t)
 *
 * @note Thread-safe via atomic operations on _perf_avg_speed.
 */
inline void NtfsIndex::report_speed(unsigned long long const size,
	clock_t const tfrom,
	clock_t const tto)
{
	clock_t const duration = tto - tfrom;
	Speed const speed(size, duration);
	this->_perf_avg_speed.fetch_add(speed, atomic_namespace::memory_order_acq_rel);
	Speed const prev = this->_perf_reports_circ[this->_perf_reports_begin];
	this->_perf_reports_circ[this->_perf_reports_begin] = speed;
	this->_perf_reports_begin = (this->_perf_reports_begin + 1) % this->_perf_reports_circ.size();
}

#endif // UFFS_NTFS_INDEX_ACCESSORS_HPP

