# Build System

## Introduction

This document provides exhaustive detail on building Ultra Fast File Search from source. After reading this document, you should be able to:

1. Set up the development environment
2. Configure dependencies (Boost, WTL, LLVM)
3. Build both GUI and CLI versions
4. Understand compiler flags and optimizations
5. Troubleshoot common build issues

---

## Overview: Build Configurations

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      Build Configurations                                │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    EXE Configuration                             │   │
│  ├─────────────────────────────────────────────────────────────────┤   │
│  │ Output:     uffs.exe                                            │   │
│  │ Subsystem:  Windows GUI                                         │   │
│  │ Entry:      WinMain                                             │   │
│  │ Purpose:    Interactive file search with GUI                    │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    COM Configuration                             │   │
│  ├─────────────────────────────────────────────────────────────────┤   │
│  │ Output:     uffs.com                                            │   │
│  │ Subsystem:  Console                                             │   │
│  │ Entry:      main                                                │   │
│  │ Purpose:    Command-line interface for scripting                │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│  Both configurations share the same source code.                        │
│  The subsystem determines which entry point is used.                    │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Prerequisites

### Required Software

| Software | Version | Purpose |
|----------|---------|---------|
| Visual Studio | 2017+ | C++ compiler and IDE |
| Windows SDK | 10.0+ | Windows API headers and libraries |
| ATL | VS 2017+ | Active Template Library |
| Boost | 1.90.0 | Xpressive, Algorithm libraries (header-only) |
| WTL | r636 | Windows Template Library for GUI |
| LLVM | 10.0+ | CommandLine parser for CLI |

### Directory Structure

The default build expects this directory layout:

```
C:\uffs\
├── boost_1_90_0\           # Boost library (header-only)
│   ├── boost\              # Headers
│   └── libs\               # Documentation/examples only
├── wtl-code-r636-trunk\    # WTL library
│   └── wtl\
│       └── Include\        # WTL headers
└── llvm\                   # LLVM library
    ├── llvm\
    │   └── include\        # LLVM headers
    ├── build\
    │   └── include\        # Generated headers
    └── lib\
        ├── Debug\          # Debug libraries
        │   └── LLVMSupport.lib
        └── Release\        # Release libraries
            └── LLVMSupport.lib
```

---

## Project Files

### Solution Structure

```
UltraFastFileSearch-code/
├── UltraFastFileSearch.sln      # Visual Studio solution
├── UltraFastFileSearch.vcxproj  # Main project file
├── UltraFastFileSearch.vcxproj.filters  # File organization
├── UltraFastFileSearch.vcxproj.user     # User settings
└── Project.props                # Shared MSBuild properties
```

### Project.props: Shared Configuration

```xml
<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup>
    <BOOST_ROOT>C:\uffs\boost_1_90_0\</BOOST_ROOT>
    <XPDeprecationWarning>false</XPDeprecationWarning>
    <IncludePath>C:\uffs\wtl-code-r636-trunk\wtl\Include;$(IncludePath)</IncludePath>
  </PropertyGroup>

  <ItemDefinitionGroup>
    <PreBuildEvent>
      <Command>CScript //nologo "$(ProjectDir)$(ProjectName).js" PreBuild ...</Command>
    </PreBuildEvent>
    <PreLinkEvent>
      <Command>CScript //nologo "$(ProjectDir)$(ProjectName).js" PreLink ...</Command>
    </PreLinkEvent>
    <PostBuildEvent>
      <Command>CScript //nologo "$(ProjectDir)$(ProjectName).js" PostBuild ...</Command>
    </PostBuildEvent>

    <ClCompile>
      <ForcedIncludeFiles>stdafx.h;$(ProjectDir)targetver.h</ForcedIncludeFiles>
      <AdditionalOptions>/d2Zi+</AdditionalOptions>
      <WarningLevel>EnableAllWarnings</WarningLevel>
      <MinimalRebuild>false</MinimalRebuild>
      <SupportJustMyCode>false</SupportJustMyCode>
    </ClCompile>

    <Link>
      <UACExecutionLevel>RequireAdministrator</UACExecutionLevel>
      <SubSystem>Windows</SubSystem>
    </Link>
  </ItemDefinitionGroup>
</Project>
```

