// ============================================================================
// x64 Launcher - Extract and run 64-bit version from 32-bit process
// ============================================================================
// Extracted from UltraFastFileSearch.cpp for proper modular architecture
// ============================================================================

#pragma once

#ifndef UFFS_X64_LAUNCHER_HPP
#define UFFS_X64_LAUNCHER_HPP

#include <Windows.h>
#include <tchar.h>
#include <string>
#include <utility>
#include <fstream>
#include <io.h>

// Forward declarations - these are defined in the monolith
class RefCountedCString;
class string_matcher;

namespace uffs {

/**
 * @brief Get the application GUID used for temp file naming
 * @return The application GUID string
 * 
 * Note: This function uses RefCountedCString which is defined in the monolith.
 * The implementation remains in the monolith for now.
 */
// RefCountedCString get_app_guid();  // Defined in monolith

/**
 * @brief Extract and run the 64-bit version if running under WOW64
 * @param hInstance The application instance handle
 * @param argc Argument count
 * @param argv Argument vector
 * @return Pair of (exit code, module path). Exit code is -1 if not launched.
 * 
 * This function:
 * 1. Checks if running under WOW64 (32-bit on 64-bit Windows)
 * 2. Extracts the embedded AMD64 binary from resources
 * 3. Launches it and waits for completion
 * 4. Returns the exit code of the 64-bit process
 * 
 * Note: This function depends on:
 * - Wow64::is_wow64() from wow64.hpp
 * - string_matcher for pattern matching
 * - get_app_guid() for temp file naming
 * - Various path utilities from path.hpp
 * 
 * The implementation remains in the monolith due to these dependencies.
 */
// std::pair<int, std::tstring> extract_and_run_if_needed(HINSTANCE hInstance, int argc, TCHAR* const argv[]);

} // namespace uffs

#endif // UFFS_X64_LAUNCHER_HPP

