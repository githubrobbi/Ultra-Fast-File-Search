/**
 * @file ntfs_index_load.hpp
 * @brief Core MFT parsing and index building implementation.
 *
 * This file contains the preload_concurrent() and load() methods that
 * transform raw MFT data into the searchable in-memory index.
 *
 * ## Architecture Overview
 *
 * The MFT loading process has two phases:
 *
 * 1. **Preload Phase** (preload_concurrent):
 *    - Called from multiple threads concurrently
 *    - Applies multi-sector fixup to validate records
 *    - Pre-allocates vectors to avoid reallocations during load
 *
 * 2. **Load Phase** (load):
 *    - Called sequentially with ordered MFT chunks
 *    - Parses NTFS attributes from each record
 *    - Builds parent-child relationships
 *    - Runs preprocessor to calculate directory sizes
 *
 * ## Processing Pipeline
 *
 * ```
 *   MFT Reader                    NtfsIndex
 *       |                             |
 *       |  preload_concurrent()       |
 *       |  (concurrent, unordered)    |
 *       |---------------------------->|  Fixup + pre-allocate
 *       |                             |
 *       |  load()                     |
 *       |  (sequential, ordered)      |
 *       |---------------------------->|  Parse attributes
 *       |                             |  Build relationships
 *       |                             |  Run preprocessor
 *       |                             |
 *       |                        [Index Ready]
 * ```
 *
 * ## Multi-Sector Fixup
 *
 * NTFS uses a multi-sector protection scheme where the last two bytes of each
 * 512-byte sector are replaced with a sequence number. The unfixup() method
 * restores the original bytes and validates the sequence numbers match.
 * If validation fails, the record is marked as 'BAAD' (bad).
 *
 * ## Thread Safety
 *
 * - preload_concurrent(): Thread-safe, can be called from multiple threads
 * - load(): NOT thread-safe, must be called sequentially with ordered chunks
 *
 * @note This file is included by ntfs_index_impl.hpp.
 *       Do not include this file directly.
 *
 * @see ntfs_index.hpp for NtfsIndex class declaration
 * @see ntfs_index_preprocess.hpp for the Preprocessor struct
 * @see mft_reader.hpp for the MFT reading pipeline
 */

#ifndef UFFS_NTFS_INDEX_LOAD_HPP
#define UFFS_NTFS_INDEX_LOAD_HPP

#ifndef UFFS_NTFS_INDEX_IMPL_HPP
#error "Do not include ntfs_index_load.hpp directly. Include ntfs_index.hpp instead."
#endif

// ============================================================================
// SECTION: Concurrent Preload Phase
// ============================================================================

/**
 * @brief Pre-processes MFT records for concurrent loading.
 *
 * This method is called from multiple threads during the concurrent MFT
 * reading phase. It performs two critical tasks:
 *
 * 1. **Multi-sector fixup**: Validates and restores sector end bytes
 * 2. **Vector pre-allocation**: Ensures vectors are sized for all FRS numbers
 *
 * @param virtual_offset Byte offset in the MFT file where this buffer starts
 * @param buffer         Pointer to the raw MFT data buffer
 * @param size           Size of the buffer in bytes
 */
inline void NtfsIndex::preload_concurrent(unsigned long long virtual_offset,
	void* buffer,
	size_t size) volatile
{
	unsigned int max_frs_plus_one = 0;
	unsigned int const mft_record_size = this->_mft_record_size;

	// Calculate log2 of MFT record size for efficient division via bit shift
	unsigned int mft_record_size_log2 = 0;
	for (;;)
	{
		unsigned int const v = mft_record_size_log2 + 1;
		if (!(mft_record_size >> v))
		{
			break;
		}
		mft_record_size_log2 = v;
	}

	assert((1U << mft_record_size_log2) == mft_record_size && "MFT record size not a power of 2");
	unsigned int const mft_record_size_pow2_mod_mask = mft_record_size - 1;

	// Iterate through each MFT record in the buffer
	for (size_t i = virtual_offset & mft_record_size_pow2_mod_mask
			 ? mft_record_size - virtual_offset & mft_record_size_pow2_mod_mask
			 : 0;
		i + mft_record_size <= size;
		i += mft_record_size)
	{
		unsigned int const frs = static_cast<unsigned int>((virtual_offset + i) >> mft_record_size_log2);

		ntfs::FILE_RECORD_SEGMENT_HEADER* const frsh =
			reinterpret_cast<ntfs::FILE_RECORD_SEGMENT_HEADER*>(&static_cast<unsigned char*>(buffer)[i]);

		// Check for 'FILE' magic (stored as 'ELIF' in little-endian)
		if (frsh->MultiSectorHeader.Magic == 'ELIF')
		{
			// Apply multi-sector fixup to validate and restore sector end bytes
			if (frsh->MultiSectorHeader.unfixup(mft_record_size))
			{
				unsigned int const frs_base = frsh->BaseFileRecordSegment
					? static_cast<unsigned int>(frsh->BaseFileRecordSegment)
					: frs;

				if (max_frs_plus_one < frs_base + 1)
				{
					max_frs_plus_one = frs_base + 1;
				}
			}
			else
			{
				// Fixup failed - mark record as bad
				frsh->MultiSectorHeader.Magic = 'DAAB';
			}
		}
	}

	// Pre-allocate vectors to avoid reallocations during load()
	if (max_frs_plus_one > 0)
	{
		lock(this)->at(max_frs_plus_one - 1);
	}
}

