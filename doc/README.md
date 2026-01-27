# Ultra Fast File Search (UFFS)

**Lightning-fast Windows file search by reading the NTFS Master File Table directly.**

## What is it?

UFFS is a command-line and GUI tool that searches files on Windows NTFS drives **68% faster than Everything** and **4x faster than WizFile**. It achieves this by reading the MFT (Master File Table) directly instead of querying the OS for each file.

| Tool | 19 Million Files | Notes |
|------|------------------|-------|
| **UFFS** | **121 seconds** | All disks in parallel |
| Everything | 178 seconds | Requires background service |
| WizFile | 299 seconds | Single disk only |

## Quick Start

```powershell
# Search for all .txt files starting with "report"
uffs c:/report*.txt

# Search all drives for pictures
uffs * --ext=pictures

# Regex search (prefix with >)
uffs ">C:\\Users\\.*\.pdf"

# Output to CSV
uffs * --ext=documents --out=results.csv
```

**Note:** Requires Administrator privileges (UAC prompt) to read the MFT.

## Features

- **Glob patterns** - `*` matches characters, `**` matches path separators
- **Regex support** - Full regex with `">pattern"` syntax
- **Multi-drive** - Search all NTFS drives in parallel
- **Flexible output** - CSV, console, custom separators and columns
- **File type groups** - `--ext=pictures`, `--ext=documents`, `--ext=videos`

## Installation

Download the latest release:
- `uffs.com` - Command-line version
- `uffs.exe` - GUI version

Or build from source (requires Visual Studio 2022 + Boost 1.90).

## How it Works

Traditional file search tools ask the OS for files one at a time. Each request loads the entire MFT, extracts one file's info, then discards the MFT. Repeat millions of times.

UFFS reads the MFT once, parses all records in memory, and searches in parallel across drives. This is why it's so fast.

## Project Structure

```
├── bin/x64/           # Pre-built binaries
├── UltraFastFileSearch-code/
│   ├── src/
│   │   ├── cli/       # Command-line interface
│   │   ├── core/      # Core types and utilities
│   │   ├── gui/       # Windows GUI (WTL)
│   │   ├── index/     # In-memory file index
│   │   ├── io/        # Async I/O and MFT reading
│   │   ├── search/    # Pattern matching
│   │   └── util/      # Utilities
│   └── tests/         # Unit tests (doctest)
├── docs/              # Detailed architecture docs
└── boost/, wtl/       # Dependencies
```

## Building

1. Install Visual Studio 2022 with C++ Desktop Development
2. Extract `wtl/` from `Original Packages/WTL10_10320_Release.zip`
3. Download Boost 1.90 to `boost/`
4. Open `UltraFastFileSearch.sln` and build

## License

Based on [SwiftSearch](https://sourceforge.net/projects/swiftsearch/) by wfunction.

Licensed under **Creative Commons Attribution Non-Commercial License 2.0**.

Adapted by Robert Nio (2023) with CLI interface and code modernization.

