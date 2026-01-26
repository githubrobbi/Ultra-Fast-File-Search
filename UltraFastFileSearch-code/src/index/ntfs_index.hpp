#pragma once
// NtfsIndex class - extracted from UltraFastFileSearch.cpp
// This class manages NTFS index data for fast file searching

#include <Windows.h>
#include <tchar.h>
#include <climits>
#include <iterator>
#include <vector>
#include <codecvt>

#include "../util/intrusive_ptr.hpp"
#include "../util/atomic_compat.hpp"
#include "../util/handle.hpp"
#include "../util/containers.hpp"
#include "../io/overlapped.hpp"
#include "../core/ntfs_types.hpp"
#include "../util/buffer.hpp"
#include "../util/path.hpp"
#include "../util/append_directional.hpp"
#include "../util/error_utils.hpp"
#include "../io/winnt_types.hpp"
#include "../io/io_priority.hpp"
#include "../util/type_traits_ext.hpp"
#include "../core/file_attributes_ext.hpp"
#include "mapping_pair_iterator.hpp"

class NtfsIndex : public RefCounted < NtfsIndex>
{
	typedef NtfsIndex this_type;
	template < class = void > struct small_t
	{
		typedef unsigned int type;
	};
#pragma pack(push, 1)
	class file_size_type
	{
		unsigned int low;
		unsigned short high;
		typedef file_size_type this_type;
	public:
		file_size_type() noexcept : low(), high() {}

		file_size_type(unsigned long long
			const value) noexcept : low(static_cast<unsigned int> (value)), high(static_cast<unsigned short> (value >> (sizeof(low) * CHAR_BIT))) {}

		[[nodiscard]] operator unsigned long long() const noexcept
		{
			return static_cast<unsigned long long> (this->low) | (static_cast<unsigned long long> (this->high) << (sizeof(this->low) * CHAR_BIT));
		}

		file_size_type operator+=(this_type
			const value) noexcept
		{
			return *this = static_cast<unsigned long long> (*this) + static_cast<unsigned long long> (value);
		}

		file_size_type operator-=(this_type
			const value) noexcept
		{
			return *this = static_cast<unsigned long long> (*this) - static_cast<unsigned long long> (value);
		}

		[[nodiscard]] bool operator!() const noexcept
		{
			return !this->low && !this->high;
		}
	};

	static unsigned int IsWow64Process_()
	{
		return ::IsWow64Process(GetCurrentProcess(), &(BOOL&) value_initialized<BOOL>()) && ((BOOL&) value_initialized<BOOL>());
	}

	struct StandardInfo
	{
		unsigned long long
			created,
			written,
			accessed           : 0x40 - 6,
			//is_system        : 1,
			//is_directory     : 1,
			//is_sparse        : 1,
			//is_compressed    : 1,
			//is_encrypted     : 1,
			//is_reparse       : 1,
			is_readonly        : 1,
			is_archive         : 1,
			is_system          : 1,
			is_hidden          : 1,
			is_offline         : 1,
			is_notcontentidx   : 1,
			is_noscrubdata     : 1,
			is_integretystream : 1,
			is_pinned          : 1,
			is_unpinned        : 1,
			is_directory       : 1,
			is_compressed      : 1,
			is_encrypted       : 1,
			is_sparsefile      : 1,
			is_reparsepoint    : 1;

		// FILE_ATTRIBUTE_* constants are in file_attributes_ext.hpp

		[[nodiscard]] unsigned long attributes() const noexcept
		{
			//return (this->is_system     ? FILE_ATTRIBUTE_SYSTEM              : 0U) |
			//	(this->is_directory       ? FILE_ATTRIBUTE_DIRECTORY           : 0U) |
			//	(this->is_sparse          ? FILE_ATTRIBUTE_SPARSE_FILE         : 0U) |
			//	(this->is_compressed      ? FILE_ATTRIBUTE_COMPRESSED          : 0U) |
			//	(this->is_encrypted       ? FILE_ATTRIBUTE_ENCRYPTED           : 0U) |
			//	(this->is_reparse         ? FILE_ATTRIBUTE_REPARSE_POINT       : 0U);

			// #####
			return (this->is_readonly     ? FILE_ATTRIBUTE_READONLY            : 0U) |
				(this->is_archive         ? FILE_ATTRIBUTE_ARCHIVE             : 0U) |
				(this->is_system          ? FILE_ATTRIBUTE_SYSTEM              : 0U) |
				(this->is_hidden          ? FILE_ATTRIBUTE_HIDDEN              : 0U) |
				(this->is_offline         ? FILE_ATTRIBUTE_OFFLINE             : 0U) |
				(this->is_notcontentidx   ? FILE_ATTRIBUTE_NOT_CONTENT_INDEXED : 0U) |
				(this->is_noscrubdata     ? FILE_ATTRIBUTE_NO_SCRUB_DATA       : 0U) |
				(this->is_integretystream ? FILE_ATTRIBUTE_INTEGRITY_STREAM    : 0U) |
				(this->is_pinned          ? FILE_ATTRIBUTE_PINNED              : 0U) |
				(this->is_unpinned        ? FILE_ATTRIBUTE_UNPINNED            : 0U) |
				(this->is_directory       ? FILE_ATTRIBUTE_DIRECTORY           : 0U) |
				(this->is_compressed      ? FILE_ATTRIBUTE_COMPRESSED          : 0U) |
				(this->is_encrypted       ? FILE_ATTRIBUTE_ENCRYPTED           : 0U) |
				(this->is_sparsefile      ? FILE_ATTRIBUTE_SPARSE_FILE         : 0U) |
				(this->is_reparsepoint    ? FILE_ATTRIBUTE_REPARSE_POINT       : 0U);
		}

		void attributes(unsigned long
			const value) noexcept
		{
			//this->is_system        = !!(value & FILE_ATTRIBUTE_SYSTEM);
			//this->is_directory     = !!(value & FILE_ATTRIBUTE_DIRECTORY);
			//this->is_sparse        = !!(value & FILE_ATTRIBUTE_SPARSE_FILE);
			//this->is_compressed    = !!(value & FILE_ATTRIBUTE_COMPRESSED);
			//this->is_encrypted     = !!(value & FILE_ATTRIBUTE_ENCRYPTED);
			//this->is_reparse       = !!(value & FILE_ATTRIBUTE_REPARSE_POINT);

			this->is_readonly        = !!(value & FILE_ATTRIBUTE_READONLY);
			this->is_archive         = !!(value & FILE_ATTRIBUTE_ARCHIVE);
			this->is_system          = !!(value & FILE_ATTRIBUTE_SYSTEM);
			this->is_hidden          = !!(value & FILE_ATTRIBUTE_HIDDEN);
			this->is_offline         = !!(value & FILE_ATTRIBUTE_OFFLINE);
			this->is_notcontentidx   = !!(value & FILE_ATTRIBUTE_NOT_CONTENT_INDEXED);
			this->is_noscrubdata     = !!(value & FILE_ATTRIBUTE_NO_SCRUB_DATA);
			this->is_integretystream = !!(value & FILE_ATTRIBUTE_INTEGRITY_STREAM);
			this->is_pinned          = !!(value & FILE_ATTRIBUTE_PINNED);
			this->is_unpinned        = !!(value & FILE_ATTRIBUTE_UNPINNED);
			this->is_directory       = !!(value & FILE_ATTRIBUTE_DIRECTORY);
			this->is_compressed      = !!(value & FILE_ATTRIBUTE_COMPRESSED);
			this->is_encrypted       = !!(value & FILE_ATTRIBUTE_ENCRYPTED);
			this->is_sparsefile      = !!(value & FILE_ATTRIBUTE_SPARSE_FILE);
			this->is_reparsepoint    = !!(value & FILE_ATTRIBUTE_REPARSE_POINT);
		}
	};


