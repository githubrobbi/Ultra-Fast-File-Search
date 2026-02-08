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
    #include "../core/packed_file_size.hpp"
    #include "../core/standard_info.hpp"
    #include "../core/ntfs_record_types.hpp"
    #include "../core/ntfs_key_type.hpp"
    #include "mapping_pair_iterator.hpp"

    class NtfsIndex : public RefCounted < NtfsIndex>
    {
	typedef NtfsIndex this_type;

		// small_t is now in ntfs_record_types.hpp
		template <class T = void>
		using small_t = ::uffs::small_t<T>;

		// file_size_type is now in packed_file_size.hpp
		typedef ::uffs::file_size_type file_size_type;

		static unsigned int IsWow64Process_();

		// StandardInfo is now in standard_info.hpp
		typedef ::uffs::StandardInfo StandardInfo;

		// SizeInfo is now in packed_file_size.hpp
		typedef ::uffs::SizeInfo SizeInfo;

		// NameInfo is now in ntfs_record_types.hpp
		typedef ::uffs::NameInfo NameInfo;

		// LinkInfo is now in ntfs_record_types.hpp
		typedef ::uffs::LinkInfo LinkInfo;

		// StreamInfo is now in ntfs_record_types.hpp
		typedef ::uffs::StreamInfo StreamInfo;

		// ChildInfo is now in ntfs_record_types.hpp
		typedef ::uffs::ChildInfo ChildInfo;

		friend struct std::is_scalar<StandardInfo>;
		friend struct std::is_scalar<NameInfo>;
		friend struct std::is_scalar<LinkInfo>;
		friend struct std::is_scalar<StreamInfo>;

		typedef std::codecvt<std::tstring::value_type, char, int /*std::mbstate_t*/ > CodeCvt;
		typedef vector_with_fast_size<LinkInfo> LinkInfos;
		typedef vector_with_fast_size<StreamInfo> StreamInfos;
		// Record is now in ntfs_record_types.hpp
		typedef ::uffs::Record Record;
		typedef vector_with_fast_size<Record> Records;
		typedef std::vector < unsigned int > RecordsLookup;

		typedef vector_with_fast_size<ChildInfo> ChildInfos;
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

		// key_type_internal is now in ntfs_key_type.hpp
		typedef ::uffs::key_type_internal key_type_internal;

		// mapping_pair_iterator is now in mapping_pair_iterator.hpp

		// === Internal helpers declared here, implemented in ntfs_index_impl.hpp ===

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

	public:
		// Public type aliases for callers
		typedef key_type_internal key_type;
		typedef StandardInfo standard_info;
		typedef NameInfo name_info;
		typedef SizeInfo size_info;

		// Publicly visible statistics / configuration
		atomic_namespace::atomic<long long> reserved_clusters;
		value_initialized<long long> mft_zone_start, mft_zone_end;
		value_initialized<unsigned int> cluster_size;
		value_initialized<unsigned int> mft_record_size;
		value_initialized<unsigned int> mft_capacity;

		// Construction / lifetime
		NtfsIndex(std::tvstring value);
		~NtfsIndex();

		// Initialization and lifecycle
		[[nodiscard]] bool init_called() const noexcept;
		void init();
		void set_finished(unsigned int const& result);

		// Volatile helpers for lock_ptr and multi-threaded access
		[[nodiscard]] NtfsIndex*       unvolatile() volatile noexcept;
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
		[[nodiscard]] void*  volume() const volatile noexcept;
		[[nodiscard]] atomic_namespace::recursive_mutex& get_mutex() const volatile noexcept;
		[[nodiscard]] Speed  speed() const volatile noexcept;
		[[nodiscard]] std::tvstring const& root_path() const volatile noexcept;
		[[nodiscard]] unsigned int get_finished() const volatile noexcept;
		[[nodiscard]] bool cancelled() const volatile noexcept;
		void cancel() volatile noexcept;
		[[nodiscard]] uintptr_t finished_event() const noexcept;

			// Capacity reservation
			void reserve(unsigned int records);

			void preload_concurrent(unsigned long long virtual_offset,
				void* buffer,
				size_t size) volatile;

			void load(unsigned long long virtual_offset,
				void* buffer,
				size_t size,
				unsigned long long skipped_begin,
				unsigned long long skipped_end);

	  void report_speed(unsigned long long const size, clock_t const tfrom, clock_t const tto);

	  struct file_pointers
	  {
		  Records::value_type const* record;
		  LinkInfos::value_type const* link;
		  StreamInfos::value_type const* stream;
		  [[nodiscard]] key_type parent() const noexcept
		  {
			  return key_type(link->parent, static_cast<key_type::name_info_type>(~key_type::name_info_type()), static_cast<key_type::stream_info_type>(~key_type::stream_info_type()));
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

	  size_t get_path(key_type key, std::tvstring& result, bool const name_only, unsigned int* attributes = nullptr) const;

	  [[nodiscard]] size_info const& get_sizes(key_type const& key) const;

	  [[nodiscard]] standard_info const& get_stdinfo(unsigned int const frn) const;

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

// Include implementation details (header-only pattern)
#include "ntfs_index_impl.hpp"

#endif // UFFS_NTFS_INDEX_HPP