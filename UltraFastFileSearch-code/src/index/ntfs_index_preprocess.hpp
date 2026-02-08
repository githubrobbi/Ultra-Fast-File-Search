/**
 * @file ntfs_index_preprocess.hpp
 * @brief Directory size calculation via depth-first tree traversal.
 *
 * This file contains the Preprocessor struct, which performs a depth-first
 * traversal of the NTFS directory tree to calculate cumulative sizes for
 * each directory after MFT parsing is complete.
 *
 * ## Architecture Overview
 *
 * After the MFT is parsed, each file knows its own size, but directories
 * don't know the total size of their contents. The Preprocessor calculates:
 *
 * - **treesize**: Total number of files/directories in subtree
 * - **allocated**: Total disk space used by subtree
 * - **length**: Total logical size of subtree
 * - **bulkiness**: Size metric excluding small children (for sorting)
 *
 * ## Processing Flow
 *
 * ```
 *   Start at Root (FRS 5)
 *          |
 *          v
 *   For each child directory:
 *     - Recurse into child
 *     - Accumulate child sizes
 *     - Track child bulkiness
 *          |
 *          v
 *   Bulkiness Threshold Calculation:
 *     - Build heap of children
 *     - Find children > 1% threshold
 *     - Subtract large items from bulkiness
 *          |
 *          v
 *   Stream Processing:
 *     - Process each stream
 *     - Handle WOF compression
 *     - Update progress counter
 *          |
 *          v
 *   Return cumulative sizes
 * ```
 *
 * ## Bulkiness Calculation
 *
 * "Bulkiness" is a size metric that excludes children smaller than 1% of
 * the parent's allocated size. This prevents many small files from
 * dominating the size display when sorting by size.
 *
 * ## Hard Link Size Distribution
 *
 * When a file has multiple hard links (names), its size is distributed
 * fairly among all links using the Accumulator helper:
 *   share[i] = value * (i+1) / n - value * i / n
 * This ensures: sum(share[0..n-1]) == value (no rounding loss)
 *
 * ## WOF Compression Handling
 *
 * Windows 10+ uses Windows Overlay Filter (WOF) compression for system files.
 * Compressed files have a :WofCompressedData stream containing the actual
 * compressed data. The default stream contains a reparse point. We merge
 * the compressed stream's size into the default stream for accurate reporting.
 *
 * ## Thread Safety
 *
 * The Preprocessor is designed for single-threaded use during the final
 * phase of index building. It modifies stream info in place and uses
 * atomic operations only for progress tracking.
 *
 * @note This file is included by ntfs_index_impl.hpp.
 *       Do not include this file directly.
 *
 * @see ntfs_index.hpp for NtfsIndex class declaration
 * @see ntfs_index_load.hpp for the load() function that invokes preprocessing
 */

#ifndef UFFS_NTFS_INDEX_PREPROCESS_HPP
#define UFFS_NTFS_INDEX_PREPROCESS_HPP

#ifndef UFFS_NTFS_INDEX_IMPL_HPP
#error "Do not include ntfs_index_preprocess.hpp directly. Include ntfs_index.hpp instead."
#endif

// ============================================================================
// SECTION: Preprocessor Struct
// ============================================================================

struct NtfsIndex::Preprocessor
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
					static_cast<unsigned long long>(me->_reserved_clusters) * me->_cluster_size;
			}

			result = children_size;

			// Accumulator: Fair Size Distribution for Hard Links
			struct Accumulator
			{
				static unsigned long long delta_impl(unsigned long long const value,
					unsigned short const i, unsigned short const n)
				{
					return value * (i + 1) / n - value * i / n;
				}

				static unsigned long long delta(unsigned long long const value,
					unsigned short const i, unsigned short const n)
				{
					return n != 1
						? (n != 2 ? delta_impl(value, i, n)
								: (i != 1 ? delta_impl(value, ((void)(assert(i == 0)), 0), n)
									   : delta_impl(value, i, n)))
						: delta_impl(value, ((void)(assert(i == 0)), 0), n);
				}
			};

			// Stream Processing and Compression Handling
			StreamInfos::value_type* default_stream = nullptr;
			StreamInfos::value_type* compressed_default_stream_to_merge = nullptr;
			unsigned long long default_allocated_delta = 0;
			unsigned long long compressed_default_allocated_delta = 0;

			for (StreamInfos::value_type* k = me->streaminfo(fr); k; k = me->streaminfo(k->next_entry))
			{
				bool const is_data_attribute =
					(k->type_name_id << (CHAR_BIT / 2)) ==
					static_cast<int>(ntfs::AttributeTypeCode::AttributeData);

				bool const is_default_stream = is_data_attribute && !k->name.length;

				unsigned long long const allocated_delta = Accumulator::delta(
					k->is_allocated_size_accounted_for_in_main_stream
						? static_cast<file_size_type>(0)
						: k->allocated,
					name_info, total_names);
				unsigned long long const bulkiness_delta = Accumulator::delta(
					k->bulkiness, name_info, total_names);

				if (is_default_stream)
				{
					default_stream = k;
					default_allocated_delta += allocated_delta;
				}

				// WOF Compression Handling
				bool const is_compression_reparse_point =
					is_data_attribute && k->name.length &&
					(k->name.ascii()
						 ? memcmp(reinterpret_cast<char const*>(&me->names[k->name.offset()]),
							        "WofCompressedData", 17 * sizeof(char)) == 0
						 : memcmp(reinterpret_cast<wchar_t const*>(&me->names[k->name.offset()]),
							        L"WofCompressedData", 17 * sizeof(wchar_t)) == 0);

				unsigned long long const length_delta = Accumulator::delta(
					is_compression_reparse_point ? static_cast<file_size_type>(0) : k->length,
					name_info, total_names);

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

			// Merge WOF compressed stream size into default stream
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
};

#endif // UFFS_NTFS_INDEX_PREPROCESS_HPP

