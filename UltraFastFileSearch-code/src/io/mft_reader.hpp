/**
 * @file mft_reader.hpp
 * @brief NTFS MFT (Master File Table) async reader implementation
 *
 * This file contains the OverlappedNtfsMftReadPayload class which handles
 * asynchronous reading of the NTFS MFT using I/O completion ports.
 *
 * Dependencies (must be defined before including this header):
 * - Overlapped class (from overlapped.hpp)
 * - IoCompletionPort class (from io_completion_port.hpp)
 * - atomic_namespace (from atomic_compat.hpp)
 * - intrusive_ptr, RefCounted (from intrusive_ptr.hpp)
 * - lock() function (from lock_ptr.hpp)
 * - Handle class (forward declared, defined in monolith)
 * - NtfsIndex class (forward declared, defined in monolith)
 * - get_retrieval_pointers() function (defined in monolith before this include)
 * - CheckAndThrow() function (defined in monolith before this include)
 * - CppRaiseException() function (defined in monolith before this include)
 * - CStructured_Exception class (defined in monolith before this include)
 */

#ifndef UFFS_MFT_READER_HPP
#define UFFS_MFT_READER_HPP

#include "overlapped.hpp"
#include "io_completion_port.hpp"
#include "../util/atomic_compat.hpp"
#include "../util/intrusive_ptr.hpp"
#include "../util/lock_ptr.hpp"

#include <vector>
#include <ctime>
#include <climits>
#include <algorithm>
#include <stdexcept>

// Forward declarations for types defined in the monolith
class Handle;
class NtfsIndex;

class OverlappedNtfsMftReadPayload : public Overlapped
{
protected: struct RetPtr
{
    unsigned long long vcn, cluster_count;
    long long lcn;
    atomic_namespace::atomic < unsigned long long > skip_begin, skip_end;
    RetPtr(unsigned long long
        const vcn, unsigned long long
        const cluster_count, unsigned long long
        const lcn) : vcn(vcn), cluster_count(cluster_count), lcn(lcn), skip_begin(0), skip_end(0) {}

    RetPtr(RetPtr
        const& other) : vcn(other.vcn), cluster_count(other.cluster_count), lcn(other.lcn), skip_begin(other.skip_begin.load(atomic_namespace::memory_order_relaxed)), skip_end(other.skip_end.load(atomic_namespace::memory_order_relaxed)) {}

    RetPtr& operator=(RetPtr
        const& other)
    {
        this->vcn = other.vcn;
        this->cluster_count = other.cluster_count;
        this->lcn = other.lcn;
        this->skip_begin.store(other.skip_begin.load(atomic_namespace::memory_order_relaxed));
        this->skip_end.store(other.skip_end.load(atomic_namespace::memory_order_relaxed));
        return *this;
    }
};

         typedef std::vector<RetPtr> RetPtrs;
         typedef std::vector < unsigned char > Bitmap;
         IoCompletionPort volatile* iocp;
         Handle closing_event;
         RetPtrs bitmap_ret_ptrs, data_ret_ptrs;
         unsigned int cluster_size;
         unsigned long long read_block_size;
         atomic_namespace::atomic<RetPtrs::size_type > jbitmap, nbitmap_chunks_left, jdata;
         atomic_namespace::atomic < unsigned int > valid_records;
         Bitmap mft_bitmap;	// may be unavailable -- don't fail in that case!
         intrusive_ptr < NtfsIndex volatile > p;
public: class ReadOperation; ~OverlappedNtfsMftReadPayload() override = default;

      OverlappedNtfsMftReadPayload(IoCompletionPort volatile& iocp, intrusive_ptr < NtfsIndex volatile > p, Handle
          const& closing_event) : Overlapped(), iocp(&iocp), closing_event(closing_event), cluster_size(), read_block_size(1 << 20), jbitmap(0), nbitmap_chunks_left(0), jdata(0), valid_records(0)
      {
          using std::swap;
          swap(p, this->p);
      }

