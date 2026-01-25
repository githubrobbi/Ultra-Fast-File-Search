// ============================================================================
// buffer.hpp - Raw Memory Buffer Class
// ============================================================================
// Extracted from UltraFastFileSearch.cpp monolith
// 
// This class provides a raw memory buffer using malloc/free/realloc.
// It supports STL-like interface with iterators and size/capacity tracking.
// ============================================================================

#ifndef UFFS_UTIL_BUFFER_HPP
#define UFFS_UTIL_BUFFER_HPP

#include <cstddef>
#include <cstdlib>
#include <memory>
#include <utility>

class buffer
{
	typedef buffer this_type;
	typedef void value_type_internal;
	typedef value_type_internal* pointer_internal;
	typedef size_t size_type_internal;
	typedef ptrdiff_t difference_type;
	pointer_internal p;
	size_type_internal c, n;
	void init()
	{
		if (!this->p && this->c)
		{
			using namespace std;
			this->p = malloc(this->c);
		}
	}

public:
	typedef value_type_internal value_type;
	typedef size_type_internal size_type;
	typedef pointer_internal pointer;
	typedef value_type const* const_pointer;
	typedef unsigned char& reference;
	typedef unsigned char const& const_reference;
	typedef unsigned char* iterator;
	typedef unsigned char const* const_iterator;

	~buffer()
	{
		using namespace std;
		free(this->p);
	}

	buffer() : p(), c(), n() {}

	explicit buffer(size_type const c) : p(), c(c), n()
	{
		this->init();
	}

	buffer(this_type const& other) : p(), c(other.c), n()
	{
		this->init();
		this->n = static_cast<size_type>(std::uninitialized_copy(other.begin(), other.end(), this->begin()) - this->begin());
	}

	pointer get() const
	{
		return this->p;
	}

	size_type size() const
	{
		return this->n;
	}

	size_type capacity() const
	{
		return this->c;
	}

	this_type& operator=(this_type other)
	{
		return other.swap(*this), *this;
	}

	pointer tail()
	{
		return static_cast<unsigned char*>(this->p) + static_cast<ptrdiff_t>(this->n);
	}

	const_pointer tail() const
	{
		return static_cast<unsigned char const*>(this->p) + static_cast<ptrdiff_t>(this->n);
	}

	void swap(this_type& other)
	{
		using std::swap;
		swap(this->p, other.p);
		swap(this->c, other.c);
		swap(this->n, other.n);
	}

	friend void swap(this_type& a, this_type& b)
	{
		return a.swap(b);
	}

	iterator begin()
	{
		return static_cast<iterator>(this->get());
	}

	const_iterator begin() const
	{
		return static_cast<const_iterator>(this->get());
	}

	iterator end()
	{
		return static_cast<iterator>(this->tail());
	}

	const_iterator end() const
	{
		return static_cast<const_iterator>(this->tail());
	}

	bool empty() const
	{
		return !this->n;
	}

	void clear()
	{
		buffer().swap(*this);
	}

	template<class T>
	T* emplace_back(size_type const size = sizeof(T))
	{
		size_type const old_size = this->size();
		this->resize(old_size + size);
		return new(static_cast<unsigned char*>(this->get()) + static_cast<difference_type>(old_size)) T;
	}

	reference operator[](size_type const i)
	{
		return *(this->begin() + static_cast<difference_type>(i));
	}

	const_reference operator[](size_type const i) const
	{
		return *(this->begin() + static_cast<difference_type>(i));
	}

	// These arguments are in BYTES and not elements, so users might get confused
	void reserve_bytes(size_type c)
	{
		if (c > this->c)
		{
			size_type const min_c = this->c + this->c / 2;
			if (c < min_c)
			{
				c = min_c;
			}

			using namespace std;
			void* new_p = realloc(this->p, this->n);	// shrink first, to avoid copying memory beyond the block
			if (new_p)
			{
				this->p = new_p;
				this->c = this->n;
			}
			new_p = realloc(this->p, c);
			if (new_p)
			{
				this->p = new_p;
				this->c = c;
			}
		}
	}

private:
	void resize(size_type const n)
	{
		if (n <= this->c)
		{
			// No destructors to call here...
			this->n = n;
		}
		else
		{
			size_type c = this->c + this->c / 2;
			if (c < n)
			{
				c = n;
			}

			using namespace std;
			this->p = c ? realloc(this->p, c) : nullptr;
			this->n = n;
		}
	}
};

#endif // UFFS_UTIL_BUFFER_HPP

