#pragma once
// ============================================================================
// SearchResult and Results - Search Result Data Structures
// ============================================================================
// Extracted from UltraFastFileSearch.cpp
// 
// SearchResult: Compact representation of a single search match
// Results: Container for search results with index management
//
// Dependencies (must be included before this header):
// - NtfsIndex (from ntfs_index.hpp or monolith)
// - memheap_vector (from monolith)
// - intrusive_ptr (from intrusive_ptr.hpp or monolith)
// ============================================================================

#pragma pack(push, 1)
struct SearchResult
{
	typedef unsigned short index_type;
	typedef unsigned short depth_type;
	explicit SearchResult(NtfsIndex::key_type const key, depth_type const depth) : _key(key), _depth(depth) {}

	NtfsIndex::key_type key() const
	{
		return this->_key;
	}

	depth_type depth() const
	{
		return static_cast<depth_type>(this->_depth);
	}

	NtfsIndex::key_type::index_type index() const
	{
		return this->_key.index();
	}

	void index(NtfsIndex::key_type::index_type const value)
	{
		this->_key.index(value);
	}

private:
	NtfsIndex::key_type _key;
	depth_type _depth;
};
#pragma pack(pop)

#ifdef _MSC_VER
__declspec(align(0x40))
#endif
class Results : memheap_vector<SearchResult>
{
	typedef Results this_type;
	typedef memheap_vector<value_type, allocator_type> base_type;
	typedef std::vector<intrusive_ptr<NtfsIndex volatile const>> Indexes;
	typedef std::vector<std::pair<Indexes::value_type::value_type*, SearchResult::index_type>> IndicesInUse;
	Indexes indexes /*to keep alive */;
	IndicesInUse indices_in_use /*to keep alive */;

public:
	typedef base_type::allocator_type allocator_type;
	typedef base_type::value_type value_type;
	typedef base_type::reverse_iterator iterator;
	typedef base_type::const_reverse_iterator const_iterator;
	typedef base_type::size_type size_type;

	Results() : base_type() {}

	explicit Results(allocator_type const& alloc) : base_type(alloc) {}

	iterator begin()
	{
		return this->base_type::rbegin();
	}

	const_iterator begin() const
	{
		return this->base_type::rbegin();
	}

	iterator end()
	{
		return this->base_type::rend();
	}

	const_iterator end() const
	{
		return this->base_type::rend();
	}

	using base_type::capacity;

	size_type size() const
	{
		return this->end() - this->begin();
	}

	SearchResult::index_type save_index(Indexes::value_type::element_type* const index)
	{
		auto j = std::lower_bound(this->indices_in_use.begin(), this->indices_in_use.end(), std::make_pair(index, IndicesInUse::value_type::second_type()));
		if (j == this->indices_in_use.end() || j->first != index)
		{
			this->indexes.push_back(index);
			j = this->indices_in_use.insert(j, IndicesInUse::value_type(index, static_cast<IndicesInUse::value_type::second_type>(this->indexes.size() - 1)));
		}
		return j->second;
	}

	Indexes::value_type::element_type* item_index(size_t const i) const
	{
		return this->ith_index((*this)[i].index());
	}

	Indexes::value_type::element_type* ith_index(value_type::index_type const i) const
	{
		return this->indexes[i];
	}

	value_type const& operator[](size_t const i) const
	{
		return this->begin()[static_cast<ptrdiff_t>(i)];
	}

	void reserve(size_t const n)
	{
		(void)n;
		this->base_type::reserve(n);
	}

	void push_back(Indexes::value_type::element_type* const index, base_type::const_reference value)
	{
		this->base_type::push_back(value);
		(*(this->base_type::end() - 1)).index(this->save_index(index));
	}

	void clear()
	{
		this->base_type::clear();
		this->indexes.clear();
		this->indices_in_use.clear();
	}

	void swap(this_type& other)
	{
		this->base_type::swap(static_cast<base_type&>(other));
		this->indexes.swap(other.indexes);
		this->indices_in_use.swap(other.indices_in_use);
	}

	friend void swap(this_type& a, this_type& b)
	{
		return a.swap(b);
	}
};