      void queue_next() volatile;
      int operator()(size_t /*size*/, uintptr_t /*key*/) override;
      virtual void preopen() {}
};

class OverlappedNtfsMftReadPayload::ReadOperation : public Overlapped
{
    unsigned long long _voffset, _skipped_begin, _skipped_end;
    clock_t _time;
    static atomic_namespace::recursive_mutex recycled_mutex;
    static std::vector<std::pair<size_t, void*>> recycled;
    bool _is_bitmap;
    intrusive_ptr < OverlappedNtfsMftReadPayload volatile > q;
    static void* operator new(size_t n)
    {
        void* p;
        if (true)
        {
            { 	atomic_namespace::unique_lock<atomic_namespace::recursive_mutex > guard(recycled_mutex);
            size_t ifound = recycled.size();
            for (size_t i = 0; i != recycled.size(); ++i)
            {
                if (recycled[i].first >= n && (ifound >= recycled.size() || recycled[i].first <= recycled[ifound].first))
                {
                    ifound = i;
                }
            }

            if (ifound < recycled.size())
            {
                p = recycled[ifound].second;
                recycled.erase(recycled.begin() + static_cast<ptrdiff_t> (ifound));
            }
            else
            {
                p = nullptr;
            }
            }

            if (!p)
            {
                p = malloc(n) /*so we can use _msize() */;
            }
        }
        else
        {
            p = ::operator new(n);
        }

        return p;
    }

public:
    static void* operator new(size_t n, size_t m)
    {
        return operator new(n + m);
    }

    static void operator delete(void* p)
    {
        if (true)
        {
            atomic_namespace::unique_lock<atomic_namespace::recursive_mutex >(recycled_mutex), recycled.push_back(std::pair<size_t, void*>(_msize(p), p));
        }
        else
        {
            return ::operator delete(p);
        }
    }

    static void operator delete(void* p, size_t /*m*/)
    {
        return operator delete(p);
    }

	explicit ReadOperation(intrusive_ptr < OverlappedNtfsMftReadPayload volatile>
		const& q, bool
		const is_bitmap) : Overlapped(), _voffset(), _skipped_begin(), _skipped_end(), _time(clock()), _is_bitmap(is_bitmap), q(q) {}

	[[nodiscard]] unsigned long long voffset() const noexcept
	{
		return this->_voffset;
	}

	void voffset(unsigned long long value) noexcept
	{
		this->_voffset = value;
	}

	[[nodiscard]] unsigned long long skipped_begin() const noexcept
	{
		return this->_skipped_begin;
	}

	void skipped_begin(unsigned long long value) noexcept
	{
		this->_skipped_begin = value;
	}

	[[nodiscard]] unsigned long long skipped_end() const noexcept
	{
		return this->_skipped_end;
	}

	void skipped_end(unsigned long long value) noexcept
	{
		this->_skipped_end = value;
	}

	[[nodiscard]] clock_t time() const noexcept
	{
		return this->_time;
	}

	void time(clock_t value) noexcept
	{
		this->_time = value;
	}

