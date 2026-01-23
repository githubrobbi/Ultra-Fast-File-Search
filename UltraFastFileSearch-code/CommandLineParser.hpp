#pragma once
#include "CLI11.hpp"
#include <string>
#include <vector>
#include <set>
#include <cstdint>
#include <iostream>

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
    uint32_t columnFlags = 0;  // 0 means no columns specified (use default behavior)
    bool columnsSpecified = false;  // Track if --columns was used
    
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
    bool outputSpecified = false;  // Track if --out was used
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

