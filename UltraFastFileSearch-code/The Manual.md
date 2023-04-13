# SwiftSearch (Robert Nio's version)

## [a.k.a Ultra Fast File Search ]

------
(Professional IT admin tool to find files on Windows disks lightning fast)

[TOC]



## TL;DR

### Why does the tool need elevated access:

**UFFS** and any other tools in this category require administrative privileges for low level read access to NTFS volumes. Windows manages this through it's **U**ser **A**ccess **C**ontrol (UAC).

Some people do not want this "nag" and work around it: https://sourceforge.net/p/swiftsearch/discussion/General/thread/12e048f1/#b2b4

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

NOTE: Reading the MTF needs ADMIN rights. On Windows it will trigger a UAC prompt indicating that the tool will want to read the MTF directly. 

Here are a few hashes of the **uttf.com** file

SHA-256

BLAKE-3



### General Information

The MAGIC behind this lightning fast search engine is reading the MTF (like a phonebook) of the disks.

Traditionally almost all file search tools / functions (e.g. os.walk in python) use this simple process:

1. Ask the OS to find a file (or next one).
2. The OS will go and get the MTF (the COMPLETE phonebook) and get the file info & attributes.
3. This info (NOT the MTF) is passed on to the tool/function.
4. Throw away the MTF (BTW. it has ALL the info for ALL the files and we just took ONE file info)
5. Repeat for the NEXT file.

I knew about this in principle, but never came across a tool that was able to do this as EFFICIENTLY as SwiftSearch (see below for more info). It can query all disks in parallel. Once a MTF is loaded ... it starts parsing the content to match your query ... this is in parallel to all the other processes too. ==> **LIGHTNING FAST !**

### Where is the source? Why BINARY only?

The creator of SwiftSearch (the core engine we use) asked me to just publish a binary version.

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


#### SwiftSearch
https://sourceforge.net/projects/swiftsearch/

Most advance use of MULTITHREADING / MULTITASKING of any of the file search tools out there.

It will start reading the MTF on all drives in parallel. Once a MTF is loaded it will start parsing it while every other process is still running in parallel.

There were a few areas to improve on the core design:

CLI parser is quite old / not up to modern standards

Even though all the attributes of each file record are read ... not all are passed to the user

The CLI parser is very particular on how the CLI parameters and switches are provided

#### SwiftSearch (Robert Nio's version) / [a.k.a Ultra Fast File Search ]

This version of the tool is geared to the IT professional.

It lends itself to automation and customizing the results in any shape or form.

The core engine is the one provided by SwiftSearch, a robust and VERY fast engine.

##### Main Differences

1. Modern and powerful CLI parser.
2. Search strings can be more windows / dos like and less complicated.
3. Understands pathnames in LINUX and WINDOWS formats.
4. Powerful output engine; enabling you to customize every aspect of it.
5. Speed differences between SwiftSearch and this version are due to the amount of data collected and matched

**==> The results are similar to SwiftSearch**

#### BENCHMARK

1 SSD / 4 HardDisks / total of 19 Million records

| Program     | # of Records    | Time used to load and sort | NOTE             |
| ----------- | --------------- | -------------------------- | ---------------- |
| **UFFS**    | **19 Million**  | **121 seconds**            | **all disks**    |
|             | **6.5 Million** | **56 seconds**             | **1 Hard Drive** |
| SwiftSearch | 19 Million      | 120 seconds                | all disks        |
|             | 6.5 Million     | 55 seconds                 | 1 Hard Drive     |
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

​				*****				= Zero or more occurrences

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

​					``uffs DuaLippa --case=on`

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
| **videos**      | mpeg, mp4      |
| music           | mp3, wav,      |

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

​				``uffs c:/Music** | grep dualippa | musicplayer`

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

## Known Issues

Needs more work on REGEX in the version based on the current source of SwiftSearch. SwiftSearch binaries do seem to have better REGEX processing.

## TODO

1. Modernize the MTF engine.

2. Speed Up the REGEX matching 
3. Make CASE sensitivity an option, rather than to just ignore CASE altogether
4. Make SORTING more customizable. Right now ranking is different for `File` and `file`
5. Return code for NO RESULT should be customizable. Right now the tool will only return 0 for normal execution. Would be nice to get a specifiable error code for the NO RESULT case.
6. Maybe add a DATABASE structure to catch all results and make that available to other tools via IPC.
7. With that it makes sense to have a trigger to update the search results periodically.
8. Symbolic Links are not always correctly followed.
9. Support multiple search location at the same time. E.g. all TXT files in `c:/data` and `d:/family`