	struct SizeInfo
	{
		file_size_type length, allocated, bulkiness;
		value_initialized < unsigned int>::type treesize;
	};

	friend struct std::is_scalar<StandardInfo>;
	struct NameInfo
	{
		small_t<size_t>::type _offset;
		[[nodiscard]] bool ascii() const noexcept
		{
			return !!(this->_offset & 1U);
		}

		void ascii(bool
			const value) noexcept
		{
			this->_offset = static_cast<small_t<size_t>::type> ((this->_offset & static_cast<small_t<size_t>::type> (~static_cast<small_t<size_t>::type> (1U))) | (value ? 1U : small_t<size_t>::type()));
		}

		[[nodiscard]] small_t<size_t>::type offset() const noexcept
		{
			small_t<size_t>::type result = this->_offset >> 1;
			if (result == (static_cast<small_t<size_t>::type> (negative_one) >> 1))
			{
				result = static_cast<small_t<size_t>::type> (negative_one);
			}

			return result;
		}

		void offset(small_t<size_t>::type
			const value) noexcept
		{
			this->_offset = (value << 1) | (this->_offset & 1U);
		}

		unsigned char length;
	};

	friend struct std::is_scalar<NameInfo>;
	struct LinkInfo
	{
		LinkInfo() : next_entry(negative_one)
		{
			this->name.offset(negative_one);
		}

		typedef small_t<size_t>::type next_entry_type;
		next_entry_type next_entry;
		NameInfo name;
		unsigned int parent;
	};

	friend struct std::is_scalar<LinkInfo>;
	struct StreamInfo : SizeInfo
	{
		StreamInfo() : SizeInfo(), next_entry(), name(), type_name_id() {}

		typedef small_t<size_t>::type next_entry_type;
		next_entry_type next_entry;
		NameInfo name;
		unsigned char is_sparse : 1;
		unsigned char is_allocated_size_accounted_for_in_main_stream : 1;
		unsigned char type_name_id : CHAR_BIT - 2 /*zero if and only if $I30:$INDEX_ROOT or $I30:$INDEX_ALLOCATION */;
	};

	friend struct std::is_scalar<StreamInfo>;
	typedef std::codecvt<std::tstring::value_type, char, int /*std::mbstate_t*/ > CodeCvt;
	typedef vector_with_fast_size<LinkInfo> LinkInfos;
	typedef vector_with_fast_size<StreamInfo> StreamInfos;
	struct Record;
	typedef vector_with_fast_size<Record> Records;
	typedef std::vector < unsigned int > RecordsLookup;
	struct ChildInfo
	{
		ChildInfo() : next_entry(negative_one), record_number(negative_one), name_index(negative_one) {}

		typedef small_t<size_t>::type next_entry_type;
		next_entry_type next_entry;
		small_t<Records::size_type>::type record_number;
		unsigned short name_index;
	};

	typedef vector_with_fast_size<ChildInfo> ChildInfos;
	struct Record
	{
		StandardInfo stdinfo;
		unsigned short name_count /*<= 1024 < 2048 */, stream_count /*<= 4106?<8192 */;
		ChildInfos::value_type::next_entry_type first_child;
		LinkInfos::value_type first_name;
		StreamInfos::value_type first_stream;
		Record() : stdinfo(), name_count(), stream_count(), first_child(negative_one), first_name(), first_stream()
		{
			this->first_stream.name.offset(negative_one);
			this->first_stream.next_entry = negative_one;
		}
	};
#pragma pack(pop)
	friend struct std::is_scalar<Record>;
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
	atomic_namespace::atomic < unsigned int > _finished;
	atomic_namespace::atomic<size_t> _total_names_and_streams;
	value_initialized < unsigned int > _expected_records;
	atomic_namespace::atomic<bool> _cancelled;
	atomic_namespace::atomic < unsigned int > _records_so_far, _preprocessed_so_far;
	std::vector<Speed> _perf_reports_circ /*circular buffer */; value_initialized<size_t> _perf_reports_begin;
	atomic_namespace::spin_atomic<Speed> _perf_avg_speed;
#pragma pack(push, 1)
		struct key_type_internal
	{
		// max 1023 hardlinks
		// max 4107 streams
		enum
		{
			name_info_bits = 10, stream_info_bits = 13, index_bits = sizeof(unsigned int) * CHAR_BIT - (name_info_bits + stream_info_bits)
		};

		typedef unsigned int frs_type;
		typedef unsigned short name_info_type;
		typedef unsigned short stream_info_type;
		typedef unsigned short index_type;
		[[nodiscard]] frs_type frs() const noexcept
		{
			frs_type
				const result = this->_frs;
			return result;
		}

		[[nodiscard]] name_info_type name_info() const noexcept
		{
			name_info_type
				const result = this->_name_info;
			return result == ((static_cast<name_info_type> (1) << name_info_bits) - 1) ? ~name_info_type() : result;
		}

		[[nodiscard]] name_info_type stream_info() const noexcept
		{

			stream_info_type
				const result = this->_stream_info;
			return result == ((static_cast<stream_info_type> (1) << stream_info_bits) - 1) ? ~stream_info_type() : result;
		}

		void stream_info(name_info_type
			const value) noexcept
		{
			this->_stream_info = value;
		}

		[[nodiscard]] index_type index() const noexcept
		{
			index_type
				const result = this->_index;
			return result == ((static_cast<index_type> (1) << index_bits) - 1) ? ~index_type() : result;
		}

		void index(name_info_type
			const value) noexcept
		{
			this->_index = value;
		}

		explicit key_type_internal(frs_type
			const frs, name_info_type
			const name_info, stream_info_type
			const stream_info) noexcept : _frs(frs), _name_info(name_info), _stream_info(stream_info), _index(negative_one) {}

		[[nodiscard]] bool operator==(key_type_internal
			const& other) const noexcept
		{
			return this->_frs == other._frs && this->_name_info == other._name_info && this->_stream_info == other._stream_info;
		}

	private:
		frs_type _frs;
		name_info_type _name_info : name_info_bits;
		stream_info_type _stream_info : stream_info_bits;
		index_type _index : index_bits;
	};
#pragma pack(pop)

	// mapping_pair_iterator is now in mapping_pair_iterator.hpp

