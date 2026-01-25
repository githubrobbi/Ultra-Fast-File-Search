// ============================================================================
// String Utilities - Implementation
// ============================================================================

#include "string_utils.hpp"

#include <algorithm>
#include <cstdio>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// ============================================================================
// Drive Enumeration
// ============================================================================

// Buffer length
static DWORD maxdrives = 250;
// Buffer for drive string storage
static char lpBuffer[100];

std::string drivenames(void)
{
    std::string drives = "";

    DWORD test;

    int i;

    test = GetLogicalDriveStringsW(maxdrives, (LPWSTR)lpBuffer);

    if (test != 0)

    {
        //printf("GetLogicalDriveStrings() return value: %d, Error (if any): %d \n", test, GetLastError());

        //printf("The logical drives of this machine are:\n");

        //if (GetLastError() != 0) printf("Trying to find all physical DISKS failed!!! Error code: %d\n", GetLastError());

        // Check up to 100 drives...

        for (i = 0; i < 100; i++)

            drives += lpBuffer[i];
    }
    else

        //printf("GetLogicalDriveStrings() is failed lor!!! Error code: %d\n", GetLastError());
        printf("Trying to find all physical DISKS failed!!! Error code: %lu\n", GetLastError());

    return drives;

}

// ============================================================================
// String Manipulation
// ============================================================================

void replaceAll(std::string& str, const std::string& from, const std::string& to)
{
    if (from.empty())
        return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos)
    {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();	// In case 'to' contains 'from', like replacing 'x' with 'yx' 
    }

}

// Function to remove all spaces from a given string 
std::string removeSpaces(std::string str)
{
    str.erase(remove(str.begin(), str.end(), '\0'), str.end());
    return str;
}

