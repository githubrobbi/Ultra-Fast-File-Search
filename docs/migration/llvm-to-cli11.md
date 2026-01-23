# LLVM CommandLine to CLI11 Migration Guide

## Overview

This document provides a step-by-step guide to replace the LLVM CommandLine library with CLI11 in Ultra Fast File Search (UFFS). The goal is **100% CLI compatibility** - users should not notice any difference in command-line behavior.

**Estimated Effort:** 1-2 days  
**Difficulty:** Junior-Intermediate  
**Risk:** Low (CLI11 is well-tested, header-only)

---

## Why Migrate?

| Aspect | LLVM CommandLine | CLI11 |
|--------|------------------|-------|
| Size | ~50MB headers + library | ~50KB single header |
| Build time | Slow | Fast |
| Dependencies | Complex LLVM ecosystem | None (header-only) |
| Maintenance | Tied to LLVM releases | Independent, active |
| C++ Standard | C++14 | C++11/14/17/20 |

---

## Current CLI Behavior (MUST PRESERVE)

### Command Examples That Must Work Identically

```bash
# Basic search
uffs.com c:/pro*
uffs.com /pro*.txt
uffs.com /pro*.txt --drives=c,d,m

# With extensions
uffs.com /pro** --ext=jpg,mp4,documents

# REGEX search (starts with >)
uffs.com ">C:\\TemP.*\.txt"
uffs.com ">.*\\DatA.*\d{3}-\d{2}-\d{4}.*" --drives=d,c

# Output options
uffs.com d:/data** --ext=pictures --out=console
uffs.com * --ext=movies --out=bigfile.csv
uffs.com * --out=bigfile.csv --header=true --sep=; --columns=path,size,created

# Help commands
uffs.com --help
uffs.com --help-list
uffs.com --help-hidden
uffs.com --version
uffs.com -h
```

---

## Milestone 1: Setup CLI11 (30 minutes)

### Task 1.1: Download CLI11

CLI11 is header-only. Download from: https://github.com/CLIUtils/CLI11

```bash
# Option A: Single header (recommended)
curl -o UltraFastFileSearch-code/CLI11.hpp \
  https://github.com/CLIUtils/CLI11/releases/download/v2.4.2/CLI11.hpp

# Option B: Git submodule
git submodule add https://github.com/CLIUtils/CLI11.git external/CLI11
```

### Task 1.2: Add to Project

In `UltraFastFileSearch.vcxproj`, add the header to the project (no library linking needed).

### Task 1.3: Create Test Branch

```bash
git checkout -b feature/cli11-migration
```

---

## Milestone 2: Create CommandLineParser Class (2 hours)

### Task 2.1: Create New Header File

Create `UltraFastFileSearch-code/CommandLineParser.hpp`:

```cpp
#pragma once
#include "CLI11.hpp"
#include <string>
#include <vector>
#include <set>
#include <cstdint>

// Forward declaration for version printer
void PrintVersion();

// Column flags enum (matches existing File_Attributes)
enum ColumnFlags : uint32_t {
    COL_NONE        = 0,
    COL_ALL         = 1 << 0,
    COL_PATH        = 1 << 1,
    COL_NAME        = 1 << 2,
    COL_PATHONLY    = 1 << 3,
    COL_TYPE        = 1 << 4,
    COL_SIZE        = 1 << 5,
    COL_SIZEONDISK  = 1 << 6,
    COL_CREATED     = 1 << 7,
    COL_WRITTEN     = 1 << 8,
    COL_ACCESSED    = 1 << 9,
    COL_DECENDENTS  = 1 << 10,
    COL_R           = 1 << 11,
    COL_A           = 1 << 12,
    COL_S           = 1 << 13,
    COL_H           = 1 << 14,
    COL_O           = 1 << 15,
    COL_NOTCONTENT  = 1 << 16,
    COL_NOSCRUB     = 1 << 17,
    COL_INTEGRITY   = 1 << 18,
    COL_PINNED      = 1 << 19,
    COL_UNPINNED    = 1 << 20,
    COL_DIRECTORY   = 1 << 21,
    COL_COMPRESSED  = 1 << 22,
    COL_ENCRYPTED   = 1 << 23,
    COL_SPARSE      = 1 << 24,
    COL_REPARSE     = 1 << 25,
    COL_ATTRVALUE   = 1 << 26
};

struct CommandLineOptions {
    // Search options
    std::string searchPath;
    std::vector<std::string> drives;
    
    // Filter options
    std::vector<std::string> extensions;
    bool caseSensitive = false;
    bool bypassUAC = false;
    
    // Output options
    std::string outputFilename = "console";
    bool includeHeader = true;
    std::string quotes = "\"";
    std::string separator = ",";
    std::string positiveMarker = "1";
    std::string negativeMarker = "0";
    uint32_t columnFlags = COL_PATH | COL_NAME;
    
    // Diagnostic options
    std::string dumpMftDrive;
    std::string dumpMftOutput = "mft_dump.raw";
    std::string dumpExtentsDrive;
    std::string dumpExtentsOutput;
    bool verifyExtents = false;
    std::string benchmarkMftDrive;
    std::string benchmarkIndexDrive;
    
    // Metadata
    bool helpRequested = false;
    int parseResult = 0;
};

class CommandLineParser {
public:
    CommandLineParser(const std::string& diskDrives);
    int parse(int argc, const char* const* argv);
    const CommandLineOptions& options() const { return opts_; }
    
private:
    CLI::App app_;
    CommandLineOptions opts_;
    std::set<std::string> columnSet_;  // For parsing --columns
    
    void setupOptions(const std::string& diskDrives);
    uint32_t parseColumns(const std::set<std::string>& cols);
};
```