	Records::iterator at(size_t
		const frs, Records::iterator* const existing_to_revalidate = nullptr)
	{
		if (frs >= this->records_lookup.size())
		{
			this->records_lookup.resize(frs + 1, ~RecordsLookup::value_type());
		}

		RecordsLookup::iterator
			const k = this->records_lookup.begin() + static_cast<ptrdiff_t> (frs);
		if (!~*k)
		{
			ptrdiff_t
				const j = (existing_to_revalidate ? *existing_to_revalidate : this->records_data.end()) - this->records_data.begin();
			*k = static_cast<unsigned int> (this->records_data.size());
			this->records_data.resize(this->records_data.size() + 1);
			if (existing_to_revalidate)
			{
				*existing_to_revalidate = this->records_data.begin() + j;
			}
		}

		return this->records_data.begin() + static_cast<ptrdiff_t> (*k);
	}

	template < class Me>
	static typename propagate_const<Me, Records::value_type>::type* _find(Me* const me, key_type_internal::frs_type
		const frs)
	{
		typedef typename propagate_const<Me, Records::value_type>::type* pointer_type;
		pointer_type result;
		if (frs < me->records_lookup.size())
		{
			RecordsLookup::value_type
				const islot = me->records_lookup[frs];
			// The complicated logic here is to remove the 'imul' instruction...
			result = fast_subscript(me->records_data.begin(), islot);
		}
		else
		{
			result = me->records_data.empty() ? nullptr : &*(me->records_data.end() - 1) + 1;
		}

		return result;
	}

	Records::value_type* find(key_type_internal::frs_type
		const frs)
	{
		return this->_find(this, frs);
	}

	Records::value_type
		const* find(key_type_internal::frs_type
			const frs) const
	{
		return this->_find(this, frs);
	}

	ChildInfos::value_type* childinfo(Records::value_type* const i)
	{
		return this->childinfo(i->first_child);
	}

	ChildInfos::value_type
		const* childinfo(Records::value_type
			const* const i) const
	{
		return this->childinfo(i->first_child);
	}

	ChildInfos::value_type* childinfo(ChildInfo::next_entry_type
		const i)
	{
		return !~i ? nullptr : fast_subscript(this->childinfos.begin(), i);
	}

	ChildInfos::value_type
		const* childinfo(ChildInfo::next_entry_type
			const i) const
	{
		return !~i ? nullptr : fast_subscript(this->childinfos.begin(), i);
	}

	LinkInfos::value_type* nameinfo(LinkInfo::next_entry_type
		const i)
	{
		return !~i ? nullptr : fast_subscript(this->nameinfos.begin(), i);
	}

	LinkInfos::value_type
		const* nameinfo(LinkInfo::next_entry_type
			const i) const
	{
		return !~i ? nullptr : fast_subscript(this->nameinfos.begin(), i);
	}

	LinkInfos::value_type* nameinfo(Records::value_type* const i)
	{
		return ~i->first_name.name.offset() ? &i->first_name : nullptr;
	}

	LinkInfos::value_type
		const* nameinfo(Records::value_type
			const* const i) const
	{
		return ~i->first_name.name.offset() ? &i->first_name : nullptr;
	}

	StreamInfos::value_type* streaminfo(StreamInfo::next_entry_type
		const i)
	{
		return !~i ? nullptr : fast_subscript(this->streaminfos.begin(), i);
	}

	StreamInfos::value_type
		const* streaminfo(StreamInfo::next_entry_type
			const i) const
	{
		return !~i ? nullptr : fast_subscript(this->streaminfos.begin(), i);
	}

	StreamInfos::value_type* streaminfo(Records::value_type* const i)
	{
		assert(~i->first_stream.name.offset() || (!i->first_stream.name.length && !i->first_stream.length));
		return ~i->first_stream.name.offset() ? &i->first_stream : nullptr;
	}

	StreamInfos::value_type
		const* streaminfo(Records::value_type
			const* const i) const
	{
		assert(~i->first_stream.name.offset() || (!i->first_stream.name.length && !i->first_stream.length));
		return ~i->first_stream.name.offset() ? &i->first_stream : nullptr;
	}

public: typedef key_type_internal key_type;
	  typedef StandardInfo standard_info;
	  typedef NameInfo name_info;
	  typedef SizeInfo size_info;
	  atomic_namespace::atomic < long long > reserved_clusters;
	  value_initialized < long long > mft_zone_start, mft_zone_end;
	  value_initialized < unsigned int > cluster_size;
	  value_initialized < unsigned int > mft_record_size;
	  value_initialized < unsigned int > mft_capacity;
	  NtfsIndex(std::tvstring value) : _root_path(value), _finished_event(CreateEvent(nullptr, TRUE, FALSE, nullptr)), _finished(), _total_names_and_streams(0), _cancelled(false), _records_so_far(0), _preprocessed_so_far(0), _perf_reports_circ(1 << 6), _perf_avg_speed(Speed()), reserved_clusters(0) {}
	  ~NtfsIndex() {}

	  [[nodiscard]] bool init_called() const noexcept
	  {
		  return this->_init_called;
	  }