// ============================================================================
// SECTION: Main MFT Parsing (load method)
// ============================================================================

/**
 * @brief Parses MFT records and builds the in-memory file system index.
 *
 * This is the core method that transforms raw MFT data into the searchable
 * index. It processes each MFT record, extracts NTFS attributes, and builds
 * the data structures needed for fast file searching.
 *
 * ## NTFS Attribute Types Handled
 *
 * | Attribute Type          | Data Extracted                    |
 * |-------------------------|-----------------------------------|
 * | StandardInformation     | Created/Modified/Accessed times   |
 * | FileName                | Name, parent FRS, name type       |
 * | Data                    | File size, allocated size         |
 * | IndexRoot/Allocation    | Directory index (as $I30 stream)  |
 * | ReparsePoint            | Compression info (WofCompressed)  |
 *
 * @param virtual_offset Byte offset in the MFT file where this buffer starts
 * @param buffer         Pointer to the raw MFT data buffer
 * @param size           Size of the buffer in bytes
 * @param skipped_begin  Bytes skipped at start (unused records from bitmap)
 * @param skipped_end    Bytes skipped at end (unused records from bitmap)
 */
inline void NtfsIndex::load(unsigned long long virtual_offset,
	void* buffer,
	size_t size,
	unsigned long long skipped_begin,
	unsigned long long skipped_end)
{
	unsigned int const mft_record_size = this->_mft_record_size;

	// Calculate log2 of MFT record size for efficient division via bit shift
	unsigned int mft_record_size_log2 = 0;
	for (;;)
	{
		unsigned int const v = mft_record_size_log2 + 1;
		if (!(mft_record_size >> v))
		{
			break;
		}
		mft_record_size_log2 = v;
	}

	assert((1U << mft_record_size_log2) == mft_record_size && "MFT record size not a power of 2");
	unsigned int const mft_record_size_pow2_mod_mask = mft_record_size - 1;

	// Validate buffer size is a multiple of MFT record size
	if (size % mft_record_size)
	{
		throw std::runtime_error("Cluster size is smaller than MFT record size; split MFT records (over multiple clusters) not supported. Defragmenting your MFT may sometimes avoid this condition.");
	}

	// Account for skipped records in progress tracking
	if (skipped_begin || skipped_end)
	{
		this->_records_so_far.fetch_add(static_cast<unsigned int>((skipped_begin + skipped_end) >> mft_record_size_log2));
	}

	ChildInfo const empty_child_info;

	// ========================================================================
	// Main MFT Record Processing Loop
	// ========================================================================
	for (size_t i = virtual_offset & mft_record_size_pow2_mod_mask
			 ? mft_record_size - virtual_offset & mft_record_size_pow2_mod_mask
			 : 0;
		i + mft_record_size <= size;
		i += mft_record_size,
		this->_records_so_far.fetch_add(1, atomic_namespace::memory_order_acq_rel))
	{
		unsigned int const frs = static_cast<unsigned int>((virtual_offset + i) >> mft_record_size_log2);

		ntfs::FILE_RECORD_SEGMENT_HEADER* const frsh =
			reinterpret_cast<ntfs::FILE_RECORD_SEGMENT_HEADER*>(&static_cast<unsigned char*>(buffer)[i]);

		// Only process valid, in-use records
		if (frsh->MultiSectorHeader.Magic == 'ELIF' && !!(frsh->Flags & ntfs::FRH_IN_USE))
		{
			unsigned int const frs_base = frsh->BaseFileRecordSegment
				? static_cast<unsigned int>(frsh->BaseFileRecordSegment)
				: frs;
			auto base_record = this->at(frs_base);
			void const* const frsh_end = frsh->end(mft_record_size);

			// ================================================================
			// Attribute Parsing Loop
			// ================================================================
			for (ntfs::ATTRIBUTE_RECORD_HEADER const* ah = frsh->begin();
				 ah < frsh_end && ah->Type != ntfs::AttributeTypeCode::AttributeNone && ah->Type != ntfs::AttributeTypeCode::AttributeEnd;
				 ah = ah->next())
			{
				switch (ah->Type)
				{
				// ============================================================
				// ATTRIBUTE: StandardInformation (0x10)
				// ============================================================
				case ntfs::AttributeTypeCode::AttributeStandardInformation:
					if (ntfs::STANDARD_INFORMATION const* const fn =
						static_cast<ntfs::STANDARD_INFORMATION const*>(ah->Resident.GetValue()))
					{
						base_record->stdinfo.created  = fn->CreationTime;
						base_record->stdinfo.written  = fn->LastModificationTime;
						base_record->stdinfo.accessed = fn->LastAccessTime;
						base_record->stdinfo.attributes(fn->FileAttributes |
							((frsh->Flags & ntfs::FRH_DIRECTORY) ? FILE_ATTRIBUTE_DIRECTORY : 0));
					}
					break;

				// ============================================================
				// ATTRIBUTE: FileName (0x30)
				// ============================================================
				case ntfs::AttributeTypeCode::AttributeFileName:
					if (ntfs::FILENAME_INFORMATION const* const fn =
						static_cast<ntfs::FILENAME_INFORMATION const*>(ah->Resident.GetValue()))
					{
						unsigned int const frs_parent = static_cast<unsigned int>(fn->ParentDirectory);

						// Skip DOS-only names (0x02) - prefer Win32 or POSIX names
						if (fn->Flags != 0x02 /*FILE_NAME_DOS */)
						{
							// Handle hard links: push existing name to linked list
							if (LinkInfos::value_type* const si = this->nameinfo(&*base_record))
							{
								size_t const link_index = this->nameinfos.size();
								this->nameinfos.push_back(base_record->first_name);
								base_record->first_name.next_entry = static_cast<LinkInfos::value_type::next_entry_type>(link_index);
							}

							LinkInfo* const info = &base_record->first_name;
							info->name.offset(static_cast<unsigned int>(this->names.size()));
							info->name.length = static_cast<unsigned char>(fn->FileNameLength);

							bool const ascii = is_ascii(fn->FileName, fn->FileNameLength);
							info->name.ascii(ascii);
							info->parent = frs_parent;

							append_directional(this->names, fn->FileName, fn->FileNameLength, ascii ? 1 : 0);

							// Build parent-child relationship
							if (frs_parent != frs_base)
							{
								Records::iterator const parent = this->at(frs_parent, &base_record);

								ChildInfo* const pchild = this->childinfo(&*parent);
								if (!pchild || ~pchild->record_number)
								{
									size_t const child_index = this->childinfos.size();
									this->childinfos.push_back(parent->first_child);
									parent->first_child.next_entry = static_cast<ChildInfos::value_type::next_entry_type>(child_index);
								}

								ChildInfo* const child = &parent->first_child;
								child->record_number = frs_base;
								child->name_index = base_record->name_count;
							}

							++base_record->name_count;
						}
					}
					break;

				// ============================================================
				// ATTRIBUTES: Data and Other Stream Types
				// ============================================================
				case ntfs::AttributeTypeCode::AttributeObjectId:
				case ntfs::AttributeTypeCode::AttributePropertySet:
				case ntfs::AttributeTypeCode::AttributeBitmap:
				case ntfs::AttributeTypeCode::AttributeIndexAllocation:
				case ntfs::AttributeTypeCode::AttributeIndexRoot:
				case ntfs::AttributeTypeCode::AttributeData:
				case ntfs::AttributeTypeCode::AttributeReparsePoint:
				case ntfs::AttributeTypeCode::AttributeEA:
				case ntfs::AttributeTypeCode::AttributeEAInformation:
				default:
				{
					// MFT Zone Tracking for Non-Resident Attributes
					if (ah->IsNonResident)
					{
						mapping_pair_iterator mpi(ah,
							reinterpret_cast<unsigned char const*>(frsh_end) -
							reinterpret_cast<unsigned char const*>(ah));
						for (mapping_pair_iterator::vcn_type current_vcn = mpi->next_vcn;
							 !mpi.is_final();)
						{
							++mpi;
							if (mpi->current_lcn)
							{
								mapping_pair_iterator::lcn_type intersect_mft_zone_begin = this->_mft_zone_start;
								mapping_pair_iterator::lcn_type intersect_mft_zone_end = this->_mft_zone_end;
								if (intersect_mft_zone_begin < current_vcn)
								{
									intersect_mft_zone_begin = current_vcn;
								}
								if (intersect_mft_zone_end >= mpi->next_vcn)
								{
									intersect_mft_zone_end = mpi->next_vcn;
								}
								if (intersect_mft_zone_begin < intersect_mft_zone_end)
								{
									this->_reserved_clusters.fetch_sub(intersect_mft_zone_end - intersect_mft_zone_begin);
								}
							}
							current_vcn = mpi->next_vcn;
						}
					}

					// Stream Information Extraction
					bool const is_primary_attribute = !(ah->IsNonResident && ah->NonResident.LowestVCN);
					if (is_primary_attribute)
					{
						bool const isdir =
							(ah->Type == ntfs::AttributeTypeCode::AttributeBitmap ||
							 ah->Type == ntfs::AttributeTypeCode::AttributeIndexRoot ||
							 ah->Type == ntfs::AttributeTypeCode::AttributeIndexAllocation) &&
							ah->NameLength == 4 &&
							memcmp(ah->name(), _T("$I30"), sizeof(*ah->name()) * 4) == 0;

						unsigned char const name_length =
							isdir ? static_cast<unsigned char>(0) : ah->NameLength;
						unsigned char const type_name_id = static_cast<unsigned char>(
							isdir ? 0 : static_cast<int>(ah->Type) >> (CHAR_BIT / 2));

						StreamInfo* info = nullptr;

						if (StreamInfos::value_type* const si = this->streaminfo(&*base_record))
						{
							if (isdir)
							{
								for (StreamInfos::value_type* k = si; k; k = this->streaminfo(k->next_entry))
								{
									if (k->type_name_id == type_name_id &&
										k->name.length == name_length &&
										(name_length == 0 ||
										 std::equal(ah->name(),
											ah->name() + static_cast<ptrdiff_t>(ah->NameLength),
											this->names.begin() + static_cast<ptrdiff_t>(k->name.offset()))))
									{
										info = k;
										break;
									}
								}
							}

							if (!info)
							{
								size_t const stream_index = this->streaminfos.size();
								this->streaminfos.push_back(*si);
								si->next_entry = static_cast<small_t<size_t>::type>(stream_index);
							}
						}

						if (!info)
						{
							info = &base_record->first_stream;
							info->allocated = 0;
							info->length    = 0;
							info->bulkiness = 0;
							info->treesize  = 0;
							info->is_sparse = 0;
							info->is_allocated_size_accounted_for_in_main_stream = 0;
							info->type_name_id = type_name_id;
							info->name.length  = name_length;

							if (isdir)
							{
								info->name.offset(0);
							}
							else
							{
								info->name.offset(static_cast<unsigned int>(this->names.size()));
								bool const ascii = is_ascii(ah->name(), ah->NameLength);
								info->name.ascii(ascii);
								append_directional(this->names, ah->name(), ah->NameLength, ascii ? 1 : 0);
							}

							++base_record->stream_count;
							this->_total_names_and_streams.fetch_add(base_record->name_count, atomic_namespace::memory_order_acq_rel);
						}

						// Size Calculation
						bool const is_badclus_bad =
							frs_base == 0x000000000008 && ah->NameLength == 4 &&
							memcmp(ah->name(), _T("$Bad"), sizeof(*ah->name()) * 4) == 0;

						bool const is_sparse = !!(ah->Flags & 0x8000);
						if (is_sparse)
						{
							info->is_sparse |= 0x1;
						}

						info->allocated += ah->IsNonResident
							? (ah->NonResident.CompressionUnit
								? static_cast<file_size_type>(ah->NonResident.CompressedSize)
								: static_cast<file_size_type>(is_badclus_bad
									? ah->NonResident.InitializedSize
									: ah->NonResident.AllocatedSize))
							: 0;

						info->length += ah->IsNonResident
							? static_cast<file_size_type>(is_badclus_bad ? ah->NonResident.InitializedSize
								: ah->NonResident.DataSize)
							: ah->Resident.ValueLength;

						info->bulkiness += info->allocated;
						info->treesize = isdir;
					}

					break;
				}  // end default case
			}  // end switch (ah->Type)
		}  // end attribute loop
	}  // end record loop (IN_USE check)
	}  // end main MFT record processing loop

	// ========================================================================
	// Post-Processing Phase
	// ========================================================================
	// After all MFT records are loaded, run the preprocessor to calculate
	// cumulative directory sizes and finalize the index.

	unsigned int const records_so_far = this->_records_so_far.load(atomic_namespace::memory_order_acquire);
	bool const finished = records_so_far >= this->_mft_capacity;

	if (finished && !this->_root_path.empty())
	{
		// Debug output: log memory usage statistics
		TCHAR buf[2048];
		size_t names_wasted_chars = 0;
		for (auto const& ch : this->names)
		{
			if ((ch | SCHAR_MAX) == SCHAR_MAX)
			{
				++names_wasted_chars;
			}
		}

		int const nbuf = safe_stprintf(
			buf,
			_T("%s\trecords_data\twidth = %2u\tsize = %8I64u\tcapacity = %8I64u\n")
			_T("%s\trecords_lookup\twidth = %2u\tsize = %8I64u\tcapacity = %8I64u\n")
			_T("%s\tnames\twidth = %2u\tsize = %8I64u\tcapacity = %8I64u\t%8I64u bytes wasted\n")
			_T("%s\tnameinfos\twidth = %2u\tsize = %8I64u\tcapacity = %8I64u\n")
			_T("%s\tstreaminfos\twidth = %2u\tsize = %8I64u\tcapacity = %8I64u\n")
			_T("%s\tchildinfos\twidth = %2u\tsize = %8I64u\tcapacity = %8I64u\n"),
			this->_root_path.c_str(), static_cast<unsigned>(sizeof(*this->records_data.begin())),
			static_cast<unsigned long long>(this->records_data.size()),
			static_cast<unsigned long long>(this->records_data.capacity()),
			this->_root_path.c_str(), static_cast<unsigned>(sizeof(*this->records_lookup.begin())),
			static_cast<unsigned long long>(this->records_lookup.size()),
			static_cast<unsigned long long>(this->records_lookup.capacity()),
			this->_root_path.c_str(), static_cast<unsigned>(sizeof(*this->names.begin())),
			static_cast<unsigned long long>(this->names.size()),
			static_cast<unsigned long long>(this->names.capacity()),
			static_cast<unsigned long long>(names_wasted_chars),
			this->_root_path.c_str(), static_cast<unsigned>(sizeof(*this->nameinfos.begin())),
			static_cast<unsigned long long>(this->nameinfos.size()),
			static_cast<unsigned long long>(this->nameinfos.capacity()),
			this->_root_path.c_str(), static_cast<unsigned>(sizeof(*this->streaminfos.begin())),
			static_cast<unsigned long long>(this->streaminfos.size()),
			static_cast<unsigned long long>(this->streaminfos.capacity()),
			this->_root_path.c_str(), static_cast<unsigned>(sizeof(*this->childinfos.begin())),
			static_cast<unsigned long long>(this->childinfos.size()),
			static_cast<unsigned long long>(this->childinfos.capacity()));
		buf[_countof(buf) - 1] = _T('\0');
		if (nbuf > 0)
		{
			OutputDebugString(buf);
		}

		// Run the preprocessor to calculate directory sizes
		// Uses the Preprocessor struct from ntfs_index_preprocess.hpp
		Preprocessor preprocessor = { this };

		clock_t const tbefore_preprocess = clock();

		// Start preprocessing from root directory (FRS 5 = kRootFRS)
		preprocessor(this->find(0x000000000005), 0, 1);

		clock_t const tfinish = clock();

		// Close volume handle - no longer needed after indexing
		Handle().swap(this->_volume);

		// Debug timing output (disabled by default)
		if (false)
		{
			_ftprintf(stderr,
				_T("Finished: %s (%I64u ms total, %I64u ms preprocessing)\n"),
				this->_root_path.c_str(),
				(tfinish - this->_tbegin) * 1000ULL / CLOCKS_PER_SEC,
				(tfinish - tbefore_preprocess) * 1000ULL / CLOCKS_PER_SEC);
		}
	}

	// Signal completion or reset event based on whether we're done
	finished ? SetEvent(this->_finished_event) : ResetEvent(this->_finished_event);
}

#endif // UFFS_NTFS_INDEX_LOAD_HPP