**Key Settings**:
- `BOOST_ROOT`: Path to Boost installation
- `UACExecutionLevel`: Requires admin (for raw disk access)
- `ForcedIncludeFiles`: Precompiled header injection
- `/d2Zi+`: Enhanced debug info for optimized builds

---

## Build Configurations

### Configuration Matrix

| Configuration | Platform | Output | Subsystem | Optimization |
|---------------|----------|--------|-----------|--------------|
| EXE DEBUG | x64 | uffs.exe | Windows | Disabled |
| EXE | x64 | uffs.exe | Windows | Full + LTCG |
| COM DEBUG | x64 | uffs.com | Console | Disabled |
| COM | x64 | uffs.com | Console | Full + LTCG |

### EXE Release Configuration

```xml
<ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='EXE|x64'">
  <ClCompile>
    <PrecompiledHeader>Use</PrecompiledHeader>
    <RuntimeLibrary>MultiThreaded</RuntimeLibrary>
    <DEBUGInformationFormat>ProgramDatabase</DEBUGInformationFormat>
    <Optimization>Full</Optimization>
    <AdditionalIncludeDirectories>
      C:\uffs\llvm\llvm\include;
      C:\uffs\boost_1_90_0;
      C:\uffs\llvm\build\include
    </AdditionalIncludeDirectories>
    <MultiProcessorCompilation>true</MultiProcessorCompilation>
    <InlineFunctionExpansion>AnySuitable</InlineFunctionExpansion>
    <IntrinsicFunctions>true</IntrinsicFunctions>
    <FavorSizeOrSpeed>Speed</FavorSizeOrSpeed>
    <BufferSecurityCheck>false</BufferSecurityCheck>
  </ClCompile>
  <Link>
    <GenerateDEBUGInformation>true</GenerateDEBUGInformation>
    <IgnoreSpecificDefaultLibraries>msvcprt.lib</IgnoreSpecificDefaultLibraries>
    <SubSystem>Windows</SubSystem>
    <AdditionalDependencies>
      C:\uffs\llvm\lib\Release\LLVMSupport.lib
    </AdditionalDependencies>
    <LinkTimeCodeGeneration>UseLinkTimeCodeGeneration</LinkTimeCodeGeneration>
  </Link>
</ItemDefinitionGroup>
```

**Optimization Flags**:
- `/O2` (Full): Maximum speed optimization
- `/Oi`: Enable intrinsic functions
- `/Ob2`: Inline any suitable function
- `/Ot`: Favor speed over size
- `/GS-`: Disable buffer security checks (performance)
- `/GL` + `/LTCG`: Link-time code generation

### COM Release Configuration

```xml
<ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='COM|x64'">
  <ClCompile>
    <!-- Same as EXE, but with console subsystem -->
    <WholeProgramOptimization>true</WholeProgramOptimization>
  </ClCompile>
  <Link>
    <Subsystem>Console</Subsystem>
    <EnableCOMDATFolding>true</EnableCOMDATFolding>
    <OptimizeReferences>true</OptimizeReferences>
    <TargetMachine>MachineX64</TargetMachine>
  </Link>
</ItemDefinitionGroup>
```

**Additional Linker Optimizations**:
- `/OPT:REF`: Remove unreferenced functions
- `/OPT:ICF`: Fold identical COMDATs

---

## Source Files

### Compiled Files

| File | Purpose |
|------|---------|
| `UltraFastFileSearch.cpp` | Main source (~13,400 lines) |
| `string_matcher.cpp` | Pattern matching engine |
| `stdafx.cpp` | Precompiled header creation |

### Boost Integration

The project uses **header-only** Boost libraries (Boost 1.90.0):
- **Boost.Xpressive** - Regex engine (header-only)
- **Boost.Algorithm** - Boyer-Moore-Horspool string search (header-only)

No Boost source files need to be compiled.

### Header Files

