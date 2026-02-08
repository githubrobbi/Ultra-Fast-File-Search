// ntfs_index_impl.hpp - Implementation details for NtfsIndex
//
// Do not include this file directly. Include ntfs_index.hpp instead.

#ifndef UFFS_NTFS_INDEX_IMPL_HPP
#define UFFS_NTFS_INDEX_IMPL_HPP

#ifndef UFFS_NTFS_INDEX_HPP
#error "Do not include ntfs_index_impl.hpp directly. Include ntfs_index.hpp instead."
#endif

// Implementation of NtfsIndex methods and nested helpers lives here.
// The declarations are in ntfs_index.hpp.

// preload_concurrent: implementation moved from ntfs_index.hpp
inline void NtfsIndex::preload_concurrent(unsigned long long virtual_offset,
	void* buffer,
	size_t size) volatile
{
	unsigned int max_frs_plus_one = 0;
	unsigned int const mft_record_size = this->mft_record_size;
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
	for (size_t i = virtual_offset & mft_record_size_pow2_mod_mask
			 ? mft_record_size - virtual_offset & mft_record_size_pow2_mod_mask
			 : 0;
		i + mft_record_size <= size;
		i += mft_record_size)
	{
		unsigned int const frs = static_cast<unsigned int>((virtual_offset + i) >> mft_record_size_log2);
		ntfs::FILE_RECORD_SEGMENT_HEADER* const frsh =
			reinterpret_cast<ntfs::FILE_RECORD_SEGMENT_HEADER*>(&static_cast<unsigned char*>(buffer)[i]);
		if (frsh->MultiSectorHeader.Magic == 'ELIF')
		{
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
				frsh->MultiSectorHeader.Magic = 'DAAB';
			}
		}
	}

	if (max_frs_plus_one > 0)
	{
		// Ensure the vector is only reallocated once this iteration, if possible
		lock(this)->at(max_frs_plus_one - 1);
	}
}