	  void init()
	  {
		  this->_init_called = true;
		  std::tvstring path_name = this->_root_path;
		  deldirsep(path_name);
		  if (!path_name.empty() && *path_name.begin() != _T('\\') && *path_name.begin() != _T('/'))
		  {
			  path_name.insert(static_cast<size_t> (0), _T("\\\\.\\"));
		  }

		  HANDLE
			  const h = CreateFile(path_name.c_str(), FILE_READ_DATA | FILE_READ_ATTRIBUTES | SYNCHRONIZE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
		  CheckAndThrow(h != INVALID_HANDLE_VALUE);
		  Handle volume(h);
		  winnt::IO_STATUS_BLOCK iosb;
		  struct : winnt::FILE_FS_ATTRIBUTE_INFORMATION
		  {
			  unsigned char buf[MAX_PATH];
		  }

		  info = {};

		  winnt::NTSTATUS status = winnt::NtQueryVolumeInformationFile(volume.value, &iosb, &info, sizeof(info), 5);
		  if (status != 0)
		  {
			  CppRaiseException(winnt::RtlNtStatusToDosError(status));
		  }

		  if (info.FileSystemNameLength != 4 * sizeof(*info.FileSystemName) || std::char_traits<TCHAR>::compare(info.FileSystemName, _T("NTFS"), 4))
		  {
			  CppRaiseException(ERROR_UNRECOGNIZED_VOLUME);
		  }

		  if (false)
		  {
			  IoPriority::set(reinterpret_cast<uintptr_t> (volume.value), winnt::IoPriorityLow);
		  }

		  volume.swap(this->_volume);
		  this->_tbegin = clock();
	  }

	  void set_finished(unsigned int
		  const& result)
	  {
		  SetEvent(this->_finished_event);
		  this->_finished = result;
	  }

	  [[nodiscard]] NtfsIndex* unvolatile() volatile noexcept
	  {
		  return const_cast<NtfsIndex*> (this);
	  }

	  [[nodiscard]] NtfsIndex
		  const* unvolatile() const volatile noexcept
	  {
		  return const_cast<NtfsIndex*> (this);
	  }

	  [[nodiscard]] size_t total_names_and_streams() const noexcept
	  {
		  return this->_total_names_and_streams.load(atomic_namespace::memory_order_relaxed);
	  }

	  [[nodiscard]] size_t total_names_and_streams() const volatile noexcept
	  {
		  return this->_total_names_and_streams.load(atomic_namespace::memory_order_acquire);
	  }

	  [[nodiscard]] size_t total_names() const noexcept
	  {
		  return this->nameinfos.size();
	  }

	  [[nodiscard]] size_t expected_records() const noexcept
	  {
		  return this->_expected_records;
	  }

	  [[nodiscard]] size_t preprocessed_so_far() const volatile noexcept
	  {
		  return this->_preprocessed_so_far.load(atomic_namespace::memory_order_acquire);
	  }

	  [[nodiscard]] size_t preprocessed_so_far() const noexcept
	  {
		  return this->_preprocessed_so_far.load(atomic_namespace::memory_order_relaxed);
	  }

	  [[nodiscard]] size_t records_so_far() const volatile noexcept
	  {
		  return this->_records_so_far.load(atomic_namespace::memory_order_acquire);
	  }

	  [[nodiscard]] size_t records_so_far() const noexcept
	  {
		  return this->_records_so_far.load(atomic_namespace::memory_order_relaxed);
	  }

	  [[nodiscard]] void* volume() const volatile noexcept
	  {
		  return this->_volume.value;
	  }

	  [[nodiscard]] atomic_namespace::recursive_mutex& get_mutex() const volatile noexcept
	  {
		  return this->unvolatile()->_mutex;
	  }

	  [[nodiscard]] Speed speed() const volatile noexcept
	  {
		  Speed total;
		  total = this->_perf_avg_speed.load(atomic_namespace::memory_order_acquire);
		  return total;
	  }

	  [[nodiscard]] std::tvstring const& root_path() const volatile noexcept
	  {
		  return const_cast<std::tvstring
			  const&> (this->_root_path);
	  }

	  [[nodiscard]] unsigned int get_finished() const volatile noexcept
	  {
		  return this->_finished.load();
	  }

	  [[nodiscard]] bool cancelled() const volatile noexcept
	  {
		  this_type
			  const* const me = this->unvolatile();
		  return me->_cancelled.load(atomic_namespace::memory_order_acquire);
	  }

	  void cancel() volatile noexcept
	  {
		  this_type* const me = this->unvolatile();
		  me->_cancelled.store(true, atomic_namespace::memory_order_release);
	  }

	  [[nodiscard]] uintptr_t finished_event() const noexcept
	  {
		  return reinterpret_cast<uintptr_t> (this->_finished_event.value);
	  }

	  void reserve(unsigned int records)
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

		  catch (std::bad_alloc&) {}
	  }