| File | Purpose |
|------|---------|
| `stdafx.h` | Precompiled header |
| `targetver.h` | Windows version targeting |
| `resource.h` | Resource IDs |
| `BackgroundWorker.hpp` | Threading infrastructure |
| `string_matcher.hpp` | Pattern matching declarations |
| `path.hpp` | Path manipulation utilities |
| `nformat.hpp` | Number formatting |
| `ShellItemIDList.hpp` | Shell integration |
| `CModifiedDialogImpl.hpp` | Custom dialog base |
| `NtUserCallHook.hpp` | NT user call hooks |
| `WinDDKFixes.hpp` | DDK compatibility |

---

## Precompiled Header

### stdafx.h Structure

```cpp
#pragma once

#include "targetver.h"

// Warning suppression
#pragma warning(push)
#pragma warning(disable: 4595)  // non-member operator new/delete inline
#pragma warning(disable: 4571)  // catch(...) semantics changed

#include "WinDDKFixes.hpp"

// Clang compatibility
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++98-compat-pedantic"
// ... many more clang warnings disabled ...
#endif

// C runtime
#include <process.h>
#include <stddef.h>
#include <malloc.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <tchar.h>
#include <time.h>

// C++ standard library
#ifdef __cplusplus
#include <algorithm>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>
#endif

#pragma warning(pop)
```

### Precompiled Header Usage

```xml
<ClCompile Include="stdafx.cpp">
  <PrecompiledHeader>Create</PrecompiledHeader>
</ClCompile>
<ClCompile Include="UltraFastFileSearch.cpp">
  <PrecompiledHeader>Use</PrecompiledHeader>
</ClCompile>
```

---

## Dependencies

### Boost Library

**Version**: 1.90.0 (header-only)

**Required Components**:
- `boost/xpressive/xpressive.hpp` - Regex engine
- `boost/algorithm/searching/boyer_moore_horspool.hpp` - String search

**Configuration**:
```xml
<PropertyGroup>
  <BOOST_ROOT>C:\uffs\boost_1_90_0\</BOOST_ROOT>
</PropertyGroup>
<ClCompile>
  <AdditionalIncludeDirectories>$(BOOST_ROOT);...</AdditionalIncludeDirectories>
</ClCompile>
```

### WTL (Windows Template Library)

**Required Version**: r636 or later

**Configuration**:
```xml
<PropertyGroup>
  <IncludePath>C:\uffs\wtl-code-r636-trunk\wtl\Include;$(IncludePath)</IncludePath>
</PropertyGroup>
```

**Used Components**:
- `atlapp.h` - Application framework
- `atlframe.h` - Frame windows
- `atlctrls.h` - Common controls
- `atldlgs.h` - Dialog boxes
- `atlctrlx.h` - Extended controls

### LLVM Support Library

**Purpose**: Command-line argument parsing for CLI version

**Configuration**:
```xml
<ClCompile>
  <AdditionalIncludeDirectories>
    C:\uffs\llvm\llvm\include;
    C:\uffs\llvm\build\include
  </AdditionalIncludeDirectories>
</ClCompile>
<Link>
  <AdditionalDependencies>
    C:\uffs\llvm\lib\Release\LLVMSupport.lib
  </AdditionalDependencies>
</Link>
```

**Used Components**:
- `llvm/Support/CommandLine.h` - Argument parsing
- `llvm/Support/raw_ostream.h` - Output streams

---

## Alternative Build: Clang

### Clang-Compile.bat

For building with Clang instead of MSVC:

```batch
@SetLocal
@Set "BASEDIR=%SystemDrive%\WinDDK\3790.1830"
@Set "INCLUDE=%BASEDIR%\inc\crt;%BASEDIR%\inc\wnet;%BASEDIR%\inc\atl30"
@Set "LIB=%BASEDIR%\lib\crt\amd64;%BASEDIR%\lib\atl\amd64"
@Set "WTL_ROOT=...\wtl\Include"
@Set "BOOST_ROOT=C:\Program Files\boost\boost_1_90_0"

clang-cl -fmsc-version=1300 ^
    /Og /Oi /Os /Oy /Ob2 /Gs /GF /Gy ^
    /DUNICODE=1 /D_UNICODE=1 ^
    /EHsc /MD ^
    /Zc:sizedDealloc- /Zc:threadSafeInit- ^
    /I "%WTL_ROOT%" /I "%BOOST_ROOT%" ^
    /FI stdafx.h ^
    UltraFastFileSearch.cpp string_matcher.cpp ^
    /link /Subsystem:Windows
```