---

## Milestone 3: Implement Option Parsing (3 hours)

### Task 3.1: Implement CommandLineParser.cpp

Create `UltraFastFileSearch-code/CommandLineParser.cpp`:

```cpp
#include "CommandLineParser.hpp"
#include <algorithm>
#include <map>

// Column name to flag mapping
static const std::map<std::string, uint32_t> COLUMN_MAP = {
    {"all", COL_ALL}, {"path", COL_PATH}, {"name", COL_NAME},
    {"pathonly", COL_PATHONLY}, {"type", COL_TYPE}, {"size", COL_SIZE},
    {"sizeondisk", COL_SIZEONDISK}, {"created", COL_CREATED},
    {"written", COL_WRITTEN}, {"accessed", COL_ACCESSED},
    {"decendents", COL_DECENDENTS}, {"r", COL_R}, {"a", COL_A},
    {"s", COL_S}, {"h", COL_H}, {"o", COL_O},
    {"notcontent", COL_NOTCONTENT}, {"noscrub", COL_NOSCRUB},
    {"integrity", COL_INTEGRITY}, {"pinned", COL_PINNED},
    {"unpinned", COL_UNPINNED}, {"directory", COL_DIRECTORY},
    {"compressed", COL_COMPRESSED}, {"encrypted", COL_ENCRYPTED},
    {"sparse", COL_SPARSE}, {"reparse", COL_REPARSE},
    {"attributevalue", COL_ATTRVALUE}
};

CommandLineParser::CommandLineParser(const std::string& diskDrives)
    : app_("Ultra Fast File Search") {
    setupOptions(diskDrives);
}

void CommandLineParser::setupOptions(const std::string& diskDrives) {
    // Program description (matches LLVM overview)
    app_.description(
        "\n\t\tLocate files and folders by name instantly.\n\n"
        "\t\tUltra Fast File Search is a very fast file search utility\n"
        "\t\tthat can find files on your hard drive almost instantly.\n"
        "\t\tThe entire file system can be quickly sorted by name, size\n"
        "\t\tor date. Ultra Fast File Search supports all types of hard\n"
        "\t\tdrives, hard drive folders and network shares\n\n"
    );

    // Custom version flag with callback
    app_.set_version_flag("--version,-v", "", "Display version information");
    app_.get_version_flag()->callback([](){ PrintVersion(); });

    // -h as alias for --help (LLVM compatibility)
    app_.set_help_flag("--help,-h", "Display available options");

    // Add --help-list (hidden, for LLVM compatibility)
    app_.add_flag("--help-list", "Display list of available options")
        ->group("");  // Hidden

    // Add --help-hidden (hidden, for LLVM compatibility)
    app_.add_flag("--help-hidden", "Display all options including hidden")
        ->group("");

    //==========================================================================
    // SEARCH OPTIONS GROUP
    //==========================================================================

    // Positional: search path (e.g., "C:/pro*" or "/pro*.txt")
    app_.add_option("searchPath", opts_.searchPath,
        "  <<< Search path. E.g. 'C:/' or 'C:/Prog*' >>>")
        ->group("Search options");

    // --drives (comma-separated list)
    std::string drivesDesc = "Disk Drive(s) to search e.g. 'C:, D:' or any combination of ("
                            + diskDrives + ")\nDEFAULT: all disk drives";
    app_.add_option("--drives", opts_.drives, drivesDesc)
        ->delimiter(',')
        ->group("Search options");

    //==========================================================================
    // FILTER OPTIONS GROUP
    //==========================================================================

    // --ext (comma-separated extensions)
    app_.add_option("--ext", opts_.extensions,
        "File extensions e.g. '--ext=pdf' or '--ext=pdf,doc'")
        ->delimiter(',')
        ->group("Filter options");

    // --case (hidden - not fully implemented)
    app_.add_flag("--case", opts_.caseSensitive,
        "Switch CASE sensitivity ON or OFF\t\t\t\t\tDEFAULT: False")
        ->group("");  // Really hidden

    // --pass (hidden - bypass UAC)
    app_.add_flag("--pass", opts_.bypassUAC,
        "Bypass User Access Control (UAC)\t\t\tDEFAULT: False")
        ->group("");  // Really hidden

    //==========================================================================
    // OUTPUT OPTIONS GROUP
    //==========================================================================

    // --out
    app_.add_option("--out", opts_.outputFilename,
        "Specify output filename\tDEFAULT: console")
        ->group("Output options");

    // --header
    app_.add_option("--header", opts_.includeHeader,
        "Include column header\tDEFAULT: True")
        ->default_val(true)
        ->group("Output options");

    // --sep
    app_.add_option("--sep", opts_.separator,
        "Column separator\t\tDEFAULT: ,")
        ->default_val(",")
        ->group("Output options");

    // --quotes (hidden)
    app_.add_option("--quotes", opts_.quotes,
        "Char/String to enclose output values\tDEFAULT: \"")
        ->default_val("\"")
        ->group("");  // Hidden

    // --pos (hidden)
    app_.add_option("--pos", opts_.positiveMarker,
        "Marker for BOOLEAN attributes\t\tDEFAULT: 1")
        ->default_val("1")
        ->group("");  // Hidden

    // --neg (hidden)
    app_.add_option("--neg", opts_.negativeMarker,
        "Marker for BOOLEAN attributes\t\tDEFAULT: 0")
        ->default_val("0")
        ->group("");  // Hidden

    // --columns (comma-separated, custom parsing)
    app_.add_option("--columns", columnSet_,
        "OUTPUT Value-columns: e.g. '--columns=name,path,size,r,h,s'")
        ->delimiter(',')
        ->check(CLI::IsMember({
            "all", "path", "name", "pathonly", "type", "size", "sizeondisk",
            "created", "written", "accessed", "decendents", "r", "a", "s", "h", "o",
            "notcontent", "noscrub", "integrity", "pinned", "unpinned",
            "directory", "compressed", "encrypted", "sparse", "reparse", "attributevalue"
        }))
        ->group("Output options");

    //==========================================================================
    // DIAGNOSTIC OPTIONS GROUP
    //==========================================================================

    // --dump-mft
    app_.add_option("--dump-mft", opts_.dumpMftDrive,
        "Dump raw MFT to file in UFFS-MFT format. Usage: --dump-mft=<drive_letter>")
        ->group("Output options");

    // --dump-mft-out
    app_.add_option("--dump-mft-out", opts_.dumpMftOutput,
        "Output file path for raw MFT dump")
        ->default_val("mft_dump.raw")
        ->group("Output options");

    // --dump-extents
    app_.add_option("--dump-extents", opts_.dumpExtentsDrive,
        "Dump MFT extent map as JSON. Usage: --dump-extents=<drive_letter>")
        ->group("Output options");

    // --dump-extents-out
    app_.add_option("--dump-extents-out", opts_.dumpExtentsOutput,
        "Output file path for MFT extent JSON (default: stdout)")
        ->default_val("")
        ->group("Output options");

    // --verify
    app_.add_flag("--verify", opts_.verifyExtents,
        "Verify extent mapping by reading first record from each extent")
        ->group("Output options");

    // --benchmark-mft
    app_.add_option("--benchmark-mft", opts_.benchmarkMftDrive,
        "Benchmark MFT read speed (read-only). Usage: --benchmark-mft=<drive_letter>")
        ->group("Output options");

    // --benchmark-index
    app_.add_option("--benchmark-index", opts_.benchmarkIndexDrive,
        "Benchmark full index build. Usage: --benchmark-index=<drive_letter>")
        ->group("Output options");
}

int CommandLineParser::parse(int argc, const char* const* argv) {
    try {
        app_.parse(argc, argv);

        // Convert column set to flags
        if (!columnSet_.empty()) {
            opts_.columnFlags = parseColumns(columnSet_);
        }

        opts_.parseResult = 0;
        return 0;

    } catch (const CLI::ParseError& e) {
        opts_.parseResult = app_.exit(e);
        return opts_.parseResult;
    }
}

uint32_t CommandLineParser::parseColumns(const std::set<std::string>& cols) {
    uint32_t flags = 0;
    for (const auto& col : cols) {
        auto it = COLUMN_MAP.find(col);
        if (it != COLUMN_MAP.end()) {
            flags |= it->second;
        }
    }
    // If "all" is specified, set all flags
    if (flags & COL_ALL) {
        flags = 0xFFFFFFFF;
    }
    return flags;
}
```

