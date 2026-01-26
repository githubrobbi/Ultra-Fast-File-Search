// ============================================================================
// CLI Main Entry Point - Compilation Unit
// ============================================================================
// This file provides the compilation context for cli_main.hpp
// It includes all necessary dependencies before including the implementation.
// ============================================================================

#include "../../stdafx.h"

// ============================================================================
// Standard Library Headers
// ============================================================================
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <ctime>
#include <cstdio>

// ============================================================================
// Boost Headers
// ============================================================================
#include <boost/algorithm/string.hpp>

// ============================================================================
// Project Headers - Utilities (relative paths from src/cli/)
// ============================================================================
#include "../util/core_types.hpp"
#include "../util/allocators.hpp"
#include "../util/error_utils.hpp"
#include "../util/string_utils.hpp"
#include "../util/time_utils.hpp"
#include "../util/volume_utils.hpp"
#include "../util/utf_convert.hpp"
#include "../util/nformat_ext.hpp"
#include "../util/devnull_check.hpp"
#include "../util/intrusive_ptr.hpp"
#include "../util/buffer.hpp"
#include "../util/containers.hpp"

// ============================================================================
// Project Headers - Core Components (relative paths from src/cli/)
// ============================================================================
#include "../../path.hpp"
#include "../../CommandLineParser.hpp"
#include "../index/ntfs_index.hpp"
#include "../io/io_completion_port.hpp"
#include "../search/match_operation.hpp"
#include "mft_diagnostics.hpp"

// ============================================================================
// CLI Main Implementation
// ============================================================================
// The implementation is in cli_main.hpp for now.
// This will be refactored to inline the code here in a future cleanup.
// ============================================================================

#include "cli_main.hpp"