// load: implementation moved from ntfs_index.hpp
inline void NtfsIndex::load(unsigned long long virtual_offset,
	void* buffer,
	size_t size,
	unsigned long long skipped_begin,
	unsigned long long skipped_end)
{
	unsigned int const mft_record_size = this->mft_record_size;
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
	if (size % mft_record_size)
	{
		throw std::runtime_error("Cluster size is smaller than MFT record size; split MFT records (over multiple clusters) not supported. Defragmenting your MFT may sometimes avoid this condition.");
	}

	if (skipped_begin || skipped_end)
	{
		this->_records_so_far.fetch_add(static_cast<unsigned int>((skipped_begin + skipped_end) >> mft_record_size_log2));
	}

	ChildInfo const empty_child_info;
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
		if (frsh->MultiSectorHeader.Magic == 'ELIF' && !!(frsh->Flags & ntfs::FRH_IN_USE))
		{
			unsigned int const frs_base = frsh->BaseFileRecordSegment
				? static_cast<unsigned int>(frsh->BaseFileRecordSegment)
				: frs;
			auto base_record = this->at(frs_base);
			void const* const frsh_end = frsh->end(mft_record_size);
			for (ntfs::ATTRIBUTE_RECORD_HEADER const* ah = frsh->begin();
				 ah < frsh_end && ah->Type != ntfs::AttributeTypeCode::AttributeNone && ah->Type != ntfs::AttributeTypeCode::AttributeEnd;
				 ah = ah->next())
			{
				switch (ah->Type)
				{
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
				case ntfs::AttributeTypeCode::AttributeFileName:
					if (ntfs::FILENAME_INFORMATION const* const fn =
						static_cast<ntfs::FILENAME_INFORMATION const*>(ah->Resident.GetValue()))
					{
						unsigned int const frs_parent = static_cast<unsigned int>(fn->ParentDirectory);
						if (fn->Flags != 0x02 /*FILE_NAME_DOS */)
						{
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
							if (frs_parent != frs_base)
							{
								Records::iterator const parent = this->at(frs_parent, &base_record);
								size_t const child_index = this->childinfos.size();
								this->childinfos.push_back(empty_child_info);
								ChildInfo* const child_info = &this->childinfos.back();
								child_info->record_number   = frs_base;
								child_info->name_index      = base_record->name_count;
								child_info->next_entry      = parent->first_child;
								parent->first_child         = static_cast<ChildInfos::value_type::next_entry_type>(child_index);
							}

							this->_total_names_and_streams.fetch_add(base_record->stream_count, atomic_namespace::memory_order_acq_rel);
							++base_record->name_count;
						}
					}
					break;
				// case ntfs::AttributeTypeCode::AttributeAttributeList:
				// case ntfs::AttributeTypeCode::AttributeLoggedUtilityStream:
				case ntfs::AttributeTypeCode::AttributeObjectId:
				// case ntfs::AttributeTypeCode::AttributeSecurityDescriptor:
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
								mapping_pair_iterator::lcn_type intersect_mft_zone_begin = this->mft_zone_start;
								mapping_pair_iterator::lcn_type intersect_mft_zone_end = this->mft_zone_end;
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
									this->reserved_clusters.fetch_sub(intersect_mft_zone_end - intersect_mft_zone_begin);
								}
							}

							current_vcn = mpi->next_vcn;
						}
					}

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
				}
			}
		}
	}

	unsigned int const records_so_far = this->_records_so_far.load(atomic_namespace::memory_order_acquire);
	bool const finished = records_so_far >= this->mft_capacity;
	if (finished && !this->_root_path.empty())
	{
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

		struct
		{
			using PreprocessResult = SizeInfo;
			NtfsIndex* me;
			using Scratch = std::vector<unsigned long long>;
			Scratch scratch;
			size_t depth;
			PreprocessResult operator()(Records::value_type* const fr,
				key_type::name_info_type const name_info,
				unsigned short const total_names)
			{
				size_t const old_scratch_size = scratch.size();
				PreprocessResult result;
				if (fr)
				{
					PreprocessResult children_size;
					++depth;
					for (ChildInfos::value_type* i = me->childinfo(fr);
						 i && ~i->record_number;
						 i = me->childinfo(i->next_entry))
					{
						Records::value_type* const fr2 = me->find(i->record_number);
						if (fr2 != fr)
						{
							PreprocessResult const subresult = this->operator()(fr2,
								fr2->name_count - static_cast<size_t>(1) - i->name_index,
								fr2->name_count);
							scratch.push_back(subresult.bulkiness);
							children_size.length    += subresult.length;
							children_size.allocated += subresult.allocated;
							children_size.bulkiness += subresult.bulkiness;
							children_size.treesize  += subresult.treesize;
						}
					}

					--depth;
					std::make_heap(scratch.begin() + static_cast<ptrdiff_t>(old_scratch_size), scratch.end());
					unsigned long long const threshold = children_size.allocated / 100;
					for (auto i = scratch.end();
						 i != scratch.begin() + static_cast<ptrdiff_t>(old_scratch_size);)
					{
						std::pop_heap(scratch.begin() + static_cast<ptrdiff_t>(old_scratch_size), i);
						--i;
						if (*i < threshold)
						{
							break;
						}

						children_size.bulkiness = children_size.bulkiness - *i;
					}

					if (depth == 0)
					{
						children_size.allocated +=
							static_cast<unsigned long long>(me->reserved_clusters) * me->cluster_size;
					}

					result = children_size;
					struct Accumulator
					{
						static unsigned long long delta_impl(unsigned long long const value,
							unsigned short const i,
							unsigned short const n)
						{
							return value * (i + 1) / n - value * i / n;
						}

						static unsigned long long delta(unsigned long long const value,
							unsigned short const i,
							unsigned short const n)
						{
							return n != 1
								? (n != 2 ? delta_impl(value, i, n)
										: (i != 1 ? delta_impl(value, ((void)(assert(i == 0)), 0), n)
											   : delta_impl(value, i, n)))
								: delta_impl(value, ((void)(assert(i == 0)), 0), n);
						}
					};

					StreamInfos::value_type* default_stream = nullptr;
					StreamInfos::value_type* compressed_default_stream_to_merge = nullptr;
					unsigned long long default_allocated_delta = 0;
					unsigned long long compressed_default_allocated_delta = 0;
					for (StreamInfos::value_type* k = me->streaminfo(fr);
						 k;
						 k = me->streaminfo(k->next_entry))
					{
						bool const is_data_attribute =
							(k->type_name_id << (CHAR_BIT / 2)) ==
							static_cast<int>(ntfs::AttributeTypeCode::AttributeData);
						bool const is_default_stream = is_data_attribute && !k->name.length;
						unsigned long long const allocated_delta = Accumulator::delta(
							k->is_allocated_size_accounted_for_in_main_stream
								? static_cast<file_size_type>(0)
								: k->allocated,
							name_info,
							total_names);
						unsigned long long const bulkiness_delta = Accumulator::delta(
							k->bulkiness,
							name_info,
							total_names);
						if (is_default_stream)
						{
							default_stream = k;
							default_allocated_delta += allocated_delta;
						}

						bool const is_compression_reparse_point =
							is_data_attribute && k->name.length &&
							(k->name.ascii()
								 ? memcmp(reinterpret_cast<char const*>(&me->names[k->name.offset()]),
									        "WofCompressedData",
									        17 * sizeof(char)) == 0
								 : memcmp(reinterpret_cast<wchar_t const*>(&me->names[k->name.offset()]),
									        L"WofCompressedData",
									        17 * sizeof(wchar_t)) == 0);
						unsigned long long const length_delta = Accumulator::delta(
							is_compression_reparse_point ? static_cast<file_size_type>(0) : k->length,
							name_info,
							total_names);
						if (is_compression_reparse_point)
						{
							if (!k->is_allocated_size_accounted_for_in_main_stream)
							{
								compressed_default_stream_to_merge = k;
								compressed_default_allocated_delta += allocated_delta;
							}
						}

						result.length    += length_delta;
						result.allocated += allocated_delta;
						result.bulkiness += bulkiness_delta;
						result.treesize  += 1;
						if (!k->type_name_id)
						{
							k->length    += children_size.length;
							k->allocated += children_size.allocated;
							k->bulkiness += children_size.bulkiness;
							k->treesize  += children_size.treesize;
						}

						me->_preprocessed_so_far.fetch_add(1, atomic_namespace::memory_order_acq_rel);
					}

					if (compressed_default_stream_to_merge && default_stream)
					{
						compressed_default_stream_to_merge->is_allocated_size_accounted_for_in_main_stream = 1;
						default_stream->allocated += compressed_default_stream_to_merge->allocated;
						result.allocated -= default_allocated_delta;
						result.allocated -= compressed_default_allocated_delta;
						result.allocated += Accumulator::delta(default_stream->allocated, name_info, total_names);
					}
				}

				scratch.erase(scratch.begin() + static_cast<ptrdiff_t>(old_scratch_size), scratch.end());
				return result;
			}
		} preprocessor = { this };

		clock_t const tbefore_preprocess = clock();
		preprocessor(this->find(0x000000000005), 0, 1);
		clock_t const tfinish = clock();
		Handle().swap(this->_volume);
		if (false)
		{
			_ftprintf(stderr,
				_T("Finished: %s (%I64u ms total, %I64u ms preprocessing)\n"),
				this->_root_path.c_str(),
				(tfinish - this->_tbegin) * 1000ULL / CLOCKS_PER_SEC,
				(tfinish - tbefore_preprocess) * 1000ULL / CLOCKS_PER_SEC);
		}
	}

	finished ? SetEvent(this->_finished_event) : ResetEvent(this->_finished_event);
}