---

## Milestone 4: Integration with Main Code (2 hours)

### Task 4.1: Modify UltraFastFileSearch.cpp

Replace the LLVM includes and option definitions with the new parser.

**Step 1: Remove LLVM include**

Find and remove:
```cpp
#include "llvm/Support/CommandLine.h"
```

**Step 2: Add new include**

Add at the top of the file:
```cpp
#include "CommandLineParser.hpp"
```

**Step 3: Replace option parsing in `wmain_internal`**

Find the section starting around line 12923 with:
```cpp
cl::OptionCategory SearchOptions("Search options");
```

Replace the entire LLVM option definition block (approximately lines 12923-13127) with:

```cpp
// Create parser with available disk drives
CommandLineParser parser(ws2s(diskdrives));
int parseResult = parser.parse(new_argc, new_argv);
if (parseResult != 0) {
    return parseResult;
}

const auto& opts = parser.options();

// Map options to existing variable names for minimal code changes
std::string searchPathCopy = opts.searchPath;
bool header = opts.includeHeader;
std::string OutputFilename = opts.outputFilename;
std::string quotes = opts.quotes;
std::string separator = opts.separator;
std::string positive = opts.positiveMarker;
std::string negative = opts.negativeMarker;
uint32_t output_columns_flags = opts.columnFlags;

// Handle diagnostic commands
if (!opts.dumpMftDrive.empty()) {
    char drive_letter = opts.dumpMftDrive[0];
    if (!isalpha(drive_letter)) {
        OS << "ERROR: Invalid drive letter: " << opts.dumpMftDrive << "\n";
        return ERROR_BAD_ARGUMENTS;
    }
    return dump_raw_mft(drive_letter, opts.dumpMftOutput.c_str(), OS);
}

if (!opts.dumpExtentsDrive.empty()) {
    char drive_letter = opts.dumpExtentsDrive[0];
    if (!isalpha(drive_letter)) {
        OS << "ERROR: Invalid drive letter: " << opts.dumpExtentsDrive << "\n";
        return ERROR_BAD_ARGUMENTS;
    }
    return dump_mft_extents(drive_letter, opts.dumpExtentsOutput.c_str(),
                            opts.verifyExtents, OS);
}

if (!opts.benchmarkMftDrive.empty()) {
    char drive_letter = opts.benchmarkMftDrive[0];
    if (!isalpha(drive_letter)) {
        OS << "ERROR: Invalid drive letter: " << opts.benchmarkMftDrive << "\n";
        return ERROR_BAD_ARGUMENTS;
    }
    return benchmark_mft_read(drive_letter, OS);
}

if (!opts.benchmarkIndexDrive.empty()) {
    char drive_letter = opts.benchmarkIndexDrive[0];
    if (!isalpha(drive_letter)) {
        OS << "ERROR: Invalid drive letter: " << opts.benchmarkIndexDrive << "\n";
        return ERROR_BAD_ARGUMENTS;
    }
    return benchmark_index_build(drive_letter, OS);
}
```

