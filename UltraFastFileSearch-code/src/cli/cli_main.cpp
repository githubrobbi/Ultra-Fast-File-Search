// ============================================================================
// CLI Main Entry Point - Compilation Unit
// ============================================================================
// This file provides the compilation context for cli_main.hpp
// It includes all necessary dependencies before including the implementation.
// ============================================================================

#include "stdafx.h"

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
// Project Headers - Utilities
// ============================================================================
#include "src/util/core_types.hpp"
#include "src/util/allocators.hpp"
#include "src/util/error_utils.hpp"
#include "src/util/string_utils.hpp"
#include "src/util/time_utils.hpp"
#include "src/util/volume_utils.hpp"
#include "src/util/utf_convert.hpp"
#include "src/util/nformat_ext.hpp"
#include "src/util/devnull_check.hpp"
#include "src/util/intrusive_ptr.hpp"
#include "src/util/buffer.hpp"
#include "src/util/containers.hpp"

// ============================================================================
// Project Headers - Core Components
// ============================================================================
#include "path.hpp"
#include "CommandLineParser.hpp"
#include "src/index/ntfs_index.hpp"
#include "src/io/io_completion_port.hpp"
#include "src/search/match_operation.hpp"
#include "src/cli/mft_diagnostics.hpp"

// ============================================================================
// CLI Main Implementation
// ============================================================================
// The implementation is in cli_main.hpp for now.
// This will be refactored to inline the code here in a future cleanup.
// ============================================================================

#include "src/cli/cli_main.hpp"

