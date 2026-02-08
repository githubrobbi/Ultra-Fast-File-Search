/**
 * @file x64_launcher.hpp
 * @brief x64 binary extraction and launch utilities for WoW64 environments
 *
 * @details
 * This file provides utilities for automatically launching the 64-bit version
 * of the application when running as a 32-bit process on a 64-bit Windows
 * system (WoW64).
 *
 * ## Why This Exists
 *
 * The application ships as a 32-bit executable with an embedded 64-bit binary.
 * When run on a 64-bit system, the 32-bit process extracts and launches the
 * 64-bit version for better performance (larger address space, more registers).
 *
 * ## Extraction Flow
 *
 * ```
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    x64 Launch Decision                          │
 * ├─────────────────────────────────────────────────────────────────┤
 * │                                                                 │
 * │  ┌──────────────────┐                                          │
 * │  │ Is WoW64?        │──No──> Continue as x86                   │
 * │  └────────┬─────────┘                                          │
 * │           │ Yes                                                 │
 * │           v                                                     │
 * │  ┌──────────────────┐                                          │
 * │  │ Is debugger      │──Yes──> Continue as x86 (for debugging)  │
 * │  │ attached?        │                                          │
 * │  └────────┬─────────┘                                          │
 * │           │ No                                                  │
 * │           v                                                     │
 * │  ┌──────────────────┐                                          │
 * │  │ Is exe name      │──Yes──> Continue as x86 (explicit x86)   │
 * │  │ *_x86.exe?       │                                          │
 * │  └────────┬─────────┘                                          │
 * │           │ No                                                  │
 * │           v                                                     │
 * │  ┌──────────────────┐                                          │
 * │  │ Extract AMD64    │                                          │
 * │  │ resource to temp │                                          │
 * │  └────────┬─────────┘                                          │
 * │           │                                                     │
 * │           v                                                     │
 * │  ┌──────────────────┐                                          │
 * │  │ Launch x64       │                                          │
 * │  │ process & wait   │                                          │
 * │  └────────┬─────────┘                                          │
 * │           │                                                     │
 * │           v                                                     │
 * │  ┌──────────────────┐                                          │
 * │  │ Return x64       │                                          │
 * │  │ exit code        │                                          │
 * │  └──────────────────┘                                          │
 * └─────────────────────────────────────────────────────────────────┘
 * ```
 *
 * ## Extraction Locations
 *
 * The x64 binary is extracted to one of:
 * 1. An Alternate Data Stream (ADS) on the original exe (preferred)
 * 2. A temp file in %TEMP% (fallback)
 *
 * ## Thread Safety
 *
 * - extract_and_run_if_needed() should only be called once at startup
 * - It is NOT thread-safe (modifies global process state)
 *
 * ## Usage Example
 *
 * ```cpp
 * int _tmain(int argc, TCHAR* argv[])
 * {
 *     auto [exit_code, module_path] = extract_and_run_if_needed(
 *         GetModuleHandle(nullptr), argc, argv);
 *
 *     if (exit_code >= 0) {
 *         // x64 process ran and exited
 *         return exit_code;
 *     }
 *
 *     // Continue running as x86 (or native x64)
 *     return run_application(argc, argv);
 * }
 * ```
 *
 * @see wow64.hpp - WoW64 detection utilities
 */

#pragma once

#ifndef UFFS_X64_LAUNCHER_HPP
#define UFFS_X64_LAUNCHER_HPP

#include <fstream>
#include <string>
#include <utility>

#include <Windows.h>
#include <tchar.h>

#include "path.hpp"
#include "wow64.hpp"
#include "search/string_matcher.hpp"

