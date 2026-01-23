# CLI Reference

## Introduction

This document provides exhaustive detail on the Ultra Fast File Search command-line interface (CLI). After reading this document, you should be able to:

1. Use `uffs.com` for scripted file searches
2. Understand all command-line options and their effects
3. Format output for integration with other tools
4. Build complex search queries with regex patterns
5. Integrate UFFS into automation workflows

---

## Overview: CLI Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         CLI Architecture                                 │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    Entry Point: main()                          │   │
│  ├─────────────────────────────────────────────────────────────────┤   │
│  │ • LLVM CommandLine argument parsing                             │   │
│  │ • Option validation and defaults                                │   │
│  │ • Output file/console setup                                     │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                              │                                          │
│                              ▼                                          │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    Search Execution                              │   │
│  ├─────────────────────────────────────────────────────────────────┤   │
│  │ • Drive enumeration and filtering                               │   │
│  │ • Pattern compilation (glob or regex)                           │   │
│  │ • MFT reading via IoCompletionPort                              │   │
│  │ • Parallel volume processing                                    │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                              │                                          │
│                              ▼                                          │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    Output Formatting                             │   │
│  ├─────────────────────────────────────────────────────────────────┤   │
│  │ • Column selection and ordering                                 │   │
│  │ • Separator and quote handling                                  │   │
│  │ • UTF-8 encoding for file output                                │   │
│  │ • Buffered writing for performance                              │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Quick Start

### Basic Usage

```batch
:: Search for all .cpp files
uffs.com "*.cpp"

:: Search for files in a specific directory
uffs.com "C:\Projects\*.py"

:: Search with regex pattern (prefix with >)
uffs.com ">C:\\Projects\\.*\.py"

:: Output to file
uffs.com "*.txt" --out=results.csv

:: Select specific columns
uffs.com "*.doc" --columns=path,size,written
```

### Help

```batch
uffs.com --help
```

---

## Command-Line Options

### Option Categories

| Category | Purpose |
|----------|---------|
| Search Options | Pattern, drives, search scope |
| Filter Options | File extensions, case sensitivity |
| Output Options | Columns, format, file output |

### Search Options

#### Search Pattern (Positional)

```batch
uffs.com "<pattern>"
```

| Pattern Type | Syntax | Example |
|--------------|--------|---------|
| Glob | Standard wildcards | `*.cpp`, `test?.txt` |
| Path glob | Full path with wildcards | `C:\Projects\*.py` |
| Regex | Prefix with `>` | `>C:\\.*\.cpp` |

**Glob Wildcards**:
- `*` - Match any characters
- `?` - Match single character

**Regex Notes**:
- Prefix pattern with `>` to enable regex mode
- Use `\\` for path separators in regex
- Case-insensitive by default

#### --drives

Specify which drives to search.

```batch
:: Search only C: drive
uffs.com "*.txt" --drives=C

:: Search multiple drives
uffs.com "*.txt" --drives=C,D,E

:: Search all drives (default)
uffs.com "*.txt" --drives=*
```

### Filter Options

#### --ext

Filter by file extension(s).

```batch
:: Single extension
uffs.com "*" --ext=pdf

:: Multiple extensions
uffs.com "*" --ext=pdf,doc,docx

:: Extension groups
uffs.com "*" --ext=pictures    # .jpg, .png, .tiff
uffs.com "*" --ext=documents   # .doc, .txt, .pdf
uffs.com "*" --ext=videos      # .mpeg, .mp4
uffs.com "*" --ext=music       # .mp3, .wav
```

#### --case (Hidden)

Enable case-sensitive matching.

```batch
uffs.com "README*" --case=true
```

### Output Options

#### --out

Specify output file. Default: console.

```batch
:: Output to file
uffs.com "*.cpp" --out=results.csv

:: Output to console (default)
uffs.com "*.cpp" --out=console

:: Shorthand for uffs.csv
uffs.com "*.cpp" --out=f
```

#### --columns

Select which columns to output.

```batch
:: Specific columns
uffs.com "*.cpp" --columns=path,name,size

:: All columns
uffs.com "*.cpp" --columns=all
```

**Available Columns**:

| Column | Description | Example Output |
|--------|-------------|----------------|
| `path` | Full path including filename | `C:\Projects\main.cpp` |
| `name` | Filename only | `main.cpp` |
| `pathonly` | Directory path only | `C:\Projects\` |
| `type` | File type | `cpp` |
| `size` | Actual file size (bytes) | `12345` |
| `sizeondisk` | Allocated size on disk | `16384` |
| `created` | Creation timestamp | `2024-01-15 10:30:00` |
| `written` | Last modified timestamp | `2024-01-20 14:45:00` |
| `accessed` | Last accessed timestamp | `2024-01-20 15:00:00` |
| `descendants` | Child count (directories) | `42` |
| `r` | Read-only attribute | `1` or `0` |
| `a` | Archive attribute | `1` or `0` |
| `s` | System attribute | `1` or `0` |
| `h` | Hidden attribute | `1` or `0` |
| `o` | Offline attribute | `1` or `0` |
| `notcontent` | Not content indexed | `1` or `0` |
| `noscrub` | No scrub attribute | `1` or `0` |
| `integrity` | Integrity attribute | `1` or `0` |
| `pinned` | Pinned attribute | `1` or `0` |
| `unpinned` | Unpinned attribute | `1` or `0` |
| `directory` | Is directory | `1` or `0` |
| `compressed` | Is compressed | `1` or `0` |
| `encrypted` | Is encrypted | `1` or `0` |
| `sparse` | Is sparse file | `1` or `0` |
| `reparse` | Is reparse point | `1` or `0` |
| `attributevalue` | Numeric attribute value | `32` |

#### --header

Include column headers in output.

```batch
:: With headers (default)
uffs.com "*.cpp" --header=true

:: Without headers
uffs.com "*.cpp" --header=false
```

#### --sep

Column separator character.

```batch
:: Comma (default)
uffs.com "*.cpp" --sep=,

:: Tab
uffs.com "*.cpp" --sep=TAB

:: Semicolon
uffs.com "*.cpp" --sep=SEMICOLON

:: Pipe
uffs.com "*.cpp" --sep=PIPE

:: Space
uffs.com "*.cpp" --sep=SPACE

:: Newline (one column per line)
uffs.com "*.cpp" --sep=NEWLINE
```

**Separator Keywords**:

| Keyword | Character |
|---------|-----------|
| `TAB` | `\t` |
| `COMMA` | `,` |
| `SEMICOLON` | `;` |
| `PIPE` | `\|` |
| `SPACE` | ` ` |
| `NEWLINE` | `\n` |
| `RETURN` | `\r` |
| `NULL` | `\0` |

#### --quotes (Hidden)

Character to enclose values.

```batch
:: Double quotes (default)
uffs.com "*.cpp" --quotes=\"

:: Single quotes
uffs.com "*.cpp" --quotes=\'

:: Custom
uffs.com "*.cpp" --quotes=-*-
```

#### --pos / --neg (Hidden)

Markers for boolean attributes.

```batch
:: Default: 1/0
uffs.com "*.cpp" --columns=r,h,s

:: Custom markers
uffs.com "*.cpp" --columns=r,h,s --pos=Yes --neg=No
uffs.com "*.cpp" --columns=r,h,s --pos=+ --neg=-
```

---

## Pattern Syntax

### Glob Patterns

Standard Windows glob patterns:

```batch
:: All files
uffs.com "*"

:: All .cpp files
uffs.com "*.cpp"

:: Files starting with "test"
uffs.com "test*"

:: Single character wildcard
uffs.com "file?.txt"

:: Path with wildcards
uffs.com "C:\Projects\*\src\*.cpp"
```

### Regex Patterns

Prefix with `>` to enable regex mode:

```batch
:: All .cpp or .hpp files
uffs.com ">.*\.(cpp|hpp)"

:: Files with numbers in name
uffs.com ">.*[0-9]+.*"

:: Case-sensitive regex
uffs.com ">(?-i)README.*"