### Task 4.2: Update Column Flag Checks

The existing code uses `output_columns.isSet(flag)`. Replace with bitwise checks:

**Before (LLVM):**
```cpp
if (output_columns.isSet(File_Attributes::path)) { ... }
```

**After (CLI11):**
```cpp
if (output_columns_flags & COL_PATH) { ... }
```

### Task 4.3: Update Drive List Handling

**Before (LLVM):**
```cpp
if (drives.getNumOccurrences() > 0) {
    for (size_t i = 0; i < drives.size(); ++i) {
        // process drives[i]
    }
}
```

**After (CLI11):**
```cpp
if (!opts.drives.empty()) {
    for (const auto& drive : opts.drives) {
        // process drive
    }
}
```

### Task 4.4: Update Extension Handling

**Before (LLVM):**
```cpp
if (extentions.getNumOccurrences() > 0) {
    for (size_t i = 0; i < extentions.size(); ++i) {
        // process extentions[i]
    }
}
```

**After (CLI11):**
```cpp
if (!opts.extensions.empty()) {
    for (const auto& ext : opts.extensions) {
        // process ext
    }
}
```

---

## Milestone 5: Handle Special Cases (1 hour)

### Task 5.1: Console Output Detection

The existing code checks for console output:
```cpp
const bool console = (OutputFilename.compare("console") == 0 ||
                      OutputFilename.compare("con") == 0 ||
                      OutputFilename.compare("terminal") == 0 ||
                      OutputFilename.compare("term") == 0);
```