namespace uffs {

/**
 * @brief Gets the application GUID for unique temp file naming
 * @return A unique GUID string for this application
 */
inline std::tstring get_app_guid()
{
	return std::tstring(_T("{40D41A33-D1FF-4759-9551-0A7201E9F829}"));
}

/**
 * @brief Extract and run x64 binary if running under WoW64
 *
 * @param hInstance Application instance handle
 * @param argc Argument count from main()
 * @param argv Argument vector from main()
 * @return pair<exit_code, module_path>
 *         - exit_code: -1 if not launched (continue in x86), otherwise child exit code
 *         - module_path: path to current executable
 *
 * @details
 * On a 32-bit process running under WoW64, this function:
 * 1. Extracts the embedded AMD64 binary from resources
 * 2. Writes it to a temp file or ADS
 * 3. Launches it with the same command line
 * 4. Waits for it to complete
 * 5. Returns its exit code
 *
 * The function returns -1 if:
 * - Not running under WoW64 (already native)
 * - Debugger is attached (for debugging the x86 version)
 * - Executable name contains "_x86" or similar (explicit x86 mode)
 * - Resource extraction fails
 *
 * @note The temp file is automatically deleted when the function returns.
 */
inline std::pair<int, std::tstring> extract_and_run_if_needed(
	HINSTANCE hInstance, int argc, TCHAR* const argv[])
{
	int result = -1;
	std::tstring module_path;
	if (argc > 0)
	{
		module_path = argv[0];
	}
	else
	{
		module_path.resize(USHRT_MAX, _T('\0'));
		module_path.resize(GetModuleFileName(hInstance, &module_path[0],
			static_cast<unsigned int>(module_path.size())));
	}

	if (Wow64::is_wow64() && !string_matcher(
		string_matcher::pattern_regex,
		string_matcher::pattern_option_case_insensitive,
		_T("^.*(?:(?:\\.|_)(?:x86|I386|IA32)|32)(?:\\.[^:/\\\\\\.]+)(?::.*)?$"))
		.is_match(module_path.data(), module_path.size()))
	{
		if (!IsDebuggerPresent())
		{
			HRSRC hRsrs = nullptr;
			WORD langs[] = {
				GetUserDefaultUILanguage(),
				MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				MAKELANGID(LANG_NEUTRAL, SUBLANG_SYS_DEFAULT),
				MAKELANGID(LANG_INVARIANT, SUBLANG_NEUTRAL)
			};

			for (size_t ilang = 0; ilang < sizeof(langs) / sizeof(*langs) && !hRsrs; ++ilang)
			{
				hRsrs = FindResourceEx(nullptr, _T("BINARY"), _T("AMD64"), langs[ilang]);
			}

			HGLOBAL hResource = LoadResource(nullptr, hRsrs);
			LPVOID pBinary = hResource ? LockResource(hResource) : nullptr;
			if (pBinary)
			{
				std::tstring tempDir(32 * 1024, _T('\0'));
				tempDir.resize(GetTempPath(static_cast<DWORD>(tempDir.size()), &tempDir[0]));
				if (!tempDir.empty())
				{
					std::tstring fileName;
					struct Deleter
					{
						std::tstring file;
						~Deleter()
						{
							if (!this->file.empty())
							{
								_tunlink(this->file.c_str());
							}
						}
					} deleter;

					bool success = false;
					for (int pass = 0; !success && pass < 2; ++pass)
					{
						if (pass)
						{
							std::tstring const module_file_name(
								basename(module_path.begin(), module_path.end()),
								module_path.end());
							fileName.assign(module_file_name.begin(),
								fileext(module_file_name.begin(), module_file_name.end()));
							fileName.insert(fileName.begin(), tempDir.begin(), tempDir.end());
							TCHAR tempbuf[10];
							fileName.append(_itot(64, tempbuf, 10));
							fileName.append(_T("_"));
							fileName.append(get_app_guid());
							fileName.append(
								fileext(module_file_name.begin(), module_file_name.end()),
								module_file_name.end());
						}
						else
						{
							fileName = module_path;
							fileName.append(_T(":"));
							TCHAR tempbuf[10];
							fileName.append(_itot(64, tempbuf, 10));
							fileName.append(_T("_"));
							fileName.append(get_app_guid());
						}

						std::filebuf file;
						std::ios_base::openmode const openmode =
							std::ios_base::out | std::ios_base::trunc | std::ios_base::binary;
#if defined(_CPPLIB_VER)
						success = !!file.open(fileName.c_str(), openmode);
#else
						std::string fileNameChars;
						std::copy(fileName.begin(), fileName.end(),
							std::inserter(fileNameChars, fileNameChars.end()));
						success = !!file.open(fileNameChars.c_str(), openmode);
#endif
						if (success)
						{
							deleter.file = fileName;
							file.sputn(static_cast<char const*>(pBinary),
								static_cast<std::streamsize>(SizeofResource(nullptr, hRsrs)));
							file.close();
						}
					}

					if (success)
					{
						STARTUPINFO si = { sizeof(si) };
						GetStartupInfo(&si);
						PROCESS_INFORMATION pi;
						HANDLE hJob = CreateJobObject(nullptr, nullptr);
						JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobLimits = {
							{ { 0 }, { 0 },
							  JOB_OBJECT_LIMIT_BREAKAWAY_OK | JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE }
						};

						if (hJob != nullptr &&
							SetInformationJobObject(hJob, JobObjectExtendedLimitInformation,
								&jobLimits, sizeof(jobLimits)) &&
							AssignProcessToJobObject(hJob, GetCurrentProcess()))
						{
							if (CreateProcess(fileName.c_str(), GetCommandLine(),
								nullptr, nullptr, FALSE,
								CREATE_PRESERVE_CODE_AUTHZ_LEVEL | CREATE_SUSPENDED |
								CREATE_UNICODE_ENVIRONMENT,
								nullptr, nullptr, &si, &pi))
							{
								jobLimits.BasicLimitInformation.LimitFlags |=
									JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK;
								SetInformationJobObject(hJob, JobObjectExtendedLimitInformation,
									&jobLimits, sizeof(jobLimits));
								if (ResumeThread(pi.hThread) != -1)
								{
									WaitForSingleObject(pi.hProcess, INFINITE);
									DWORD exitCode = 0;
									GetExitCodeProcess(pi.hProcess, &exitCode);
									result = static_cast<int>(exitCode);
								}
								else
								{
									TerminateProcess(pi.hProcess, GetLastError());
								}
							}
						}
					}
				}
			}

			/*continue running in x86 mode... */
		}
	}

	return std::make_pair(result, module_path);
}

} // namespace uffs

#endif // UFFS_X64_LAUNCHER_HPP