**Clang-specific Flags**:
- `-fmsc-version=1300`: MSVC compatibility mode
- `/Zc:sizedDealloc-`: Disable sized deallocation
- `/Zc:threadSafeInit-`: Disable thread-safe statics

---

## Resources

### Resource File (UltraFastFileSearch.rc)

```
UltraFastFileSearch.rc
├── Dialog: IDD_MAINDLG          # Main window
├── Menu: IDR_MAINMENU           # Context menus
├── Accelerator: IDR_ACCELERATOR1 # Keyboard shortcuts
├── Icon: IDI_ICON1              # Application icon
├── String Table                  # Localized strings
└── Version Info                  # File version
```

### Resource IDs (resource.h)

```cpp
#define IDD_MAINDLG                     101
#define IDR_MAINMENU                    102
#define IDR_ACCELERATOR1                103
#define IDI_ICON1                       104
#define IDC_LISTFILES                   1001
#define IDC_LISTVOLUMES                 1002
#define IDC_EDITFILENAME                1003
#define IDC_BUTTON_BROWSE               1004
// ... more IDs ...
```

---

## Build Steps

### Building from Visual Studio

1. Open `UltraFastFileSearch.sln`
2. Select configuration (EXE or COM, Debug or Release)
3. Build → Build Solution (F7)

### Building from Command Line

```batch
:: Set up Visual Studio environment
call "C:\Program Files\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvars64.bat"

:: Build Release EXE
msbuild UltraFastFileSearch.vcxproj /p:Configuration=EXE /p:Platform=x64

:: Build Release COM
msbuild UltraFastFileSearch.vcxproj /p:Configuration=COM /p:Platform=x64
```

### Build Events

The project uses JavaScript build events:

```xml
<PreBuildEvent>
  <Command>CScript //nologo "$(ProjectDir)$(ProjectName).js" PreBuild ...</Command>
</PreBuildEvent>
```

These events handle:
- Version number updates
- Resource compilation
- Post-build packaging

---

## Troubleshooting

### Common Issues

| Issue | Solution |
|-------|----------|
| Missing Boost | Set `BOOST_ROOT` in Project.props |
| Missing WTL | Add WTL include path to Project.props |
| Missing LLVM | Build LLVM and set paths in vcxproj |
| Link errors | Ensure correct runtime library (MT vs MD) |
| Admin required | UAC manifest requires elevation |

### Dependency Paths

If dependencies are in non-standard locations, update:

1. `Project.props`:
   ```xml
   <BOOST_ROOT>your\path\to\boost\</BOOST_ROOT>
   <IncludePath>your\path\to\wtl\Include;$(IncludePath)</IncludePath>
   ```

2. `UltraFastFileSearch.vcxproj`:
   ```xml
   <AdditionalIncludeDirectories>
     your\path\to\llvm\include;...
   </AdditionalIncludeDirectories>
   <AdditionalDependencies>
     your\path\to\LLVMSupport.lib;...
   </AdditionalDependencies>
   ```

---

## Summary

Building UFFS requires:

1. **Visual Studio 2017+** with C++ and ATL
2. **Boost 1.90.0** for regex and algorithms (header-only)
3. **WTL r636** for GUI components
4. **LLVM** for CLI argument parsing

Key build characteristics:

| Aspect | Setting |
|--------|---------|
| Platform | x64 only |
| Toolset | v141_xp (VS 2017 XP-compatible) |
| Character Set | Unicode |
| Runtime | Static (/MT) |
| Optimization | Full + LTCG |
| UAC | RequireAdministrator |

---

## See Also

- [01-architecture-overview.md](01-architecture-overview.md) - System architecture
- [The Manual.md](../UltraFastFileSearch-code/The%20Manual.md) - User documentation