This remains unchanged - just use `opts.outputFilename` instead.

### Task 5.2: Special Separator Values

The manual mentions special separator substitutions. Ensure this logic is preserved:

```cpp
// In the code that processes separator
std::string sep = opts.separator;
if (sep == "TAB" || sep == "tab") sep = "\t";
else if (sep == "NEWLINE" || sep == "NEW LINE") sep = "\n";
else if (sep == "SPACE" || sep == "space") sep = " ";
else if (sep == "RETURN" || sep == "return") sep = "\r";
else if (sep == "DOUBLE" || sep == "double") sep = "\"";
else if (sep == "SINGLE" || sep == "single") sep = "'";
else if (sep == "NULL" || sep == "null") sep = "\0";
```

### Task 5.3: Default Output Filename

```cpp
// If no output specified, default to console
if (opts.outputFilename.empty()) {
    OutputFilename = "console";
}
// Handle shorthand "f" for file
else if (opts.outputFilename == "f") {
    OutputFilename = "uffs.csv";
}
```

---

## Milestone 6: Remove LLVM Dependencies (30 minutes)

### Task 6.1: Update vcxproj

In `UltraFastFileSearch.vcxproj`:

1. Remove LLVM include paths from `AdditionalIncludeDirectories`
2. Remove LLVM library paths from `AdditionalLibraryDirectories`
3. Remove LLVM libraries from `AdditionalDependencies`

### Task 6.2: Update Project.props

If LLVM paths are defined in `Project.props`, remove them.

### Task 6.3: Clean Up Includes

Remove any remaining LLVM includes:
```cpp
// Remove these
#include "llvm/Support/CommandLine.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Support/raw_ostream.h"  // if only used for CLI
```

---

## Milestone 7: Testing (2 hours)

### Task 7.1: Create Test Script

Create `test_cli_compatibility.bat`:

