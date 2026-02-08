/**
 * @file ntfs_index.hpp
 * @brief NTFS index engine - public interface.
 *
 * This header declares the NtfsIndex class, which owns the in-memory
 * representation of an NTFS volume's file records, names, streams, and
 * directory hierarchy. Heavy implementation details live in
 * ntfs_index_impl.hpp, which is included at the end of this file to
 * preserve header-only usage where needed.
 */

#ifndef UFFS_NTFS_INDEX_HPP
#define UFFS_NTFS_INDEX_HPP

#include <Windows.h>
#include <tchar.h>
#include <climits>
#include <iterator>
#include <vector>
#include <codecvt>

#include "util/intrusive_ptr.hpp"
#include "util/atomic_compat.hpp"
#include "util/handle.hpp"
#include "util/containers.hpp"
#include "io/overlapped.hpp"
#include "core/ntfs_types.hpp"
#include "util/buffer.hpp"
#include "util/path.hpp"
#include "util/append_directional.hpp"
#include "util/error_utils.hpp"
#include "io/winnt_types.hpp"
#include "io/io_priority.hpp"
#include "util/type_traits_ext.hpp"
#include "core/file_attributes_ext.hpp"
#include "core/packed_file_size.hpp"
#include "core/standard_info.hpp"
#include "core/ntfs_record_types.hpp"
#include "core/ntfs_key_type.hpp"
#include "mapping_pair_iterator.hpp"

class NtfsIndex : public RefCounted<NtfsIndex>
{
	typedef NtfsIndex this_type;

public:
	// ========================================================================
	// NTFS File Record Segment (FRS) Constants
	// ========================================================================
	static constexpr unsigned int kRootFRS = 0x00000005;       ///< Root directory FRS
	static constexpr unsigned int kVolumeFRS = 0x00000006;     ///< $Volume metadata FRS
	static constexpr unsigned int kFirstUserFRS = 0x00000010;  ///< First user file FRS

private:
	// Type aliases from extracted headers
	template <class T = void>
	using small_t = ::uffs::small_t<T>;
	typedef ::uffs::file_size_type file_size_type;
	typedef ::uffs::StandardInfo StandardInfo;
	typedef ::uffs::SizeInfo SizeInfo;
	typedef ::uffs::NameInfo NameInfo;
	typedef ::uffs::LinkInfo LinkInfo;
	typedef ::uffs::StreamInfo StreamInfo;
	typedef ::uffs::ChildInfo ChildInfo;
	typedef ::uffs::Record Record;

	static unsigned int IsWow64Process_();

	friend struct std::is_scalar<StandardInfo>;
	friend struct std::is_scalar<NameInfo>;
	friend struct std::is_scalar<LinkInfo>;
	friend struct std::is_scalar<StreamInfo>;
	friend struct std::is_scalar<Record>;

	typedef std::codecvt<std::tstring::value_type, char, int /*std::mbstate_t*/> CodeCvt;
	typedef vector_with_fast_size<LinkInfo> LinkInfos;
	typedef vector_with_fast_size<StreamInfo> StreamInfos;
	typedef vector_with_fast_size<Record> Records;
	typedef std::vector<unsigned int> RecordsLookup;
	typedef vector_with_fast_size<ChildInfo> ChildInfos;

	mutable atomic_namespace::recursive_mutex _mutex;
	value_initialized<clock_t> _tbegin;
	value_initialized<bool> _init_called;
	std::tvstring _root_path;
	Handle _volume;
	std::tvstring names;
	Records records_data;
	RecordsLookup records_lookup;
	LinkInfos nameinfos;
	StreamInfos streaminfos;
	ChildInfos childinfos;
	Handle _finished_event;
	atomic_namespace::atomic<unsigned int> _finished;
	atomic_namespace::atomic<size_t> _total_names_and_streams;
	value_initialized<unsigned int> _expected_records;
	atomic_namespace::atomic<bool> _cancelled;
	atomic_namespace::atomic<unsigned int> _records_so_far, _preprocessed_so_far;
	std::vector<Speed> _perf_reports_circ; // circular buffer
	value_initialized<size_t> _perf_reports_begin;
	atomic_namespace::spin_atomic<Speed> _perf_avg_speed;

	// Volume configuration (private, accessed via getters)
	atomic_namespace::atomic<long long> _reserved_clusters;
	value_initialized<long long> _mft_zone_start, _mft_zone_end;
	value_initialized<unsigned int> _cluster_size;
	value_initialized<unsigned int> _mft_record_size;
	value_initialized<unsigned int> _mft_capacity;

	typedef ::uffs::key_type_internal key_type_internal;

	// Internal helpers declared here, implemented in ntfs_index_impl.hpp
	Records::iterator at(size_t frs, Records::iterator* existing_to_revalidate = nullptr);

	template <class Me>
	static typename propagate_const<Me, Records::value_type>::type*
	_find(Me* me, key_type_internal::frs_type frs);

	Records::value_type* find(key_type_internal::frs_type frs);
	Records::value_type const* find(key_type_internal::frs_type frs) const;