	int operator()(size_t size, uintptr_t /*key*/) override
	{
		OverlappedNtfsMftReadPayload* const q = const_cast<OverlappedNtfsMftReadPayload*> (static_cast<OverlappedNtfsMftReadPayload volatile*> (this->q.get()));
		if (!q->p->cancelled())
		{
			this->q->queue_next();
			void* const buffer = this + 1;
			if (this->_is_bitmap)
			{
				size_t
					const records_per_bitmap_word = sizeof(*q->mft_bitmap.begin()) * CHAR_BIT;
				if (this->voffset() * CHAR_BIT <= q->p->mft_capacity)
				{
					static unsigned char
						const popcount[] = { 0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4 };

					unsigned int nrecords = 0;
					size_t n = size;
					if (this->voffset() + n >= q->p->mft_capacity / CHAR_BIT)
					{
						n = static_cast<size_t> (q->p->mft_capacity / CHAR_BIT - this->voffset());
					}

					for (size_t i = 0; i < n; ++i)
					{
						unsigned char
							const
							v = static_cast<unsigned char
							const*> (buffer)[i],
							vlow = static_cast<unsigned char> (v >> (CHAR_BIT / 2)),
							vhigh = static_cast<unsigned char> (v ^ (vlow << (CHAR_BIT / 2)));
						nrecords += popcount[vlow];
						nrecords += popcount[vhigh];
					}

					std::copy(static_cast<unsigned char
						const*> (buffer), static_cast<unsigned char
						const*> (buffer) + static_cast<ptrdiff_t> (n), q->mft_bitmap.begin() + static_cast<ptrdiff_t> (this->voffset()));
					q->valid_records.fetch_add(nrecords, atomic_namespace::memory_order_acq_rel);
				}

				if (q->nbitmap_chunks_left.fetch_sub(1, atomic_namespace::memory_order_acq_rel) == 1)
				{
					unsigned int
						const valid_records = q->valid_records.exchange(0 /*make sure this doesn't happen twice */, atomic_namespace::memory_order_acq_rel);
					lock(q->p)->reserve(valid_records);

					// Now, go remove records from the queue that we know are invalid...
					for (RetPtrs::iterator i = q->data_ret_ptrs.begin(); i != q->data_ret_ptrs.end(); ++i)
					{
						size_t
							const
							irecord = static_cast<size_t> (i->vcn * q->cluster_size / q->p->mft_record_size),
							nrecords = static_cast<size_t> (i->cluster_count * q->cluster_size / q->p->mft_record_size);
						size_t skip_records_begin, skip_records_end;
						// TODO: We're doing a bitmap search bit-by-bit here, which is slow...
						// maybe improve it at some point... but OTOH it's still much faster than I/O anyway, so whatever...
						for (skip_records_begin = 0; skip_records_begin != nrecords; ++skip_records_begin)
						{
							size_t
								const j = irecord + skip_records_begin,
								j1 = j / records_per_bitmap_word,
								j2 = j % records_per_bitmap_word;
							if (q->mft_bitmap[j1] & (1 << j2))
							{
								break;
							}
						}

						for (skip_records_end = 0; skip_records_end != nrecords - skip_records_begin; ++skip_records_end)
						{
							size_t
								const j = irecord + nrecords - 1 - skip_records_end,
								j1 = j / records_per_bitmap_word,
								j2 = j % records_per_bitmap_word;
							if (q->mft_bitmap[j1] & (1 << j2))
							{
								break;
							}
						}

						size_t
							skip_clusters_begin = static_cast<size_t> (static_cast<unsigned long long> (skip_records_begin) * q->p->mft_record_size / q->cluster_size),
							skip_clusters_end = static_cast<size_t> (static_cast<unsigned long long> (skip_records_end) * q->p->mft_record_size / q->cluster_size);
						if (skip_clusters_begin + skip_clusters_end > i->cluster_count)
						{
							throw std::logic_error("we should never be skipping more clusters than there are");
						}

						i->skip_begin.store(skip_clusters_begin, atomic_namespace::memory_order_release);
						i->skip_end.store(skip_clusters_end, atomic_namespace::memory_order_release);
					}
				}
			}
			else
			{
				unsigned long long
					const virtual_offset = this->voffset();
				q->p->preload_concurrent(virtual_offset, buffer, size);
				lock(q->p)->load(virtual_offset, buffer, size, this->skipped_begin(), this->skipped_end());
			}

			{

				lock(q->p)->report_speed(size, this->time(), clock());
			}
		}

		return -1;
	}
};


// Static member definitions (inline for header-only usage)
inline atomic_namespace::recursive_mutex OverlappedNtfsMftReadPayload::ReadOperation::recycled_mutex;
inline std::vector<std::pair<size_t, void*>> OverlappedNtfsMftReadPayload::ReadOperation::recycled;