```batch
@echo off
setlocal enabledelayedexpansion

echo ============================================
echo CLI Compatibility Test Suite
echo ============================================

set UFFS=uffs.com
set PASS=0
set FAIL=0

:: Test 1: Help
echo Test 1: --help
%UFFS% --help >nul 2>&1
if %ERRORLEVEL% EQU 0 (echo   PASS & set /a PASS+=1) else (echo   FAIL & set /a FAIL+=1)

:: Test 2: Version
echo Test 2: --version
%UFFS% --version >nul 2>&1
if %ERRORLEVEL% EQU 0 (echo   PASS & set /a PASS+=1) else (echo   FAIL & set /a FAIL+=1)

:: Test 3: Short help
echo Test 3: -h
%UFFS% -h >nul 2>&1
if %ERRORLEVEL% EQU 0 (echo   PASS & set /a PASS+=1) else (echo   FAIL & set /a FAIL+=1)

:: Test 4: Basic search
echo Test 4: Basic search (C:/Windows*)
%UFFS% C:/Windows* --out=console >nul 2>&1
if %ERRORLEVEL% EQU 0 (echo   PASS & set /a PASS+=1) else (echo   FAIL & set /a FAIL+=1)

:: Test 5: Drives option
echo Test 5: --drives=C
%UFFS% /Windows* --drives=C --out=console >nul 2>&1
if %ERRORLEVEL% EQU 0 (echo   PASS & set /a PASS+=1) else (echo   FAIL & set /a FAIL+=1)

:: Test 6: Multiple drives
echo Test 6: --drives=C,D
%UFFS% /* --drives=C,D --out=console >nul 2>&1
if %ERRORLEVEL% EQU 0 (echo   PASS & set /a PASS+=1) else (echo   FAIL & set /a FAIL+=1)

:: Test 7: Extensions
echo Test 7: --ext=txt,pdf
%UFFS% C:/* --ext=txt,pdf --out=console >nul 2>&1
if %ERRORLEVEL% EQU 0 (echo   PASS & set /a PASS+=1) else (echo   FAIL & set /a FAIL+=1)

:: Test 8: Columns
echo Test 8: --columns=path,name,size
%UFFS% C:/Windows* --columns=path,name,size --out=console >nul 2>&1
if %ERRORLEVEL% EQU 0 (echo   PASS & set /a PASS+=1) else (echo   FAIL & set /a FAIL+=1)

:: Test 9: Header option
echo Test 9: --header=false
%UFFS% C:/Windows* --header=false --out=console >nul 2>&1
if %ERRORLEVEL% EQU 0 (echo   PASS & set /a PASS+=1) else (echo   FAIL & set /a FAIL+=1)

:: Test 10: Separator
echo Test 10: --sep=;
%UFFS% C:/Windows* --sep=; --out=console >nul 2>&1
if %ERRORLEVEL% EQU 0 (echo   PASS & set /a PASS+=1) else (echo   FAIL & set /a FAIL+=1)

:: Test 11: Output to file
echo Test 11: --out=test_output.csv
%UFFS% C:/Windows* --out=test_output.csv
if exist test_output.csv (echo   PASS & set /a PASS+=1 & del test_output.csv) else (echo   FAIL & set /a FAIL+=1)

:: Test 12: REGEX search
echo Test 12: REGEX search
%UFFS% ">C:\\Windows.*" --out=console >nul 2>&1
if %ERRORLEVEL% EQU 0 (echo   PASS & set /a PASS+=1) else (echo   FAIL & set /a FAIL+=1)

:: Test 13: All columns
echo Test 13: --columns=all
%UFFS% C:/Windows* --columns=all --out=console >nul 2>&1
if %ERRORLEVEL% EQU 0 (echo   PASS & set /a PASS+=1) else (echo   FAIL & set /a FAIL+=1)

echo ============================================
echo Results: %PASS% passed, %FAIL% failed
echo ============================================

if %FAIL% GTR 0 exit /b 1
exit /b 0
```

### Task 7.2: Run Tests

```bash
# Build the project
msbuild UltraFastFileSearch.sln /p:Configuration=Release

# Run test suite
test_cli_compatibility.bat
```

### Task 7.3: Manual Verification

Test each example from "The Manual.md":

| Command | Expected Result |
|---------|-----------------|
| `uffs c:/pro*` | Finds files starting with "pro" on C: |
| `uffs /pro*.txt` | Finds .txt files on ALL disks |
| `uffs /pro*.txt --drives=c,d,m` | Finds on specific drives |
| `uffs ">C:\\TemP.*\.txt"` | REGEX search works |
| `uffs * --out=bigfile.csv` | Creates output file |

---

## Milestone 8: Final Cleanup (30 minutes)

### Task 8.1: Remove LLVM Directory (Optional)

If LLVM is no longer needed anywhere:
```bash
# Remove from git (keep locally for now)
git rm -r --cached llvm/

# Or fully remove
rm -rf llvm/
```

### Task 8.2: Update Documentation

Update README.md to reflect:
- Removed LLVM dependency
- CLI11 is now used (header-only, no installation needed)
- Build is simpler/faster

### Task 8.3: Commit Changes

```bash
git add -A
git commit -m "Replace LLVM CommandLine with CLI11

- Removed ~50MB LLVM dependency
- Added CLI11 single-header library (~50KB)
- 100% CLI compatibility maintained
- Faster build times
- No external library linking required"
```

