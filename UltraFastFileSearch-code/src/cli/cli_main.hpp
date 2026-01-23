#pragma once

// ============================================================================
// CLI Entry Point Documentation
// ============================================================================
// This header documents the CLI entry point for UltraFastFileSearch.
// The actual implementation remains in UltraFastFileSearch.cpp for now.
//
// Location in UltraFastFileSearch.cpp: Lines 12919-14071 (~1152 lines)
// ============================================================================

// ============================================================================
// OVERVIEW
// ============================================================================
// The CLI entry point (main()) provides command-line access to UFFS functionality.
// It handles argument parsing, drive selection, search pattern processing,
// and output formatting.
//
// Entry Point:
//   int main(int argc, char* argv[])
//
// Output:
//   - Console output (stdout) for search results
//   - File output when --output is specified
//   - Error messages to stderr
//
// ============================================================================
// DEPENDENCIES
// ============================================================================
// The CLI main function depends on:
//
// 1. CommandLineParser (CommandLineParser.hpp)
//    - CLI11-based argument parsing
//    - Options structure with all command-line flags
//
// 2. NtfsIndex class (lines 3587-5565)
//    - Core indexing functionality
//    - File/directory enumeration
//
// 3. IoCompletionPort class (lines 6740-7050)
//    - Async I/O for MFT reading
//
// 4. MatchOperation class (lines 5448-5565)
//    - Pattern matching (regex and wildcard)
//
// 5. Helper functions:
//    - drivenames() - Get available drive letters
//    - replaceAll() - String replacement utility
//    - removeSpaces() - String cleanup utility
//    - GetLastErrorAsString() - Windows error formatting
//    - dump_raw_mft() - MFT dump functionality
//    - dump_mft_extents() - MFT extent diagnostics
//    - benchmark_mft_read() - MFT read benchmarking
//    - benchmark_index_build() - Index build benchmarking
//
// ============================================================================
// COMMAND-LINE OPTIONS
// ============================================================================
// See CommandLineParser.hpp for full option definitions.
//
// Key options:
//   <search-pattern>     Search pattern (supports regex with > prefix)
//   --drives=<list>      Comma-separated drive letters (default: all)
//   --extensions=<list>  Filter by file extensions
//   --output=<file>      Output file (default: console)
//   --columns=<list>     Output columns to include
//   --separator=<char>   Column separator (default: tab)
//   --header=<bool>      Include header row
//   --quotes=<char>      Quote character for values
//   --benchmark-index=<drive>  Run index build benchmark
//   --benchmark-mft=<drive>    Run MFT read benchmark
//   --dump-mft=<drive>         Dump raw MFT data
//   --dump-extents=<drive>     Dump MFT extent information
//
// ============================================================================
// MAIN FUNCTION STRUCTURE
// ============================================================================
// 1. Argument Parsing (lines 12919-12976)
//    - Initialize CommandLineParser
//    - Parse arguments and handle help/version
//    - Map options to local variables
//
// 2. Special Commands (lines 12983-13021)
//    - Handle --dump-mft
//    - Handle --dump-extents
//    - Handle --benchmark-mft
//    - Handle --benchmark-index
//
// 3. Output Setup (lines 13023-13054)
//    - Open output file if specified
//    - Handle console vs file output
//
// 4. Drive Processing (lines 13058-13145)
//    - Parse drive letters from search pattern
//    - Validate drive letters
//    - Build drive list for indexing
//
// 5. Extension Processing (lines 13147-13254)
//    - Parse file extensions
//    - Build regex pattern for extension filtering
//
// 6. Search Execution (lines 13326-14051)
//    - Initialize IoCompletionPort
//    - Build NtfsIndex for each drive
//    - Execute pattern matching
//    - Format and output results
//
// 7. Error Handling (lines 14054-14071)
//    - Catch std::invalid_argument
//    - Catch CStructured_Exception
//
// ============================================================================
// FUTURE EXTRACTION PLAN
// ============================================================================
// To fully extract the CLI entry point:
//
// 1. Create cli_main.cpp with the main() function
// 2. Move helper functions to appropriate modules:
//    - String utilities to src/util/string_utils.hpp
//    - Drive utilities to src/core/volume.hpp
// 3. Update includes to use extracted headers
// 4. Update project file to compile cli_main.cpp
// 5. Verify build and test functionality
//
// Estimated effort: 4-6 hours
// Risk: Medium (many dependencies on global state)
// ============================================================================

namespace uffs {
namespace cli {

// Forward declaration for future extraction
// int run_cli(int argc, char* argv[]);

} // namespace cli
} // namespace uffs