// report_speed: implementation moved from ntfs_index.hpp
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

// get_file_pointers: implementation moved from ntfs_index.hpp
inline NtfsIndex::file_pointers NtfsIndex::get_file_pointers(key_type key) const
{
	bool wait_for_finish = false;  // has performance penalty
	if (wait_for_finish && WaitForSingleObject(this->_finished_event, 0) == WAIT_TIMEOUT)
	{
		throw std::logic_error("Need to wait for indexing to be finished");
	}

	file_pointers result;
	result.record = nullptr;
	if (~key.frs())
	{
		Records::value_type const* const fr = this->find(key.frs());
		unsigned short ji = 0;
		for (LinkInfos::value_type const* j = this->nameinfo(fr);
			 j;
			 j = this->nameinfo(j->next_entry), ++ji)
		{
			key_type::name_info_type const name_info = key.name_info();
			if (name_info == USHRT_MAX || ji == name_info)
			{
				unsigned short ki = 0;
				for (StreamInfos::value_type const* k = this->streaminfo(fr);
					 k;
					 k = this->streaminfo(k->next_entry), ++ki)
				{
					key_type::stream_info_type const stream_info = key.stream_info();
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

// get_path: implementation moved from ntfs_index.hpp
inline size_t NtfsIndex::get_path(key_type key,
	std::tvstring& result,
	bool const name_only,
	unsigned int* attributes) const
{
	size_t const old_size = result.size();
	for (ParentIterator pi(this, key); pi.next() && !(name_only && pi.icomponent());)
	{
		if (attributes)
		{
			*attributes = pi.attributes();
			attributes = nullptr;
		}

		TCHAR const* const s = static_cast<TCHAR const*>(pi->first);
		if (name_only || !(pi->second == 1 && (pi->ascii
			? *static_cast<char const*>(static_cast<void const*>(s))
			: *s) == _T('.')))
		{
			append_directional(result, s, pi->second, pi->ascii ? -1 : 0, true);
		}
	}

	std::reverse(result.begin() + static_cast<ptrdiff_t>(old_size), result.end());
	return result.size() - old_size;
}

// get_sizes: implementation moved from ntfs_index.hpp
inline NtfsIndex::size_info const& NtfsIndex::get_sizes(key_type const& key) const
{
	StreamInfos::value_type const* k;
	Records::value_type const* const fr = this->find(key.frs());
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

// get_stdinfo: implementation moved from ntfs_index.hpp
inline NtfsIndex::standard_info const& NtfsIndex::get_stdinfo(unsigned int const frn) const
{
	return this->find(frn)->stdinfo;
}

#endif // UFFS_NTFS_INDEX_IMPL_HPP
