#include "stdafx.h"
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
    app_.description(
        "\n\t\tLocate files and folders by name instantly.\n\n"
        "\t\tUltra Fast File Search is a very fast file search utility\n"
        "\t\tthat can find files on your hard drive almost instantly.\n"
        "\t\tThe entire file system can be quickly sorted by name, size\n"
        "\t\tor date. Ultra Fast File Search supports all types of hard\n"
        "\t\tdrives, hard drive folders and network shares\n\n"
    );

    app_.set_version_flag("--version,-v", "Display version information");
    app_.set_help_flag("--help,-h", "Display available options");
    app_.add_flag("--help-list", "Display list of available options")->group("");
    app_.add_flag("--help-hidden", "Display all options including hidden")->group("");

    // Search options
    app_.add_option("searchPath", opts_.searchPath,
        "  <<< Search path. E.g. 'C:/' or 'C:/Prog*' >>>")->group("Search options");
    std::string drivesDesc = "Disk Drive(s) to search e.g. 'C:, D:' or any combination of ("
                            + diskDrives + ")\nDEFAULT: all disk drives";
    app_.add_option("--drives", opts_.drives, drivesDesc)->delimiter(',')->group("Search options");

    // Filter options
    app_.add_option("--ext", opts_.extensions,
        "File extensions e.g. '--ext=pdf' or '--ext=pdf,doc'")->delimiter(',')->group("Filter options");
    app_.add_flag("--case", opts_.caseSensitive,
        "Switch CASE sensitivity ON or OFF\t\t\t\t\tDEFAULT: False")->group("");
    app_.add_flag("--pass", opts_.bypassUAC,
        "Bypass User Access Control (UAC)\t\t\tDEFAULT: False")->group("");

    // Output options
    app_.add_option("--out", opts_.outputFilename,
        "Specify output filename\tDEFAULT: console")->group("Output options");
    app_.add_option("--header", opts_.includeHeader,
        "Include column header\tDEFAULT: True")->default_val(true)->group("Output options");
    app_.add_option("--sep", opts_.separator,
        "Column separator\t\tDEFAULT: ,")->default_val(",")->group("Output options");
    app_.add_option("--quotes", opts_.quotes,
        "Char/String to enclose output values\tDEFAULT: \"")->default_val("\"")->group("");
    app_.add_option("--pos", opts_.positiveMarker,
        "Marker for BOOLEAN attributes\t\tDEFAULT: 1")->default_val("1")->group("");
    app_.add_option("--neg", opts_.negativeMarker,
        "Marker for BOOLEAN attributes\t\tDEFAULT: 0")->default_val("0")->group("");
    app_.add_option("--columns", columnSet_,
        "OUTPUT Value-columns: e.g. '--columns=name,path,size,r,h,s'")->delimiter(',')
        ->check(CLI::IsMember({"all", "path", "name", "pathonly", "type", "size", "sizeondisk",
            "created", "written", "accessed", "decendents", "r", "a", "s", "h", "o",
            "notcontent", "noscrub", "integrity", "pinned", "unpinned",
            "directory", "compressed", "encrypted", "sparse", "reparse", "attributevalue"
        }))->group("Output options");

    // Diagnostic options
    app_.add_option("--dump-mft", opts_.dumpMftDrive,
        "Dump raw MFT to file in UFFS-MFT format. Usage: --dump-mft=<drive_letter>")->group("Output options");
    app_.add_option("--dump-mft-out", opts_.dumpMftOutput,
        "Output file path for raw MFT dump")->default_val("mft_dump.raw")->group("Output options");
    app_.add_option("--dump-extents", opts_.dumpExtentsDrive,
        "Dump MFT extent map as JSON. Usage: --dump-extents=<drive_letter>")->group("Output options");
    app_.add_option("--dump-extents-out", opts_.dumpExtentsOutput,
        "Output file path for MFT extent JSON (default: stdout)")->default_val("")->group("Output options");
    app_.add_flag("--verify", opts_.verifyExtents,
        "Verify extent mapping by reading first record from each extent")->group("Output options");
    app_.add_option("--benchmark-mft", opts_.benchmarkMftDrive,
        "Benchmark MFT read speed (read-only). Usage: --benchmark-mft=<drive_letter>")->group("Output options");
    app_.add_option("--benchmark-index", opts_.benchmarkIndexDrive,
        "Benchmark full index build. Usage: --benchmark-index=<drive_letter>")->group("Output options");
}

int CommandLineParser::parse(int argc, const char* const* argv) {
    try {
        app_.parse(argc, argv);
        
        // Check if --out was specified
        opts_.outputSpecified = app_.count("--out") > 0;
        
        // Convert column set to flags
        if (!columnSet_.empty()) {
            opts_.columnFlags = parseColumns(columnSet_);
            opts_.columnsSpecified = true;
        }
        
        opts_.parseResult = 0;
        return 0;
    } catch (const CLI::CallForHelp& e) {
        std::cout << app_.help() << std::endl;
        opts_.helpRequested = true;
        opts_.parseResult = 0;
        return 0;
    } catch (const CLI::CallForVersion& e) {
        PrintVersion();
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
    if (flags & COL_ALL) {
        flags = 0xFFFFFFFF;
    }
    return flags;
}

