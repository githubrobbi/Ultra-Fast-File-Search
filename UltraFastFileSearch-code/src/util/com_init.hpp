/**
 * @file com_init.hpp
 * @brief RAII wrappers for COM and OLE initialization
 *
 * Provides exception-safe initialization/cleanup for Windows COM/OLE:
 * - CoInit: Wraps CoInitialize/CoUninitialize
 * - OleInit: Wraps OleInitialize/OleUninitialize
 *
 * @note Extracted from UltraFastFileSearch.cpp during Phase 7 refactoring
 */

#ifndef UFFS_UTIL_COM_INIT_HPP
#define UFFS_UTIL_COM_INIT_HPP

#include <objbase.h>  // CoInitialize, CoUninitialize
#include <ole2.h>     // OleInitialize, OleUninitialize

/**
 * @brief RAII wrapper for COM initialization
 *
 * Automatically calls CoInitialize on construction and CoUninitialize
 * on destruction (if initialization succeeded with S_OK).
 */
class CoInit
{
	CoInit(CoInit const&);
	CoInit& operator=(CoInit const&);

public:
	HRESULT hr;

	CoInit(bool initialize = true) :
		hr(initialize ? CoInitialize(nullptr) : S_FALSE) {}

	~CoInit()
	{
		if (this->hr == S_OK)
		{
			CoUninitialize();
		}
	}
};

/**
 * @brief RAII wrapper for OLE initialization
 *
 * Automatically calls OleInitialize on construction and OleUninitialize
 * on destruction (if initialization succeeded with S_OK).
 *
 * OLE initialization includes COM initialization, plus additional
 * clipboard, drag-and-drop, and in-place activation support.
 */
class OleInit
{
	OleInit(OleInit const&);
	OleInit& operator=(OleInit const&);

public:
	HRESULT hr;

	OleInit(bool initialize = true) : hr(initialize ? OleInitialize(nullptr) : S_FALSE) {}

	~OleInit()
	{
		if (this->hr == S_OK)
		{
			OleUninitialize();
		}
	}
};

#endif // UFFS_UTIL_COM_INIT_HPP