	ChildInfos::value_type* childinfo(Records::value_type* i);
	ChildInfos::value_type const* childinfo(Records::value_type const* i) const;
	ChildInfos::value_type* childinfo(ChildInfo::next_entry_type i);
	ChildInfos::value_type const* childinfo(ChildInfo::next_entry_type i) const;

	LinkInfos::value_type* nameinfo(LinkInfo::next_entry_type i);
	LinkInfos::value_type const* nameinfo(LinkInfo::next_entry_type i) const;
	LinkInfos::value_type* nameinfo(Records::value_type* i);
	LinkInfos::value_type const* nameinfo(Records::value_type const* i) const;

	StreamInfos::value_type* streaminfo(StreamInfo::next_entry_type i);
	StreamInfos::value_type const* streaminfo(StreamInfo::next_entry_type i) const;
	StreamInfos::value_type* streaminfo(Records::value_type* i);
	StreamInfos::value_type const* streaminfo(Records::value_type const* i) const;

	// Forward declaration of Matcher template (defined in ntfs_index_impl.hpp)
	template <class F>
	struct Matcher;

public:
	// Public type aliases for callers
	typedef key_type_internal key_type;
	typedef StandardInfo standard_info;
	typedef NameInfo name_info;
	typedef SizeInfo size_info;

	// Volume configuration accessors (read-only)
	[[nodiscard]] long long reserved_clusters() const volatile noexcept;
	[[nodiscard]] long long mft_zone_start() const noexcept;
	[[nodiscard]] long long mft_zone_end() const noexcept;
	[[nodiscard]] unsigned int cluster_size() const noexcept;
	[[nodiscard]] unsigned int mft_record_size() const volatile noexcept;
	[[nodiscard]] unsigned int mft_capacity() const volatile noexcept;

	// Volume configuration setters (for load() to use)
	void set_reserved_clusters(long long value) volatile noexcept;
	void set_mft_zone_start(long long value) noexcept;
	void set_mft_zone_end(long long value) noexcept;
	void set_cluster_size(unsigned int value) noexcept;
	void set_mft_record_size(unsigned int value) noexcept;
	void set_mft_capacity(unsigned int value) noexcept;

	// Construction / lifetime
	NtfsIndex(std::tvstring value);
	~NtfsIndex();

	// Initialization and lifecycle
	[[nodiscard]] bool init_called() const noexcept;
	void init();
	void set_finished(unsigned int const& result);

	// Volatile helpers for lock_ptr and multi-threaded access
	[[nodiscard]] NtfsIndex* unvolatile() volatile noexcept;
	[[nodiscard]] NtfsIndex const* unvolatile() const volatile noexcept;

	// Progress and statistics accessors
	[[nodiscard]] size_t total_names_and_streams() const noexcept;
	[[nodiscard]] size_t total_names_and_streams() const volatile noexcept;
	[[nodiscard]] size_t total_names() const noexcept;
	[[nodiscard]] size_t expected_records() const noexcept;
	[[nodiscard]] size_t preprocessed_so_far() const volatile noexcept;
	[[nodiscard]] size_t preprocessed_so_far() const noexcept;
	[[nodiscard]] size_t records_so_far() const volatile noexcept;
	[[nodiscard]] size_t records_so_far() const noexcept;
	[[nodiscard]] void* volume() const volatile noexcept;
	[[nodiscard]] atomic_namespace::recursive_mutex& get_mutex() const volatile noexcept;
	[[nodiscard]] Speed speed() const volatile noexcept;
	[[nodiscard]] std::tvstring const& root_path() const volatile noexcept;
	[[nodiscard]] unsigned int get_finished() const volatile noexcept;
	[[nodiscard]] bool cancelled() const volatile noexcept;
	void cancel() volatile noexcept;
	[[nodiscard]] uintptr_t finished_event() const noexcept;

	// Capacity reservation
	void reserve(unsigned int records);

	void preload_concurrent(unsigned long long virtual_offset, void* buffer, size_t size) volatile;

	void load(unsigned long long virtual_offset, void* buffer, size_t size,
		unsigned long long skipped_begin, unsigned long long skipped_end);

	void report_speed(unsigned long long const size, clock_t const tfrom, clock_t const tto);

	struct file_pointers
	{
		Records::value_type const* record;
		LinkInfos::value_type const* link;
		StreamInfos::value_type const* stream;
		[[nodiscard]] key_type parent() const noexcept
		{
			return key_type(link->parent,
				static_cast<key_type::name_info_type>(~key_type::name_info_type()),
				static_cast<key_type::stream_info_type>(~key_type::stream_info_type()));
		}
	};

	[[nodiscard]] file_pointers get_file_pointers(key_type key) const;

	  class ParentIterator
	  {
		  typedef ParentIterator this_type;
		  struct value_type_internal
		  {
			  void
				  const* first;
			  size_t second : sizeof(size_t)* CHAR_BIT - 1;
			  size_t ascii : 1;
		  };

