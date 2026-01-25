// ============================================================================
// String Utilities
// ============================================================================
// Extracted from UltraFastFileSearch.cpp for proper modular architecture
// ============================================================================

#pragma once

#include <string>

// ============================================================================
// Drive Enumeration
// ============================================================================

/// Returns a string containing all logical drive letters on the system
std::string drivenames(void);

// ============================================================================
// String Manipulation
// ============================================================================

/// Replaces all occurrences of 'from' with 'to' in the given string (in-place)
void replaceAll(std::string& str, const std::string& from, const std::string& to);

/// Removes all null characters from a string
std::string removeSpaces(std::string str);

