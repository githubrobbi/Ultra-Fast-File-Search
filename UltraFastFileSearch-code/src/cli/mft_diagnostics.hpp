// ============================================================================
// MFT Diagnostic Tools - Forward Declarations
// ============================================================================
// These functions are defined in UltraFastFileSearch.cpp and provide
// diagnostic and benchmarking capabilities for NTFS MFT operations.
// 
// TODO: Extract implementations to mft_diagnostics.cpp once dependencies
//       on the monolith are resolved.
// ============================================================================

#pragma once

#ifndef UFFS_MFT_DIAGNOSTICS_HPP
#define UFFS_MFT_DIAGNOSTICS_HPP

#include <iostream>

namespace uffs {

/**
 * @brief Dump raw MFT to file in UFFS-MFT format
 * 
 * @param drive_letter The drive letter (e.g., 'C')
 * @param output_path Path to output file (or "console" for stdout)
 * @param OS Output stream for status messages
 * @return 0 on success, error code on failure
 */
int dump_raw_mft(char drive_letter, const char* output_path, std::ostream& OS);

/**
 * @brief Dump MFT extents as JSON for diagnostic purposes
 * 
 * @param drive_letter The drive letter (e.g., 'C')
 * @param output_path Path to output file (or "console" for stdout)
 * @param verify_extents Whether to verify extent data
 * @param OS Output stream for status messages
 * @return 0 on success, error code on failure
 */
int dump_mft_extents(char drive_letter, const char* output_path, bool verify_extents, std::ostream& OS);

/**
 * @brief Benchmark raw MFT reading speed (read-only, no output file)
 * 
 * @param drive_letter The drive letter (e.g., 'C')
 * @param OS Output stream for status messages
 * @return 0 on success, error code on failure
 */
int benchmark_mft_read(char drive_letter, std::ostream& OS);

/**
 * @brief Benchmark full index building using the real UFFS async pipeline
 * 
 * @param drive_letter The drive letter (e.g., 'C')
 * @param OS Output stream for status messages
 * @return 0 on success, error code on failure
 */
int benchmark_index_build(char drive_letter, std::ostream& OS);

} // namespace uffs

// Expose at global scope for backward compatibility
using uffs::dump_raw_mft;
using uffs::dump_mft_extents;
using uffs::benchmark_mft_read;
using uffs::benchmark_index_build;

#endif // UFFS_MFT_DIAGNOSTICS_HPP