		  NtfsIndex
			  const* index;
		  key_type key;
		  unsigned char state;
		  unsigned short iteration;
		  file_pointers ptrs;
		  value_type_internal result;
		  [[nodiscard]] bool is_root() const noexcept
		  {
			  return key.frs() == kRootFRS;
		  }

		  [[nodiscard]] bool is_attribute() const noexcept
		  {
			  return ptrs.stream->type_name_id && (ptrs.stream->type_name_id << (CHAR_BIT / 2)) != static_cast<int>(ntfs::AttributeTypeCode::AttributeData);
		  }

	  public:
		  typedef value_type_internal value_type;
		  struct value_type_compare
		  {
			bool operator()(value_type
				const& a, value_type
				const& b) const
			{
				bool result;
				if (a.ascii)
				{
					if (b.ascii)
					{
						result = std::lexicographical_compare(static_cast<char
							const*> (a.first), static_cast<char
							const*> (a.first) + static_cast<ptrdiff_t> (a.second),
							static_cast<char
							const*> (b.first), static_cast<char
							const*> (b.first) + static_cast<ptrdiff_t> (b.second));
					}
					else
					{
						result = std::lexicographical_compare(static_cast<char
							const*> (a.first), static_cast<char
							const*> (a.first) + static_cast<ptrdiff_t> (a.second),
							static_cast<wchar_t
							const*> (b.first), static_cast<wchar_t
							const*> (b.first) + static_cast<ptrdiff_t> (b.second));
					}
				}
				else
				{
					if (b.ascii)
					{
						result = std::lexicographical_compare(static_cast<wchar_t
							const*> (a.first), static_cast<wchar_t
							const*> (a.first) + static_cast<ptrdiff_t> (a.second),
							static_cast<char
							const*> (b.first), static_cast<char
							const*> (b.first) + static_cast<ptrdiff_t> (b.second));
					}
					else
					{
						result = std::lexicographical_compare(static_cast<wchar_t
							const*> (a.first), static_cast<wchar_t
							const*> (a.first) + static_cast<ptrdiff_t> (a.second),
							static_cast<wchar_t
							const*> (b.first), static_cast<wchar_t
							const*> (b.first) + static_cast<ptrdiff_t> (b.second));
					}
				}

				return result;
			}
		};

		explicit ParentIterator(NtfsIndex
			const* const index, key_type
			const& key) noexcept : index(index), key(key), state(0), iteration(0) {}

		[[nodiscard]] unsigned int attributes() const noexcept
		{
			return ptrs.record->stdinfo.attributes();
		}

		[[nodiscard]] bool empty() const noexcept
		{
			return !this->index;
		}

		bool next()
		{
			return ++ * this, !this->empty();
		}

		[[nodiscard]] value_type
			const& operator* () const noexcept
		{
			return this->result;
		}

		[[nodiscard]] value_type
			const* operator->() const noexcept
		{
			return &this->result;
		}

		[[nodiscard]] unsigned short icomponent() const noexcept
		{
			return this->iteration;
		}

		[[nodiscard]] bool operator==(this_type
			const& other) const noexcept
		{
			return this->index == other.index && this->key == other.key && this->state == other.state;
		}

		[[nodiscard]] bool operator!=(this_type const& other) const noexcept
		{
			return !(*this == other);
		}

		/// Advance to the next path component. Implementation in ntfs_index_impl.hpp.
		this_type& operator++();
	};

	size_t get_path(key_type key, std::tvstring& result, bool const name_only,
		unsigned int* attributes = nullptr) const;

	[[nodiscard]] size_info const& get_sizes(key_type const& key) const;

	[[nodiscard]] standard_info const& get_stdinfo(unsigned int const frn) const;

	/// Match files/directories against a filter function.
	/// @param func Callback invoked for each match
	/// @param path Working buffer for path construction
	/// @param match_paths Include full paths in matching
	/// @param match_streams Include alternate data streams
	/// @param match_attributes Include NTFS attributes
	template <class F>
	void matches(F func, std::tvstring& path, bool const match_paths,
		bool const match_streams, bool const match_attributes) const
	{
		Matcher<F&> matcher = {this, func, match_paths, match_streams, match_attributes, &path, 0};
		return matcher(kRootFRS);
	}
};

// std::is_scalar specializations for NtfsIndex nested types (MSVC optimization)
#ifdef _XMEMORY_
namespace std
{
	template <> struct is_scalar<NtfsIndex::StandardInfo> : is_pod<NtfsIndex::StandardInfo> {};
	template <> struct is_scalar<NtfsIndex::NameInfo> : is_pod<NtfsIndex::NameInfo> {};
	template <> struct is_scalar<NtfsIndex::StreamInfo> : is_pod<NtfsIndex::StreamInfo> {};
	template <> struct is_scalar<NtfsIndex::LinkInfo> : is_pod<NtfsIndex::LinkInfo> {};
	template <> struct is_scalar<NtfsIndex::Record> : is_pod<NtfsIndex::Record> {};
}
#endif

// Include implementation details (header-only pattern)
#include "ntfs_index_impl.hpp"

#endif // UFFS_NTFS_INDEX_HPP