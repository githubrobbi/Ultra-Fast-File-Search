#pragma once

#ifndef SHITEMID_HPP
#define SHITEMID_HPP

class CShellItemIDList
{
public:
	LPITEMIDLIST m_pidl;

	CShellItemIDList(LPITEMIDLIST pidl = NULL) : m_pidl(pidl)
	{ }

	~CShellItemIDList()
	{
		::CoTaskMemFree(m_pidl);
	}

	void Attach(LPITEMIDLIST pidl)
	{
		::CoTaskMemFree(m_pidl);
		m_pidl = pidl;
	}

	LPITEMIDLIST Detach()
	{
		LPITEMIDLIST pidl = m_pidl;
		m_pidl = NULL;
		return pidl;
	}

	bool IsNull() const
	{
		return (m_pidl == NULL);
	}

	CShellItemIDList& operator =(LPITEMIDLIST pidl)
	{
		Attach(pidl);
		return *this;
	}

	LPITEMIDLIST* operator &()
	{
		return &m_pidl;
	}

	operator LPITEMIDLIST()
	{
		return m_pidl;
	}

	operator LPCTSTR()
	{
		return (LPCTSTR)m_pidl;
	}

	operator LPTSTR()
	{
		return (LPTSTR)m_pidl;
	}

	void CreateEmpty(UINT cbSize)
	{
		::CoTaskMemFree(m_pidl);
		m_pidl = (LPITEMIDLIST)::CoTaskMemAlloc(cbSize);
		ATLASSERT(m_pidl != NULL);
		if(m_pidl != NULL)
			memset(m_pidl, 0, cbSize);
	}
};

#endif
