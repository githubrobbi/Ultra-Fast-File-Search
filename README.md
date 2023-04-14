# Ultra Fast File Search

------
Ultra Fast File Search for Windows: Command-line tool for lightning-fast searches (non-commercial use only)

- [Ultra Fast File Search](#ultra-fast-file-search)
  * [TL;DR](#tldr)
    + [Simple search](#simple-search)
    + [REGEX search](#regex-search)
    + [Output location](#output-location)
    + [Need more HELP?](#need-more-help-)
  * [MORE Information](#more-information)
    + [General Information](#general-information)
    + [HASHES / SIGNATURES](#hashes--signatures)
    + [Why does the tool need elevated access](#why-does-the-tool-need-elevated-access)
    + [Other tools](#other-tools)
      - [EVERYTHING](#everything)
      - [WizFile](#wizfile)
      - [Ultra Fast File Search](#ultra-fast-file-search)
        * [Main Differences](#main-differences)
      - [BENCHMARK](#benchmark)
    + [Search options](#search-options)
      - [DEFAULTS](#defaults)
      - [BASIC search](#basic-search)
      - [GLOB search (DEFAULT)](#glob-search-default)
      - [Regular-Expression Search (REGEX)](#regular-expression-search-regex)
        * [Some common regular expressions](#some-common-regular-expressions)
        * ["Quantifiers" can follow any expression](#-quantifiers--can-follow-any-expression)
        * [Examples of regular expressions](#examples-of-regular-expressions)
      - [--case (NOT implemented yet)](#--case--not-implemented-yet-)
      - [--drives](#--drives)
      - [--ext](#--ext)
    + [Output Options](#output-options)
      - [DEFAULTS](#defaults-1)
      - [--out](#--out)
      - [--header](#--header)
      - [--columns](#--columns)
      - [--neg](#--neg)
      - [--pos](#--pos)
      - [--quotes](#--quotes)
      - [--sep](#--sep)
  * [Compile Your Version](#compile-your-version)
	+ [Tool Chain](#tool-chain)
	+ [Directory Tree of Files](#directory-tree-of-files)
  * [Known Issues](#known-issues)
  * [TODO](#todo)
  * [License](#license)


## TL;DR

### Why does the tool need elevated access:

**UFFS** and any other tools in this category require administrative privileges for low level read access to NTFS volumes. Windows manages this through it's **U**ser **A**ccess **C**ontrol (UAC).

Some people do not want this "nag" and work around it.

UAC explained:

https://docs.microsoft.com/en-us/windows/security/identity-protection/user-account-control/how-user-account-control-works

### Simple search

| Search                                | Result                                                       |
| ------------------------------------- | ------------------------------------------------------------ |
| `uffs c:/pro*`                        | Finds all files & folders starting with "\pro" on drive C:   |
| `uffs /pro*.txt`                      | Finds all TEXT files starting with "\pro*" on ALL disks      |
| `uffs /pro*.txt --drives=c,d,m`       | Finds all TEXT files starting with "\pro*" on drives C:, D:, and M: |
| `uffs /pro** --ext=jpg,mp4,documents` | Finds all DOCUMENTS, JPG, and MP4 file types, on ALL disks   |

### REGEX search

| Regular-Expressions (REGEX)                          | Result                                           |
| ---------------------------------------------------- | ------------------------------------------------------------ |
| MUST start with `>` and be encapsulated with `"`     | NO Error :)                                                  |
| `uffs ">C:\\TemP.*\.txt"`                            | Finds all text files on C: in folder **temp** (by default the tool is NOT case sensitive) and its subdirectories |
| `uffs ">.*\\DatA.*\d{3}-\d{2}-\d{4}.*" --drives=d,c` | Finds all files & directories with a "social security number" in their names on drive C: and D: along with `data` string in their path |

### Output location

| Search                                                       | Result                                           |
| ------------------------------------------------------------ | ------------------------------------------------------------ |
| `uffs d:/data** --ext=pictures --out=console`                | Finds all pictures on D: and outputs it into the CMD window  |
| `uffs * --ext=movies --out=bigfile.csv`                      | All MOVIE files will be saved to `bigfile.csv`               |
| `uffs * --out=bigfile.csv --header=true --sep=; --columns=path,size,created` | Creates bigfile.csv with a header line and the columns PATH, SIZE, and file CREATION |

### Need more HELP?
| `uffs.com --help`       | Get more help                         |
| ----------------------- | ------------------------------------- |
| `uffs.com --help-list`  | - Displays list of available options   |
| `uffs.com --help-hidden` | - Displays all options incl. advanced options |
| `uffs.com--version`     | - Displays the version of this program |

## MORE Information

NOTE: Reading the MFT needs ADMIN rights. On Windows it will trigger a UAC prompt indicating that the tool will want to read the MFT directly. 

### HASHES / SIGNATURES:

Command Line Utility (CLI)

**Name: 		uffs.com**
Size: 		 1540608 bytes (1541 KiB)

SHA256:		
5bd141200fd06c8f216ee116f025d139178d562c307b2d9abf4b8005f7b9c419 *uffs.com

VERIFY with: shasum -a 256 -c uffs.com.sha256

Graphical User Interface (GUI)

**Name: 		uffs.exe**
Size: 		4494848 bytes (4495 KiB)

SHA256:		
f88c268aae75aad4fae771e26ee7e4acc3692cd0e9c96f9fd831370508068756 *uffs.exe

VERIFY with: shasum -a 256 -c uffs.com.sha256

### General Information

The MAGIC behind this lightning fast search engine is reading the MFT (like a phonebook) of the disks.

Traditionally almost all file search tools / functions (e.g. os.walk in python) use this simple process:

1. Ask the OS to find a file (or next one).
2. The OS will go and get the MFT (the COMPLETE phonebook) and get the file info & attributes.
3. This info (NOT the MFT) is passed on to the tool/function.
4. Throw away the MFT (BTW. it has ALL the info for ALL the files and we just took ONE file info)
5. Repeat for the NEXT file.

I knew about this in principle, but never came across a tool that was able to do this as EFFICIENTLY as UFFS. It can query all disks in parallel. Once a MFT is loaded ... it starts parsing the content to match your query ... this is in parallel to all the other processes too. ==> **LIGHTNING FAST !**

### Other tools

#### EVERYTHING

https://www.voidtools.com/

On startup it will create a database and keep it in memory.

It can get around the UAC prompt by installing it as a service (constantly running in the background)

The CLI version is just a "pipe" using inter process communications to access the everything process running as a service or as a separate task.

The tool is geared towards retail customers supporting creations of music playlists etc.

It will FORK a second process to build up the database and be ready for next call (default option). Giving the illusion that the tool is even faster the second time around.

**==> UFFS is 68%  faster !**

#### WizFile 
https://antibody-software.com/web/software/software/wizfile-finds-your-files-fast/

Another tool for the retail consumer. Lots of features to slice and dice playlists.

The main interface is the GUI . CLI is done as an "afterthought" ... you will run the GUI version with some CLI  switches.

It took a LONG time to just read the MFT of my disk of 6.5 Million records ... 5 minutes 

**==> UFFS is 4 times faster !**

#### Ultra Fast File Search

This version of the tool is geared to the IT professional at home.

It lends itself to automation and customizing the results in any shape or form.

##### Main Adaptations

1. Modern and powerful CLI parser.
2. Search strings can be more windows / DOS like and less complicated.
3. Understands pathnames in LINUX and WINDOWS formats.
4. Powerful output engine; enabling you to customize every aspect of it.

#### BENCHMARK

1 SSD / 4 HardDisks / total of 19 Million records

| Program     | # of Records    | Time used to load and sort | NOTE             |
| ----------- | --------------- | -------------------------- | ---------------- |
| **UFFS**    | **19 Million**  | **121 seconds**            | **all disks**    |
|             | **6.5 Million** | **56 seconds**             | **1 Hard Drive** |
| Everything  | 19 Million      | 178 seconds                | all disks        |
| WizFile     | 6.5 Million     | 299 seconds                | 1 Hard Drive     |


### Search options

#### DEFAULTS

| Option        | Default Value                                                | Type   |
| ------------- | ------------------------------------------------------------ | ------ |
| --case=false  | **NOT** case sensitive                                       | bool   |


#### BASIC search

​				search for **needle**

​				**uffs.com needle**

​				This will search on ALL disks for **needle**

#### GLOB search (DEFAULT)
​				https://en.wikipedia.org/wiki/Glob_(programming)

​				Globbing works like wildcards, but uses ****** to match backslashes. (Using ***** or **?** will not 
​				match a backslash

​				uffs.com  `**\Users\**\AppData\**`



#### Regular-Expression Search (REGEX)

​				Regular expressions are implemented using the Boost.Xpressive library, using ESMAScript
​				syntax.

​				A good resource to LEARN REGEX: http://www.regular-expressions.info/tutorial.html

​				An online REGEX expression builder: https://regex101.com/

##### Some common regular expressions

​				**.**			     = A single character

​				**+**				= A plus symbol (backslash is the escape character)

​				**[a-cG-K]**	= A single character from a to c or from G to K\

​				**(abc|def)** = Either "abc" or "def"

##### "Quantifiers" can follow any expression

​				*				= Zero or more occurrences

​				**+**				= One or more occurrences

​				**{m,n}**		= Between m and n occurrences (n is optional)

##### Examples of regular expressions

					`uffs ">C:\\TemP.*\.txt"`

​			Finds all text files on C: in folder **temp** (by default the tool is NOT case sensitive) and its subdirectories

					`uffs ">.*\\DatA.*\d{3}-\d{2}-\d{4}.*" --drives=d,c`

​			Finds all files & directories, with a "social security number" in their names on drive C: and D: 
​			with  `data` string in their path

#### --case (NOT implemented yet)

​			Switch case sensitivity on or off

​					`uffs DuaLippa --case=on`

​					This will find all files with "DuaLippa" in its name ... but NOT "dualippa"

#### --drives

Specify which drives to search.

The tool will ONLY accept physical drives, which are currently accessible.

​				`uffs DuaLippa --drives=m,d,s`

​				`uffs DuaLippa --drives=C:,D:,s`

​				`uffs DuaLippa --drives=D,E`

#### --ext

Specify what EXTENTIONS / file types to search for

​				`uffs DuaLippa --drives=D,E --ext=mp3,jpg,`**videos**

| EXT Collections | extensions     |
| --------------- | -------------- |
| `pictures`      | jpg, png, tiff |
| `documents`     | doc, txt, pdf  |
| `videos`        | mpeg, mp4      |
| `music`         | mp3, wav,      |

### Output Options

#### DEFAULTS

| Option        | Default Value                                                | Type   |
| ------------- | ------------------------------------------------------------ | ------ |
| --columns=all | Will output **ALL** columns / attributes                     | string |
| --header=true | Will **include** a header as the first line                  | bool   |
| --neg=0       | Will be used to indicate an **INACTIVE** attribute            | string |
| --out=console | Output LOCATION. Can be filename or console(con,**console**,term,terminal) | string |
| --pos=1       | Will be used to indicate an **ACTIVE** attribute             | string |
| --quotes="    | NAMES and PATH need to be enclosed by **DOUBLE QUOTES**      | string |
| --sep=,       | Columns are SEPARTED by **comma** by default.                | string |

#### --out

The output of the tool can be saved to a file, put out on the console, or piped to another tool for further processing:

​				`uffs c:/Music** --out=bigfile.csv`

​				`uffs c:/Music** --out=console`

​															**console** can be: con, console, term, terminal

​				`uffs c:/Music** | grep dualippa | musicplayer`

#### --header

The header will be just for the columns you selected (see `--columns` switch)

​				`uffs c:/Music** --out=con --header=true`

​				Looks like: "Path","Name","Size"

#### --columns

Specify what information you like to get for each file record.

​				`uffs c:/Music** --out=gibfile.csv --header=true --columns=path,created,type`

| Columns Flags | Flag Description |
| ------------ | ---------------- |
|`all`| "All columns will be put out|
|`path`| File or Directory PATH + FILENAME |
|`name`| File or Directory NAME|
|`pathonly`| File or Directory PATH ONLY|
|`type`| File Type|
|`size`| Actual size of the file|
|`sizeondisk`| Space used on disk|
|`created`| The time the file was created|
|`written`| The time the file was last written or truncated|
|`accessed`| The time the file was last accessed|
|`decendents`| Number of files / dir starting from path including all subdirectories |
|`r`| Read - only file attribute|
|`a`| Archive file attribute|
|`s`| System file attribute|
|`h`| Hidden file attribute|
|`o`| Offline attribute|
|`notcontent`| Not content indexed file attribute|
|`noscrub`| No scrub file attribute|
|`integrity`| Integrity attribute|
|`pinned`| Pinned attribute|
|`unpinned`| Unpinned attribute|
|`directory`| Is a directory folder|
|`compressed`| Is compressed|
|`encrypted`| Is encrypted|
|`sparse`| Is sparse|
|`reparse`| Is a reparse point|
|`attributevalue`| Number representing the condensed file attributes|

#### --neg

Some attributes are BINARY and this option specifies what the output should be for MISSING attributes.

It can be BLANK, a single number or character, or it can even be a string (e.g. populate an HTML table with color coding)

​				`uffs c:/Music** --out=gibfile.csv --neg=0`

​				`uffs c:/Music** --out=gibfile.csv --neg=minus`

​				`uffs c:/Music** --neg=<span style="color: #ff0000;"> - </span>`

Usually you want to set `--neg` together with `--pos`

#### --pos

See `--neg` for details.

Setting the representation in the output for active file attributes.

​				`uffs c:/Music** --out=gibfile.csv --pos=+`

BTW, when you read the output file into a **python pandas dataframe**, these `--neg` / `--pos` allow you to see some statistics right away.

#### --quotes

The PATH, NAME, PATHONLY column output can sometimes contain very "strange" characters. E.g. a file name can contain a **,** and thus be taken for a column separator ...

With `--quotes` you can specify SINGLE or DOUBLE quotes or anything else for that purpose.

​				`uffs c:/Music** --out=gibfile.csv --quotes='`

#### --sep

This specifies the column separator. Can be anything.

​				`uffs c:/Music** --out=gibfile.csv --sep=;`

​				`uffs c:/Music** --out=gibfile.csv --sep=--^--`

​				`uffs c:/Music** --out=gibfile.csv --sep=`**tab**

| Special Character substitute | Special Character |
| ---------------------------- | ----------------- |
|**TAB**| `\t` |
|NEWLINE| `\n`|
|NEW LINE| `\n`|
|SPACE| ` `|
|RETURN| `\r`|
|DOUBLE| `"`|
|SINGLE| `'`|
|NULL| `\0`|

## Compile Your Version

The original source code was quite dense and challenging to understand, covering all aspects of the NTFS MFT, which contains numerous intricacies that must be addressed for proper functionality.

To improve the codebase, I have taken several steps. First, I cleaned up the source code, making it more readable and organized. Additionally, I added a Command Line Interface (CLI) wrapper around the tool, bringing it up to date with modern programming practices.

Initially, I considered rewriting the entire project from scratch and updating all tools to their latest versions. However, after further consideration, I chose to port the project to the Rust programming language, which offers benefits in performance and safety. This Rust port will be implemented in the future.

### Tool Chain

Setting up the toolchain to compile this code on your machine can be a bit challenging. Here are some steps to guide you through the process:

My current setup is based on VS2022

Add these components to your VS installation:

	Visual Studio INSTALLER:

		Visual Studio VS2022 

			"C++ Windows XP Support for VS 2017 (v141) tools [Deprecated]"
			"Microsoft.VisualStudio.Component.WinXP",
			"Microsoft.VisualStudio.Component.VC.v141.ATL"
			"Microsoft.VisualStudio.Component.Windows10SDK.19041",
			"Microsoft.VisualStudio.Component.VC.v141.x86.x64",

All tools are in the "Original Packages" folder
	
Download and install the WTL source (wtl-code-r636-trunk.zip) I put it @ "C:\uffs\wtl-code-r636-trunk"

Download BOOST (https://www.boost.org/users/history/version_1_73_0.html) extract and I put it here @ "C:\uffs\boost_1_73_0"

I am using LLVM support.lib (just in case you compiled your own version of LLVM):

	llvm-config --version ==>	11.0.0git

UNZIP llvm\lib\Debug\LLVMSupport.zip   ==> llvm\lib\Debug\LLVMSupport.lib

UNZIP llvm\lib\Release\LLVMSupport.zip ==> llvm\lib\Release\LLVMSupport.lib

EDIT Project.props in source directory:

	-----------------------------------------------------------------------------------------
	<?xml version="1.0" encoding="utf-8"?>
	<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
	  <ImportGroup Label="PropertySheets" />
	  <PropertyGroup Label="UserMacros" >
	  <BOOST_ROOT>---insert BOOST root location ---\</BOOST_ROOT>
		<XPDeprecationWarning>false</XPDeprecationWarning>
		<IncludePath>--- insert location of WTL library ---wtl\Include;$(IncludePath)</IncludePath>
	  </PropertyGroup>
	  
	-----------------------------------------------------------------------------------------

Once you have the tool-chain in place you should just be able to compile the code.

### Directory Tree of Files

<pre>
```

C:\UFFS
├───bin
│   └───x64
│       ├───COM
│       │       uffs.com
│       │       uffs.com.sha512
│       │
│       └───EXE
│               uffs.exe
│               uffs.exe.sha512
├───llvm
│   ├───build
│   │   └───include
│   ├───lib
│   │   ├───Debug
│   │   └───Release
│   └───llvm
│       └───include
│
├───Original Packages
│       boost_1_73_0.7z
│       llvm.zip
│       swiftsearch-code-4043bc-2023-03-24.zip
│       wtl-code-r636-trunk.zip
│
└───UltraFastFileSearch-code
        BackgroundWorker.hpp
        CDlgTemplate.hpp
        Clang-Compile.bat
        CModifiedDialogImpl.hpp
        CxxFrameHandler.asm
        file.cpp
        MUIConfig.xml
        nformat.hpp
        NtUserCallHook.hpp
        path.hpp
        Performance.psess
        Project.props
        resource.h
        Search Drive.ico
        ShellItemIDList.hpp
        stdafx.cpp
        stdafx.h
        string_matcher.cpp
        string_matcher.hpp
        targetver.h
        The Manual.md
        UltraFastFileSearch.aps
        UltraFastFileSearch.cpp
        UltraFastFileSearch.js
        UltraFastFileSearch.rc
        UltraFastFileSearch.sln
        UltraFastFileSearch.vcxproj
        UltraFastFileSearch.vcxproj.filters
        UltraFastFileSearch.vcxproj.user
        WinDDKFixes.hpp

```
</pre>

## Known Issues

Needs more work on REGEX.

## TODO

1. Modernize the MFT engine.
2. Speed Up the REGEX matching 
3. Make CASE sensitivity an option, rather than to just ignore CASE altogether
4. Make SORTING more customizable. Right now ranking is different for `File` and `file`
5. Return code for NO RESULT should be customizable. Right now the tool will only return 0 for normal execution. Would be nice to get a specifiable error code for the NO RESULT case.
6. Maybe add a DATABASE structure to catch all results and make that available to other tools via IPC.
7. With that it makes sense to have a trigger to update the search results periodically.
8. Symbolic Links are not always correctly followed.
9. Support multiple search location at the same time. E.g. all TXT files in `c:/data` and `d:/family`

## LICENSE
Original Work:

Title: SwiftSearch

Author: wfunction (https://sourceforge.net/u/wfunction/profile/)

Source: https://sourceforge.net/projects/swiftsearch/

License: Creative Commons Attribution Non-Commercial License 2.0 (https://creativecommons.org/licenses/by-nc/2.0/legalcode)

Modifications:
Adapted by Robert Nio on 2023-03-24
Changes: Added an industrial-strength CLI interface and cleaned up the source code for better readability.

Disclaimer:
The original licensor, wfunction, does not endorse this adaptation or its use. This adapted work is licensed under the same Creative Commons Attribution Non-Commercial License 2.0. (https://creativecommons.org/licenses/by-nc/2.0/)