inline void OverlappedNtfsMftReadPayload::queue_next() volatile
{
	OverlappedNtfsMftReadPayload
		const* const me = const_cast<OverlappedNtfsMftReadPayload
		const*> (this);
	bool handled = false;
	if (!handled)
	{
		size_t
			const jbitmap = this->jbitmap.fetch_add(1, atomic_namespace::memory_order_acq_rel);
		if (jbitmap < me->bitmap_ret_ptrs.size())
		{
			handled = true;
			RetPtrs::const_iterator
				const j = me->bitmap_ret_ptrs.begin() + static_cast<ptrdiff_t> (jbitmap);
			unsigned long long
				const
				skip_begin = j->skip_begin.load(atomic_namespace::memory_order_acquire),
				skip_end = j->skip_end.load(atomic_namespace::memory_order_acquire);
			unsigned int
				const cb = static_cast<unsigned int> ((j->cluster_count - (skip_begin + skip_end)) * me->cluster_size);
			intrusive_ptr<ReadOperation> p(new(cb) ReadOperation(this, true));
			p->offset((j->lcn + skip_begin) * static_cast<long long> (me->cluster_size));
			p->voffset((j->vcn + skip_begin) * me->cluster_size);
			p->skipped_begin(skip_begin * me->cluster_size);
			p->skipped_end(skip_end * me->cluster_size);
			p->time(clock());
			me->iocp->read_file(me->p->volume(), p.get() + 1, cb, p);
		}
		else if (jbitmap > me->bitmap_ret_ptrs.size())
		{
			// oops, increased multiple times... decrease to keep at max
			this->jbitmap.fetch_sub(1, atomic_namespace::memory_order_acq_rel);
		}
	}

	if (!handled)
	{
		size_t
			const jdata = this->jdata.fetch_add(1, atomic_namespace::memory_order_acq_rel);
		if (jdata < me->data_ret_ptrs.size())
		{
			handled = true;
			RetPtrs::const_iterator
				const j = me->data_ret_ptrs.begin() + static_cast<ptrdiff_t> (jdata);
			unsigned long long
				const
				skip_begin = j->skip_begin.load(atomic_namespace::memory_order_acquire),
				skip_end = j->skip_end.load(atomic_namespace::memory_order_acquire);
			unsigned int
				const cb = static_cast<unsigned int> ((j->cluster_count - (skip_begin + skip_end)) * me->cluster_size);
			intrusive_ptr<ReadOperation> p(new(cb) ReadOperation(this, false));
			p->offset((j->lcn + skip_begin) * static_cast<long long> (me->cluster_size));
			p->voffset((j->vcn + skip_begin) * me->cluster_size);
			p->skipped_begin(skip_begin * me->cluster_size);
			p->skipped_end(skip_end * me->cluster_size);
			p->time(clock());
			me->iocp->read_file(me->p->volume(), p.get() + 1, cb, p);
		}
		else if (jdata > me->data_ret_ptrs.size())
		{
			// oops, increased multiple times... decrease to keep at max
			this->jdata.fetch_sub(1, atomic_namespace::memory_order_acq_rel);
		}
	}
}