	  void preload_concurrent(unsigned long long
		  const virtual_offset, void* const buffer, size_t
		  const size) volatile
	  {
		  unsigned int max_frs_plus_one = 0;
		  unsigned int
			  const mft_record_size = this->mft_record_size;
		  unsigned int mft_record_size_log2 = 0;
		  for (;;)
		  {
			  unsigned int
				  const v = mft_record_size_log2 + 1;
			  if (!(mft_record_size >> v))
			  {
				  break;
			  }

			  mft_record_size_log2 = v;
	  }

		  assert((1U << mft_record_size_log2) == mft_record_size && "MFT record size not a power of 2");
		  unsigned int
			  const mft_record_size_pow2_mod_mask = mft_record_size - 1;
		  for (size_t i = virtual_offset & mft_record_size_pow2_mod_mask /*this should only occur if cluster size < MFT record size */ ? mft_record_size - virtual_offset & mft_record_size_pow2_mod_mask : 0; i + mft_record_size <= size; i += mft_record_size)
		  {
			  unsigned int
				  const frs = static_cast<unsigned int> ((virtual_offset + i) >> mft_record_size_log2);
			  ntfs::FILE_RECORD_SEGMENT_HEADER* const frsh = reinterpret_cast<ntfs::FILE_RECORD_SEGMENT_HEADER*> (&static_cast<unsigned char*> (buffer)[i]);
			  if (frsh->MultiSectorHeader.Magic == 'ELIF')
			  {
				  if (frsh->MultiSectorHeader.unfixup(mft_record_size))
				  {
					  unsigned int
						  const frs_base = frsh->BaseFileRecordSegment ? static_cast<unsigned int> (frsh->BaseFileRecordSegment) : frs;
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

	  void load(unsigned long long
		  const virtual_offset, void* const buffer, size_t
		  const size, unsigned long long
		  const skipped_begin, unsigned long long
		  const skipped_end)
	  {
		  unsigned int
			  const mft_record_size = this->mft_record_size;
		  unsigned int mft_record_size_log2 = 0;
		  for (;;)
		  {
			  unsigned int
				  const v = mft_record_size_log2 + 1;
			  if (!(mft_record_size >> v))
			  {
				  break;
			  }

			  mft_record_size_log2 = v;
		  }

		  assert((1U << mft_record_size_log2) == mft_record_size && "MFT record size not a power of 2");
		  unsigned int
			  const mft_record_size_pow2_mod_mask = mft_record_size - 1;
		  if (size % mft_record_size)
		  {
			  throw std::runtime_error("Cluster size is smaller than MFT record size; split MFT records (over multiple clusters) not supported. Defragmenting your MFT may sometimes avoid this condition.");
		  }

		  if (skipped_begin || skipped_end)
		  {
			  this->_records_so_far.fetch_add(static_cast<unsigned int> ((skipped_begin + skipped_end) >> mft_record_size_log2));
		  }

		  ChildInfo
			  const empty_child_info;
		  for (size_t i = virtual_offset & mft_record_size_pow2_mod_mask /*this should only occur if cluster size < MFT record size */ ? mft_record_size - virtual_offset & mft_record_size_pow2_mod_mask : 0; i + mft_record_size <= size; i += mft_record_size, this->_records_so_far.fetch_add(1, atomic_namespace::memory_order_acq_rel))
		  {
			  unsigned int
				  const frs = static_cast<unsigned int> ((virtual_offset + i) >> mft_record_size_log2);
			  ntfs::FILE_RECORD_SEGMENT_HEADER* const frsh = reinterpret_cast<ntfs::FILE_RECORD_SEGMENT_HEADER*> (&static_cast<unsigned char*> (buffer)[i]);
			  if (frsh->MultiSectorHeader.Magic == 'ELIF' && !!(frsh->Flags & ntfs::FRH_IN_USE))
			  {
				  unsigned int
					  const frs_base = frsh->BaseFileRecordSegment ? static_cast<unsigned int> (frsh->BaseFileRecordSegment) : frs;
				  auto base_record = this->at(frs_base);
				  void
					  const* const frsh_end = frsh->end(mft_record_size);
				  for (ntfs::ATTRIBUTE_RECORD_HEADER
					  const*
					  ah = frsh->begin(); ah < frsh_end && ah->Type != ntfs::AttributeTypeCode::AttributeNone && ah->Type != ntfs::AttributeTypeCode::AttributeEnd; ah = ah->next())
				  {
					  switch (ah->Type)
					  {
					  case ntfs::AttributeTypeCode::AttributeStandardInformation:
						  if (ntfs::STANDARD_INFORMATION
							  const* const fn = static_cast<ntfs::STANDARD_INFORMATION
							  const*> (ah->Resident.GetValue()))
						  {
							  base_record->stdinfo.created  = fn->CreationTime;
							  base_record->stdinfo.written  = fn->LastModificationTime;
							  base_record->stdinfo.accessed = fn->LastAccessTime;
							  base_record->stdinfo.attributes(fn->FileAttributes | ((frsh->Flags & ntfs::FRH_DIRECTORY) ? FILE_ATTRIBUTE_DIRECTORY : 0));
						  }

						  break;
					  case ntfs::AttributeTypeCode::AttributeFileName:
						  if (ntfs::FILENAME_INFORMATION
							  const* const fn = static_cast<ntfs::FILENAME_INFORMATION
							  const*> (ah->Resident.GetValue()))
						  {
							  unsigned int
								  const frs_parent = static_cast<unsigned int> (fn->ParentDirectory);
							  if (fn->Flags != 0x02 /*FILE_NAME_DOS */)
							  {
								  if (LinkInfos::value_type* const si = this->nameinfo(&*base_record))
								  {
									  size_t
										  const link_index = this->nameinfos.size();
									  this->nameinfos.push_back(base_record->first_name);
									  base_record->first_name.next_entry = static_cast<LinkInfos::value_type::next_entry_type> (link_index);
								  }

								  LinkInfo* const info = &base_record->first_name;
								  info->name.offset(static_cast<unsigned int> (this->names.size()));
								  info->name.length = static_cast<unsigned char> (fn->FileNameLength);
								  bool
									  const ascii = is_ascii(fn->FileName, fn->FileNameLength);
								  info->name.ascii(ascii);
								  info->parent = frs_parent;
								  append_directional(this->names, fn->FileName, fn->FileNameLength, ascii ? 1 : 0);
								  if (frs_parent != frs_base)
								  {
									  Records::iterator
										  const parent            = this->at(frs_parent, &base_record);
									  size_t
										  const child_index       = this->childinfos.size();
									  this->childinfos.push_back(empty_child_info);
									  ChildInfo* const child_info = &this->childinfos.back();
									  child_info->record_number   = frs_base;
									  child_info->name_index      = base_record->name_count;
									  child_info->next_entry      = parent->first_child;
									  parent->first_child         = static_cast<ChildInfos::value_type::next_entry_type> (child_index);
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
							  mapping_pair_iterator mpi(ah, reinterpret_cast<unsigned char
								  const*> (frsh_end) - reinterpret_cast<unsigned char
								  const*> (ah));
							  for (mapping_pair_iterator::vcn_type current_vcn = mpi->next_vcn; !mpi.is_final();)
							  {
								  ++mpi;
								  if (mpi->current_lcn)
								  {
									  mapping_pair_iterator::lcn_type
										  intersect_mft_zone_begin = this->mft_zone_start,
										  intersect_mft_zone_end = this->mft_zone_end;
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

						  bool
							  const is_primary_attribute = !(ah->IsNonResident && ah->NonResident.LowestVCN);
						  if (is_primary_attribute)
						  {
							  bool
								  const isdir = (ah->Type == ntfs::AttributeTypeCode::AttributeBitmap || ah->Type == ntfs::AttributeTypeCode::AttributeIndexRoot || ah->Type == ntfs::AttributeTypeCode::AttributeIndexAllocation) && ah->NameLength == 4 && memcmp(ah->name(), _T("$I30"), sizeof(*ah->name()) * 4) == 0;
							  unsigned char
								  const name_length = isdir ? static_cast<unsigned char> (0) : ah->NameLength;
							  unsigned char
								  const type_name_id = static_cast<unsigned char> (isdir ? 0 : static_cast<int>(ah->Type) >> (CHAR_BIT / 2));
							  StreamInfo* info = nullptr;
							  if (StreamInfos::value_type* const si = this->streaminfo(&*base_record))
							  {
								  if (isdir)	// Do we want to merge this with another attribute? (e.g. AttributeIndexAllocation with AttributeIndexRoot, or secondary entry with primary entry)
								  {
									  for (StreamInfos::value_type* k = si; k; k = this->streaminfo(k->next_entry))
									  {
										  if (k->type_name_id == type_name_id && k->name.length == name_length &&
											  (name_length == 0 || std::equal(ah->name(), ah->name() + static_cast<ptrdiff_t> (ah->NameLength), this->names.begin() + static_cast<ptrdiff_t> (k->name.offset()))))
										  {
											  info = k;
											  break;
										  }
									  }
								  }

								  if (!info)
								  {
									  size_t
										  const stream_index = this->streaminfos.size();
									  this->streaminfos.push_back(*si);
									  si->next_entry = static_cast<small_t<size_t>::type> (stream_index);
								  }
							  }

							  if (!info)
							  {
								  info = &base_record->first_stream;
								  info->allocated                                      = 0;	// We have to initialize this because we add to it later
								  info->length                                         = 0;	// We have to initialize this because we add to it later
								  info->bulkiness                                      = 0;	// We have to initialize this because we add to it later
								  info->treesize                                       = 0;	// We have to initialize this because we add to it later
								  // We also have to initialize the name because it _identifies_ the attribute
								  info->is_sparse                                      = 0;
								  info->is_allocated_size_accounted_for_in_main_stream = 0;
								  info->type_name_id                                   = type_name_id;
								  info->name.length                                    = name_length;
								  if (isdir)
								  {
									  // Suppress name
									  info->name.offset(0);
								  }
								  else
								  {
									  info->name.offset(static_cast<unsigned int> (this->names.size()));
									  bool
										  const ascii = is_ascii(ah->name(), ah->NameLength);
									  info->name.ascii(ascii);
									  append_directional(this->names, ah->name(), ah->NameLength, ascii ? 1 : 0);
								  }

								  ++base_record->stream_count;
								  this->_total_names_and_streams.fetch_add(base_record->name_count, atomic_namespace::memory_order_acq_rel);
							  }

							  bool
								  const is_badclus_bad = frs_base == 0x000000000008 && ah->NameLength == 4 && memcmp(ah->name(), _T("$Bad"), sizeof(*ah->name()) * 4) == 0;
							  bool
								  const is_sparse = !!(ah->Flags & 0x8000);
							  if (is_sparse)
							  {
								  info->is_sparse |= 0x1;
							  }

							  info->allocated += ah->IsNonResident ?
								  ah->NonResident.CompressionUnit ?
								  static_cast<file_size_type> (ah->NonResident.CompressedSize) :
								  static_cast<file_size_type> (is_badclus_bad /*$BadClus */ ?
									  ah->NonResident.InitializedSize /*actually this is still wrong... should be looking at VCNs */ :
									  ah->NonResident.AllocatedSize) :
								  0;
							  info->length += ah->IsNonResident ? static_cast<file_size_type> (is_badclus_bad /*$BadClus */ ? ah->NonResident.InitializedSize /*actually this is still wrong... */ : ah->NonResident.DataSize) : ah->Resident.ValueLength;
							  info->bulkiness += info->allocated;
							  info->treesize = isdir;
						  }

						  break;
					  }
					  }
				  }

				  // fprintf(stderr, "%llx\n", frsh->BaseFileRecordSegment);
			  }
		  }

		  unsigned int
			  const records_so_far = this->_records_so_far.load(atomic_namespace::memory_order_acquire);
		  bool
			  const finished = records_so_far >= this->mft_capacity;
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

			  int
				  const nbuf = safe_stprintf(buf,
					  _T("%s\trecords_data\twidth = %2u\tsize = %8I64u\tcapacity = %8I64u\n") _T("%s\trecords_lookup\twidth = %2u\tsize = %8I64u\tcapacity = %8I64u\n") _T("%s\tnames\twidth = %2u\tsize = %8I64u\tcapacity = %8I64u\t%8I64u bytes wasted\n") _T("%s\tnameinfos\twidth = %2u\tsize = %8I64u\tcapacity = %8I64u\n") _T("%s\tstreaminfos\twidth = %2u\tsize = %8I64u\tcapacity = %8I64u\n") _T("%s\tchildinfos\twidth = %2u\tsize = %8I64u\tcapacity = %8I64u\n"),
					  this->_root_path.c_str(), static_cast<unsigned> (sizeof(*this->records_data.begin())), static_cast<unsigned long long> (this->records_data.size()), static_cast<unsigned long long> (this->records_data.capacity()),
					  this->_root_path.c_str(), static_cast<unsigned> (sizeof(*this->records_lookup.begin())), static_cast<unsigned long long> (this->records_lookup.size()), static_cast<unsigned long long> (this->records_lookup.capacity()),
					  this->_root_path.c_str(), static_cast<unsigned> (sizeof(*this->names.begin())), static_cast<unsigned long long> (this->names.size()), static_cast<unsigned long long> (this->names.capacity()), static_cast<unsigned long long> (names_wasted_chars),
					  this->_root_path.c_str(), static_cast<unsigned> (sizeof(*this->nameinfos.begin())), static_cast<unsigned long long> (this->nameinfos.size()), static_cast<unsigned long long> (this->nameinfos.capacity()),
					  this->_root_path.c_str(), static_cast<unsigned> (sizeof(*this->streaminfos.begin())), static_cast<unsigned long long> (this->streaminfos.size()), static_cast<unsigned long long> (this->streaminfos.capacity()),
					  this->_root_path.c_str(), static_cast<unsigned> (sizeof(*this->childinfos.begin())), static_cast<unsigned long long> (this->childinfos.size()), static_cast<unsigned long long> (this->childinfos.capacity()));
			  buf[_countof(buf) - 1] = _T('\0');  // Ensure null termination for static analysis
			  if (nbuf > 0)
			  {
				  OutputDebugString(buf);
			  }

			  struct
			  {
				  typedef SizeInfo PreprocessResult;
				  NtfsIndex* me;
				  typedef std::vector < unsigned long long > Scratch;
				  Scratch scratch;
				  size_t depth;
				  PreprocessResult operator()(Records::value_type* const fr, key_type::name_info_type
					  const name_info, unsigned short
					  const total_names)
				  {
					  size_t
						  const old_scratch_size = scratch.size();
					  PreprocessResult result;
					  if (fr)
					  {
						  PreprocessResult children_size;
						  ++depth;
						  for (ChildInfos::value_type* i = me->childinfo(fr); i && ~i->record_number; i = me->childinfo(i->next_entry))
						  {
							  Records::value_type* const fr2 = me->find(i->record_number);
							  if (fr2 != fr) /*root directory is the only one that is a child of itself */
							  {
								  PreprocessResult
									  const
									  subresult = this->operator()(fr2, fr2->name_count - static_cast<size_t> (1) - i->name_index, fr2->name_count);
								  scratch.push_back(subresult.bulkiness);
								  children_size.length    += subresult.length;
								  children_size.allocated += subresult.allocated;
								  children_size.bulkiness += subresult.bulkiness;
								  children_size.treesize  += subresult.treesize;
							  }
						  }

						  --depth;
						  std::make_heap(scratch.begin() + static_cast<ptrdiff_t> (old_scratch_size), scratch.end());
						  unsigned long long
							  const threshold = children_size.allocated / 100;
						  // Algorithm: All files whose sizes are<X% of the current folder's size are assumed to contribute to the folder's bulkiness.
						  for (auto i = scratch.end(); i != scratch.begin() + static_cast<ptrdiff_t> (old_scratch_size);)
						  {
							  std::pop_heap(scratch.begin() + static_cast<ptrdiff_t> (old_scratch_size), i);
							  --i;
							  if (*i < threshold)
							  {
								  break;
							  }

							  children_size.bulkiness = children_size.bulkiness - *i;
						  }

						  if (depth == 0)
						  {
							  children_size.allocated += static_cast<unsigned long long> (me->reserved_clusters) * me->cluster_size;
						  }

						  result = children_size;
						  struct Accumulator
						  {
							  static unsigned long long delta_impl(unsigned long long
								  const value, unsigned short
								  const i, unsigned short
								  const n)
							  {
								  return value * (i + 1) / n - value * i / n;
							  }

							  static unsigned long long delta(unsigned long long
								  const value, unsigned short
								  const i, unsigned short
								  const n)
							  {
								  return n != 1 ? n != 2 ? delta_impl(value, i, n) : i != 1 ? delta_impl(value, ((void)(assert(i == 0)), 0), n) : delta_impl(value, i, n) : delta_impl(value, ((void)(assert(i == 0)), 0), n);
							  }
						  };

						  StreamInfos::value_type* default_stream = nullptr, * compressed_default_stream_to_merge = nullptr;
						  unsigned long long default_allocated_delta = 0, compressed_default_allocated_delta = 0;
						  for (StreamInfos::value_type* k = me->streaminfo(fr); k; k = me->streaminfo(k->next_entry))
						  {
							  bool
								  const is_data_attribute = (k->type_name_id << (CHAR_BIT / 2)) == static_cast<int>(ntfs::AttributeTypeCode::AttributeData);
							  bool
								  const is_default_stream = is_data_attribute && !k->name.length;
							  unsigned long long
								  const
								  allocated_delta = Accumulator::delta(k->is_allocated_size_accounted_for_in_main_stream ? static_cast<file_size_type> (0) : k->allocated, name_info, total_names),
								  bulkiness_delta = Accumulator::delta(k->bulkiness, name_info, total_names);
							  if (is_default_stream)
							  {
								  default_stream = k;
								  default_allocated_delta += allocated_delta;
							  }

							  bool
								  const is_compression_reparse_point = is_data_attribute &&
								  k->name.length &&
								  (k->name.ascii() ?
									  memcmp(reinterpret_cast<char
										  const*> (&me->names[k->name.offset()]), "WofCompressedData", 17 * sizeof(char)) == 0 :
									  memcmp(reinterpret_cast<wchar_t
										  const*> (&me->names[k->name.offset()]), L"WofCompressedData", 17 * sizeof(wchar_t)) == 0
									  );
							  unsigned long long
								  const
								  length_delta = Accumulator::delta(is_compression_reparse_point ? static_cast<file_size_type> (0) : k->length, name_info, total_names);
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

					  scratch.erase(scratch.begin() + static_cast<ptrdiff_t> (old_scratch_size), scratch.end());
					  return result;
				  }
			  }

			  preprocessor = { this
			  };

			  clock_t
				  const tbefore_preprocess = clock();
			  preprocessor(this->find(0x000000000005), 0, 1);
			  clock_t
				  const tfinish = clock();
			  Handle().swap(this->_volume);
			  if (false)
			  {
				  _ftprintf(stderr, _T("Finished: %s (%I64u ms total, %I64u ms preprocessing)\n"), this->_root_path.c_str(), (tfinish - this->_tbegin) * 1000ULL / CLOCKS_PER_SEC, (tfinish - tbefore_preprocess) * 1000ULL / CLOCKS_PER_SEC);
			  }
		  }

		  finished ? SetEvent(this->_finished_event) : ResetEvent(this->_finished_event);
	  }

	  void report_speed(unsigned long long
		  const size, clock_t
		  const tfrom, clock_t
		  const tto)
	  {
		  clock_t
			  const duration = tto - tfrom;
		  Speed
			  const speed(size, duration);
		  this->_perf_avg_speed.fetch_add(speed, atomic_namespace::memory_order_acq_rel);
		  Speed
			  const prev = this->_perf_reports_circ[this->_perf_reports_begin];
		  this->_perf_reports_circ[this->_perf_reports_begin] = speed;
		  this->_perf_reports_begin = (this->_perf_reports_begin + 1) % this->_perf_reports_circ.size();
	  }

	  struct file_pointers
	  {
		  Records::value_type
			  const* record;
		  LinkInfos::value_type
			  const* link;
		  StreamInfos::value_type
			  const* stream;
		  [[nodiscard]] key_type parent() const noexcept
		  {
			  return key_type(link->parent /*... | 0 | 0 (since we want the first name of all ancestors)*/, static_cast<key_type::name_info_type> (~key_type::name_info_type()), static_cast<key_type::stream_info_type> (~key_type::stream_info_type()));
		  }
	  };

	  [[nodiscard]] file_pointers get_file_pointers(key_type key) const
	  {
		  bool wait_for_finish = false;	// has performance penalty
		  if (wait_for_finish && WaitForSingleObject(this->_finished_event, 0) == WAIT_TIMEOUT)
		  {
			  throw std::logic_error("Need to wait for indexing to be finished");
		  }

		  file_pointers result;
		  result.record = nullptr;
		  if (~key.frs())
		  {
			  Records::value_type
				  const* const fr = this->find(key.frs());
			  unsigned short ji = 0;
			  for (LinkInfos::value_type
				  const* j = this->nameinfo(fr); j; j = this->nameinfo(j->next_entry), ++ji)
			  {
				  key_type::name_info_type
					  const name_info = key.name_info();
				  if (name_info == USHRT_MAX || ji == name_info)
				  {
					  unsigned short ki = 0;
					  for (StreamInfos::value_type
						  const* k = this->streaminfo(fr); k; k = this->streaminfo(k->next_entry), ++ki)
					  {
						  key_type::stream_info_type
							  const stream_info = key.stream_info();
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
			  return key.frs() == 0x000000000005;
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

		[[nodiscard]] bool operator!=(this_type
			const& other) const noexcept
		{
			return !(*this == other);
		}

		this_type& operator++()
		{
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wimplicit-fallthrough"
#endif
				switch (state)
				{
					for (;;)
					{
				case 0:;
					ptrs = index->get_file_pointers(key);
					if (!is_root())
					{
						if (!ptrs.stream->type_name_id)
						{
							result.first = _T("\\");
							result.second = 1;
							result.ascii = false;
							if (result.second)
							{
								state = 1;
								break;
							}
							[[fallthrough]];
				case 1:;
						}
					}

					if (!this->iteration)
					{
						if (is_attribute() && ptrs.stream->type_name_id < sizeof(ntfs::attribute_names) / sizeof(*ntfs::attribute_names))
						{
							result.first = ntfs::attribute_names[ptrs.stream->type_name_id].data;
							result.second = ntfs::attribute_names[ptrs.stream->type_name_id].size;
							result.ascii = false;
							if (result.second)
							{
								state = 2;
								break;
							}
							[[fallthrough]];
				case 2:;
					result.first = _T(":");
					result.second = 1;
					result.ascii = false;
					if (result.second)
					{
						state = 3;
						break;
					  }

				  [[fallthrough]];
				  case 3:;
						  }

						  if (ptrs.stream->name.length)
						  {
							  result.first = &index->names[ptrs.stream->name.offset()];
							  result.second = ptrs.stream->name.length;
							  result.ascii = ptrs.stream->name.ascii();
							  if (result.second)
							  {
								  state = 4;
								  break;
							  }

				  [[fallthrough]];
				  case 4:;
						  }

						  if (ptrs.stream->name.length || is_attribute())
						  {
							  result.first = _T(":");
							  result.second = 1;
							  result.ascii = false;
							  if (result.second)
							  {
								  state = 5;
								  break;
							  }

				  [[fallthrough]];
				  case 5:;
						  }
					  }

					  if (!this->iteration || !is_root())
					  {
						  result.first = &index->names[ptrs.link->name.offset()];
						  result.second = ptrs.link->name.length;
						  result.ascii = ptrs.link->name.ascii();
						  if (result.second)
						  {
							  state = 6;
							  break;
						  }
					  }

				  [[fallthrough]];
				  case 6:;
					  if (is_root())
					  {
						  this->index = nullptr;
						  break;
					  }

					  state = 0;
					  key = ptrs.parent();
					  ++this->iteration;
					  }

				  default:
					  ;
				  }
#ifdef __clang__
#pragma clang diagnostic pop
#endif
					  return *this;
		  }
	  };

	  size_t get_path(key_type key, std::tvstring& result, bool
		  const name_only, unsigned int* attributes = nullptr) const
	  {
		  size_t
			  const old_size = result.size();
		  for (ParentIterator pi(this, key); pi.next() && !(name_only && pi.icomponent());)
		  {
			  if (attributes)
			  {
				  *attributes = pi.attributes();
				  attributes = nullptr;
			  }

			  TCHAR
				  const* const s = static_cast<TCHAR
				  const*> (pi->first);
			  if (name_only || !(pi->second == 1 && (pi->ascii ? *static_cast<char
				  const*> (static_cast<void
					  const*> (s)) : *s) == _T('.')))
			  {
				  append_directional(result, s, pi->second, pi->ascii ? -1 : 0, true);
			  }
		  }

		  std::reverse(result.begin() + static_cast<ptrdiff_t> (old_size), result.end());
		  return result.size() - old_size;
	  }

	  [[nodiscard]] size_info const& get_sizes(key_type
		  const& key) const
	  {
		  StreamInfos::value_type
			  const* k;
		  Records::value_type
			  const* const fr = this->find(key.frs());
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

	  [[nodiscard]] standard_info const& get_stdinfo(unsigned int
		  const frn) const
	  {
		  return this->find(frn)->stdinfo;
	  }

	  template < class F>
	  void matches(F func, std::tvstring& path, bool
		  const match_paths, bool
		  const match_streams, bool
		  const match_attributes) const
	  {
		  Matcher < F&> matcher = { this, func, match_paths, match_streams, match_attributes, &path, 0
		  };

		  return matcher(0x000000000005);
	  }

private: template < class F>
	struct Matcher
{
	NtfsIndex
		const* me;
	F func;
	bool match_paths;
	bool match_streams;
	bool match_attributes;
	std::tvstring* path;
	size_t basename_index_in_path;
	NameInfo name;
	size_t depth;

	void operator()(key_type::frs_type
		const frs)
	{
		if (frs < me->records_lookup.size())
		{
			TCHAR
				const dirsep = getdirsep();
			std::tvstring temp;
			Records::value_type
				const* const i = me->find(frs);
			unsigned short ji = 0;
			for (LinkInfos::value_type
				const* j = me->nameinfo(i); j; j = me->nameinfo(j->next_entry), ++ji)
			{
				size_t
					const old_basename_index_in_path = basename_index_in_path;
				basename_index_in_path = path->size();
				temp.clear();
				append_directional(temp, &dirsep, 1, 0);
				if (!(match_paths && frs == 0x00000005))
				{
					append_directional(temp, &me->names[j->name.offset()], j->name.length, j->name.ascii() ? -1 : 0);
				}

				this->operator()(frs, ji, temp.data(), temp.size());
				basename_index_in_path = old_basename_index_in_path;
			}
		}
	}

	void operator()(key_type::frs_type
		const frs, key_type::name_info_type
		const name_info, TCHAR
		const stream_prefix[], size_t
		const stream_prefix_size)
	{
		bool
			const match_paths_or_streams = match_paths || match_streams || match_attributes;
		bool
			const buffered_matching = stream_prefix_size || match_paths_or_streams;
		if (frs < me->records_lookup.size() && (frs == 0x00000005 || frs >= 0x00000010 || this->match_attributes))
		{
			Records::value_type
				const* const fr = me->find(frs);
			key_type new_key(frs, name_info, 0);
			ptrdiff_t traverse = 0;
			for (StreamInfos::value_type
				const* k = me->streaminfo(fr); k; k = me->streaminfo(k->next_entry), new_key.stream_info(new_key.stream_info() + 1))
			{
				assert(k->name.offset() <= me->names.size());
				bool
					const is_attribute = k->type_name_id && (k->type_name_id << (CHAR_BIT / 2)) != static_cast<int>(ntfs::AttributeTypeCode::AttributeData);
				if (!match_attributes && is_attribute)
				{
					continue;
				}

				size_t
					const old_size = path->size();
				if (stream_prefix_size)
				{
					path->append(stream_prefix, stream_prefix_size);
				}

				if (match_paths_or_streams)
				{
					if ((fr->stdinfo.attributes() & FILE_ATTRIBUTE_DIRECTORY) && frs != 0x00000005)
					{
						path->push_back(_T('\\'));
					}
				}

				if (match_streams || match_attributes)
				{
					if (k->name.length)
					{
						path->push_back(_T(':'));
						append_directional(*path, k->name.length ? &me->names[k->name.offset()] : nullptr, k->name.length, k->name.ascii() ? -1 : 0);
					}

					if (is_attribute)
					{
						if (!k->name.length)
						{
							path->push_back(_T(':'));
						}

						path->push_back(_T(':')), path->append(ntfs::attribute_names[k->type_name_id].data, ntfs::attribute_names[k->type_name_id].size);
					}
				}

				bool ascii;
				size_t name_offset, name_length;
				if (buffered_matching)
				{
					name_offset = match_paths ? 0 : static_cast<unsigned int> (basename_index_in_path);
					name_length = path->size() - name_offset;
					ascii = false;
				}
				else
				{
					name_offset = name.offset();
					name_length = name.length;
					ascii = name.ascii();
				}

				if (frs != 0x00000005 || ((depth > 0) ^ (k->type_name_id == 0))) /*this is to list the root directory at the top level, but its attributes at level 1 */
				{
					traverse += func((buffered_matching ? path->data() : &*me->names.begin()) + static_cast<ptrdiff_t> (name_offset), name_length, ascii, new_key, depth);
				}

				if (buffered_matching)
				{
					path->erase(old_size, path->size() - old_size);
				}
			}

			if ((frs != 0x00000005 || depth == 0) && traverse > 0)
			{
				size_t
					const old_size = path->size();
				NameInfo
					const old_name = name;
				size_t
					const old_basename_index_in_path = basename_index_in_path;
				++depth;
				if (buffered_matching)
				{
					if (match_paths_or_streams)
					{
						path->push_back(_T('\\'));
					}

					basename_index_in_path = path->size();
				}

				unsigned short ii = 0;
				for (ChildInfos::value_type
					const* i = me->childinfo(fr); i && ~i->record_number; i = me->childinfo(i->next_entry), ++ii)
				{
					unsigned short name_index = i->name_index;
					unsigned int record_number = i->record_number;
				START:
#pragma warning(push)
#pragma warning(disable: 4456)
					if (bool i = true /*this is to HIDE the outer 'i', to prevent accessing it directly (because we put in a dummy entry sometimes) */)
#pragma warning(pop)
					{
						Records::value_type
							const* const fr2 = me->find(record_number);
						unsigned short ji_target = name_index;
						unsigned short ji = 0;
						for (LinkInfos::value_type
							const* j = me->nameinfo(fr2); j; j = me->nameinfo(j->next_entry), ++ji)
						{
							if (j->parent == frs && ji == ji_target)
							{
								if (buffered_matching)
								{
									append_directional(*path, &me->names[j->name.offset()], j->name.length, j->name.ascii() ? -1 : 0);
								}

								name = j->name;
								this->operator()(static_cast<key_type::frs_type> (record_number), ji, nullptr, 0);
								if (buffered_matching)
								{
									path->erase(path->end() - static_cast<ptrdiff_t> (j->name.length), path->end());
								}
							}
						}
					}

				if (record_number == 0x00000006 && depth == 1)
				{
					name_index = 0;
					record_number = 0x00000005;
					goto START;
				}
				}

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

};

// std::is_scalar specializations for NtfsIndex nested types
namespace std
{
#ifdef _XMEMORY_
#define X(...) template < > struct is_scalar<__VA_ARGS__>: is_pod<__VA_ARGS__> {}
	X(NtfsIndex::StandardInfo);
	X(NtfsIndex::NameInfo);
	X(NtfsIndex::StreamInfo);
	X(NtfsIndex::LinkInfo);
	X(NtfsIndex::Record);
#undef X
#endif
}