---

## CLI Feature Mapping Reference

| LLVM Feature | CLI11 Equivalent | Notes |
|--------------|------------------|-------|
| `cl::opt<bool>` | `app.add_flag()` | Direct replacement |
| `cl::opt<std::string>` | `app.add_option()` | Direct replacement |
| `cl::list<std::string>` | `app.add_option()->expected(-1)` | Use `->delimiter(',')` for comma-separated |
| `cl::bits<enum>` | `std::set<std::string>` + custom parsing | See `parseColumns()` |
| `cl::Positional` | First positional in `add_option()` | Just use option name without `--` |
| `cl::desc()` | Second parameter to add_option | Direct replacement |
| `cl::init()` | `->default_val()` | Direct replacement |
| `cl::cat()` | `->group()` | Direct replacement |
| `cl::Hidden` | `->group("")` | Empty group = hidden |
| `cl::ReallyHidden` | `->group("")` | Same as Hidden in CLI11 |
| `cl::CommaSeparated` | `->delimiter(',')` | Direct replacement |
| `cl::ZeroOrMore` | Default behavior | No action needed |
| `getNumOccurrences()` | `option->count()` or check if empty | Direct replacement |
| `getValue()` | Direct variable access | Simpler in CLI11 |
| `SetVersionPrinter` | `set_version_flag()` + callback | See example |
| `HideUnrelatedOptions` | Groups handle visibility | Automatic |
| `ParseCommandLineOptions` | `app.parse()` | Direct replacement |

---

## Troubleshooting

### Issue: "Unknown option" errors

**Cause:** CLI11 is stricter about option format.

**Solution:** Ensure options use `--name=value` or `--name value` format consistently.

### Issue: Comma-separated values not parsing

**Cause:** Missing `->delimiter(',')`.

**Solution:** Add delimiter to list options:
```cpp
app.add_option("--drives", opts.drives, "...")->delimiter(',');
```

### Issue: Help output looks different

**Cause:** CLI11 has different default formatting.

**Solution:** Customize formatter if exact match needed:
```cpp
auto fmt = std::make_shared<CLI::Formatter>();
fmt->column_width(40);
app.formatter(fmt);
```

### Issue: Boolean options not working

**Cause:** CLI11 uses `add_flag()` for booleans, not `add_option()`.

**Solution:**
```cpp
// Wrong
app.add_option("--header", opts.includeHeader, "...");

// Right for flags (no value needed)
app.add_flag("--verbose", opts.verbose, "...");

// Right for bool with value (--header=true/false)
app.add_option("--header", opts.includeHeader, "...")->default_val(true);
```

---

## Verification Checklist

Before merging, verify:

- [ ] `uffs --help` shows all options correctly
- [ ] `uffs --version` shows version info
- [ ] `uffs -h` works as alias for --help
- [ ] Positional search path works: `uffs C:/test*`
- [ ] `--drives=C,D` parses correctly
- [ ] `--ext=pdf,doc` parses correctly
- [ ] `--columns=path,name,size` parses correctly
- [ ] `--out=filename.csv` creates file
- [ ] `--out=console` outputs to console
- [ ] `--header=true/false` works
- [ ] `--sep=;` works
- [ ] REGEX search `">pattern"` works
- [ ] All diagnostic commands work (--dump-mft, --benchmark-mft, etc.)
- [ ] Build completes without LLVM libraries
- [ ] No LLVM includes remain in code

---

## Summary

| Milestone | Time | Description |
|-----------|------|-------------|
| 1 | 30 min | Setup CLI11 |
| 2 | 2 hours | Create CommandLineParser class |
| 3 | 3 hours | Implement option parsing |
| 4 | 2 hours | Integration with main code |
| 5 | 1 hour | Handle special cases |
| 6 | 30 min | Remove LLVM dependencies |
| 7 | 2 hours | Testing |
| 8 | 30 min | Final cleanup |
| **Total** | **~12 hours** | **1-2 working days** |

---

## Resources

- **CLI11 Documentation:** https://cliutils.github.io/CLI11/book/
- **CLI11 GitHub:** https://github.com/CLIUtils/CLI11
- **CLI11 Examples:** https://github.com/CLIUtils/CLI11/tree/main/examples
- **Single Header Download:** https://github.com/CLIUtils/CLI11/releases