inline int OverlappedNtfsMftReadPayload::operator()(size_t /*size*/, uintptr_t key)
{
	int result = -1;
	intrusive_ptr<NtfsIndex> p = this->p->unvolatile();
	struct SetFinished
	{
		intrusive_ptr<NtfsIndex> p;
		unsigned int result;
		~SetFinished()
		{
			if (p)
			{
				p->set_finished(this->result);
			}
		}
	}

	set_finished = { p
	};

	try
	{
		if (!p->init_called())
		{
			p->init();
		}

		if (void* const volume = p->volume())
		{
			this->preopen();
			unsigned long br;
			NTFS_VOLUME_DATA_BUFFER info;
			CheckAndThrow(DeviceIoControl(volume, FSCTL_GET_NTFS_VOLUME_DATA, nullptr, 0, &info, sizeof(info), &br, nullptr));
			this->cluster_size = static_cast<unsigned int> (info.BytesPerCluster);
			p->cluster_size = info.BytesPerCluster;
			p->mft_record_size = info.BytesPerFileRecordSegment;
			p->mft_capacity = static_cast<unsigned int> (info.MftValidDataLength.QuadPart / info.BytesPerFileRecordSegment);
			p->mft_zone_start = info.MftZoneStart.QuadPart;
			p->mft_zone_end = info.MftZoneEnd.QuadPart;
			p->mft_zone_end = p->mft_zone_start;	// This line suppresses MFT zone inclusion in the "size on disk"
			p->reserved_clusters.store(info.TotalReserved.QuadPart + p->mft_zone_end - p->mft_zone_start);
			this->iocp->associate(volume, reinterpret_cast<uintptr_t> (&*p));
			typedef std::vector<std::pair < unsigned long long, long long >> RP;
			{ { 		long long llsize = 0;
			RP
				const ret_ptrs = get_retrieval_pointers((p->root_path() + std::tvstring(_T("$MFT::$BITMAP"))).c_str(), &llsize, info.MftStartLcn.QuadPart, this->p->mft_record_size);
			unsigned long long prev_vcn = 0;
			for (RP::const_iterator i = ret_ptrs.begin(); i != ret_ptrs.end(); ++i)
			{
				long long
					const clusters_left = static_cast<long long> (std::max(i->first, prev_vcn) - prev_vcn);
				unsigned long long n;
				for (long long m = 0; m < clusters_left; m += static_cast<long long> (n))
				{
					n = std::min(i->first - prev_vcn, 1 + (static_cast<unsigned long long> (this->read_block_size) - 1) / this->cluster_size);
					this->bitmap_ret_ptrs.push_back(RetPtrs::value_type(prev_vcn, n, i->second + m));
					prev_vcn += n;
				}
			}

			this->mft_bitmap.resize(static_cast<size_t> (llsize), static_cast<Bitmap::value_type> (~Bitmap::value_type()) /*default should be to read unused slots too */);
			this->nbitmap_chunks_left.store(this->bitmap_ret_ptrs.size(), atomic_namespace::memory_order_release);
				}

			{

				long long llsize = 0;
				RP
					const ret_ptrs = get_retrieval_pointers((p->root_path() + std::tvstring(_T("$MFT::$DATA"))).c_str(), &llsize, info.MftStartLcn.QuadPart, p->mft_record_size);
				if (ret_ptrs.empty())
				{
					CppRaiseException(ERROR_UNRECOGNIZED_VOLUME);
				}

				unsigned long long prev_vcn = 0;
				for (RP::const_iterator i = ret_ptrs.begin(); i != ret_ptrs.end(); ++i)
				{
					long long
						const clusters_left = static_cast<long long> (std::max(i->first, prev_vcn) - prev_vcn);
					unsigned long long n;
					for (long long m = 0; m < clusters_left; m += static_cast<long long> (n))
					{
						n = std::min(i->first - prev_vcn, 1 + (static_cast<unsigned long long> (this->read_block_size) - 1) / this->cluster_size);
						this->data_ret_ptrs.push_back(RetPtrs::value_type(prev_vcn, n, i->second + m));
						prev_vcn += n;
					}
				}
			}
			}

			{

				set_finished.p.reset();	// make sure we don't set this as finished anymore
				for (int concurrency = 0; concurrency < 2; ++concurrency)
				{
					this->queue_next();
				}
			}
		}
	}

	catch (CStructured_Exception& ex)
	{
		set_finished.result = ex.GetSENumber();
	}

	return result;
}

#endif // UFFS_MFT_READER_HPP