:: Path matching
uffs.com ">C:\\Projects\\.*\\src\\.*\.cpp"
```

**Regex Features**:
- Full Boost.Xpressive regex support
- Case-insensitive by default (`(?i)`)
- Use `(?-i)` for case-sensitive
- Escape backslashes: `\\`

---

## Output Formats

### CSV Output

```batch
uffs.com "*.cpp" --out=results.csv --columns=path,size,written
```

Output:
```csv
"Path","Size","Last Written"
"C:\Projects\main.cpp","12345","2024-01-20 14:45:00"
"C:\Projects\util.cpp","5678","2024-01-19 10:30:00"
```

### TSV Output

```batch
uffs.com "*.cpp" --out=results.tsv --sep=TAB --columns=path,size
```

### Pipe-Delimited

```batch
uffs.com "*.cpp" --sep=PIPE --columns=name,size
```

Output:
```
"Name"|"Size"
"main.cpp"|"12345"
"util.cpp"|"5678"
```

### No Headers

```batch
uffs.com "*.cpp" --header=false --columns=path
```

Output:
```
"C:\Projects\main.cpp"
"C:\Projects\util.cpp"
```



---

## Examples

### Find Large Files

```batch
:: Find files larger than 1GB (requires post-processing)
uffs.com "*" --columns=path,size | findstr /R "^.*,[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]"
```

### Export Directory Listing

```batch
:: Export all files on C: drive
uffs.com "C:\*" --out=c_drive.csv --columns=all
```

### Find Recently Modified Files

```batch
:: List all files with modification dates
uffs.com "*" --columns=path,written --out=recent.csv
```

### Search Specific Extensions

```batch
:: Find all source code files
uffs.com "*" --ext=cpp,hpp,c,h,py,js,ts --columns=path,size
```

### Integration with PowerShell

```powershell
# Import UFFS results into PowerShell
$files = uffs.com "*.cpp" --columns=path,size --header=false --sep=TAB |
    ConvertFrom-Csv -Delimiter "`t" -Header "Path","Size"

# Process results
$files | Where-Object { [int]$_.Size -gt 10000 } |
    Select-Object Path
```

### Integration with Python

```python
import subprocess
import csv

# Run UFFS and capture output
result = subprocess.run(
    ['uffs.com', '*.py', '--columns=path,size', '--header=true'],
    capture_output=True, text=True
)

# Parse CSV output
reader = csv.DictReader(result.stdout.splitlines())
for row in reader:
    print(f"{row['Path']}: {row['Size']} bytes")
```

---

## Error Handling

### Common Errors

| Error | Cause | Solution |
|-------|-------|----------|
| Invalid DRIVE LETTER | Drive doesn't exist | Check available drives |
| Output File ERROR | Cannot create output file | Check path and permissions |
| Invalid pattern | Malformed regex | Check regex syntax |

### Exit Codes

| Code | Meaning |
|------|---------|
| 0 | Success |
| -13 | Invalid drive letter |
| Other | Windows error code |

---

## Performance Tips

1. **Limit drives**: Use `--drives` to search only needed drives
2. **Use specific patterns**: More specific patterns = faster search
3. **Limit columns**: Only request needed columns
4. **File output**: Writing to file is faster than console
5. **Disable headers**: Use `--header=false` for large exports

---

## Comparison: GUI vs CLI

| Feature | GUI (uffs.exe) | CLI (uffs.com) |
|---------|----------------|----------------|
| Interactive | ✓ | ✗ |
| Scriptable | ✗ | ✓ |
| Real-time results | ✓ | ✗ |
| File output | ✓ | ✓ |
| Column selection | Fixed | Flexible |
| Shell integration | ✓ | ✗ |
| Batch processing | ✗ | ✓ |

---

## Summary

The UFFS CLI provides:

1. **Full search capability** - Same speed as GUI
2. **Flexible output** - CSV, TSV, custom formats
3. **25+ columns** - Path, size, timestamps, attributes
4. **Regex support** - Full Boost.Xpressive patterns
5. **Scriptable** - Integration with PowerShell, Python, etc.

Key command patterns:

```batch
# Basic search
uffs.com "pattern"

# With options
uffs.com "pattern" --drives=C,D --ext=cpp --columns=path,size --out=results.csv

# Regex search
uffs.com ">regex_pattern" --columns=path
```

---

## See Also

- [01-architecture-overview.md](01-architecture-overview.md) - System architecture
- [05-pattern-matching-engine.md](05-pattern-matching-engine.md) - Pattern syntax details
- [08-search-algorithm.md](08-search-algorithm.md) - Search implementation
- [09-build-system.md](09-build-system.md) - Building CLI version