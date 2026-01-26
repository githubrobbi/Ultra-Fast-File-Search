#pragma once

int main(int argc, char* argv[])
	{
		std::ostream& OS = std::cout;

		int new_argc = argc;
		char** new_argv = argv;

		// SET Demo mode with sample arguments
		// Create a new set of command-line arguments
		//char* new_argv_arr[] = { argv[0], ">^C:.*(\\.[a-zA-Z0-9]+)", "--columns=name", "--header=true", "--quotes="};

		char* new_argv_arr[] = { argv[0], "--help" };

		if (argc == 1)
		{
			new_argc = sizeof(new_argv_arr) / sizeof(new_argv_arr[0]);
			new_argv = new_argv_arr;
		}

		int result;

		std::string diskdrives = drivenames();
		replaceAll(diskdrives, "\\", ",");
		diskdrives.erase(diskdrives.find_last_of(",") - 1, std::string::npos);
		diskdrives = removeSpaces(diskdrives);

		std::string disks = drivenames();
		replaceAll(disks, "\\", "|");
		disks.erase(disks.find_last_of("|") - 1, std::string::npos);
		disks = removeSpaces(disks);

		////////////////////////////////////////////////////////////////////////////////////////////////
		// CLI11 Command Line Parsing (replaces LLVM CommandLine)
		////////////////////////////////////////////////////////////////////////////////////////////////
		CommandLineParser parser(diskdrives);
		int parseResult = parser.parse(new_argc, new_argv);
		if (parseResult != 0) {
			return parseResult;
		}

		const auto& opts = parser.options();

		// If help or version was requested, exit successfully
		if (opts.helpRequested || opts.versionRequested) {
			return 0;
		}

		// Map options to local variables for minimal code changes
		std::string searchPathCopy = opts.searchPath;
		bool header = opts.includeHeader;
		std::string OutputFilename = opts.outputFilename;
		std::string quotes = opts.quotes;
		std::string separator = opts.separator;
		std::string positive = opts.positiveMarker;
		std::string negative = opts.negativeMarker;
		uint32_t output_columns_flags = opts.columnFlags;
		bool columnsSpecified = opts.columnsSpecified;

		// Create a vector reference for drives and extensions
		const std::vector<std::string>& drives = opts.drives;
		const std::vector<std::string>& extentions = opts.extensions;

		OS << "\n";

		// Handle --dump-mft option (raw MFT dump in UFFS-MFT format)
		if (!opts.dumpMftDrive.empty()) {
			char drive_letter = opts.dumpMftDrive[0];
			if (!isalpha(drive_letter)) {
				OS << "ERROR: Invalid drive letter: " << opts.dumpMftDrive << "\n";
				return ERROR_BAD_ARGUMENTS;
			}
			return dump_raw_mft(drive_letter, opts.dumpMftOutput.c_str(), OS);
		}

		// Handle --dump-extents option (MFT extent diagnostic tool)
		if (!opts.dumpExtentsDrive.empty()) {
			char drive_letter = opts.dumpExtentsDrive[0];
			if (!isalpha(drive_letter)) {
				OS << "ERROR: Invalid drive letter: " << opts.dumpExtentsDrive << "\n";
				return ERROR_BAD_ARGUMENTS;
			}
			return dump_mft_extents(drive_letter, opts.dumpExtentsOutput.c_str(), opts.verifyExtents, OS);
		}

		// Handle --benchmark-mft option (MFT read speed benchmark)
		if (!opts.benchmarkMftDrive.empty()) {
			char drive_letter = opts.benchmarkMftDrive[0];
			if (!isalpha(drive_letter)) {
				OS << "ERROR: Invalid drive letter: " << opts.benchmarkMftDrive << "\n";
				return ERROR_BAD_ARGUMENTS;
			}
			return benchmark_mft_read(drive_letter, OS);
		}

		// Handle --benchmark-index option (full index build benchmark)
		if (!opts.benchmarkIndexDrive.empty()) {
			char drive_letter = opts.benchmarkIndexDrive[0];
			if (!isalpha(drive_letter)) {
				OS << "ERROR: Invalid drive letter: " << opts.benchmarkIndexDrive << "\n";
				return ERROR_BAD_ARGUMENTS;
			}
			return benchmark_index_build(drive_letter, OS);
		}

		HANDLE outHandle = 0;

		// Handle output filename defaults
		if (OutputFilename.empty() || OutputFilename == "console") {
			OutputFilename = "console";
		} else if (OutputFilename == "f") {
			OutputFilename = "uffs.csv";
		}

		static
			const bool console = (OutputFilename == "console" || OutputFilename == "con" || OutputFilename == "terminal" || OutputFilename == "term");

		SetLastError(0);

		if ((opts.outputSpecified && !console) || (!columnsSpecified))	//&OutputFilename.ValueStr == true)
		{
			outHandle = CreateFileA((LPCSTR)OutputFilename.c_str(),	//argv[1],    	// name of the write	//(L"uffs.csv")
				GENERIC_WRITE,	// open for writing
				FILE_SHARE_READ,	//0,                                                     	// do not share
				nullptr,	// default security
				CREATE_ALWAYS,	// create new file only
				FILE_ATTRIBUTE_NORMAL,	// normal file
				nullptr);
		};

		int err = GetLastError();

		if (err != 0 && err != ERROR_ALREADY_EXISTS)
		{
			OS << "Output File ERROR: (" << err << ")\t" << GetLastErrorAsString() << "\n";
			return err;
		};

		// searchPathCopy already defined above from opts.searchPath

		std::tvstring laufwerke;
		std::tvstring tvtemp;
		std::wstring laufwerkeW;
		static int gotdrives = 0;
		laufwerke = L"";

		std::filesystem::path tempath;
		static char driveletter = '\0';
		std::string searchsubstring = "";
		if (!searchPathCopy.empty()) {
			searchPathCopy[0] == '>' ? searchsubstring = searchPathCopy.substr(1, 4) : searchsubstring = searchPathCopy.substr(0, 3);
		}
		boost::to_upper(searchsubstring);
		static char s1 = searchsubstring.size() > 0 ? searchsubstring[0] : '\0';
		static char s2 = searchsubstring.size() > 1 ? searchsubstring[1] : '\0';
		static char s3 = searchsubstring.size() > 2 ? searchsubstring[2] : '\0';
		static char searchdrive = '\0';
		static bool drivematch = false;
		if (s1 == '>' && s3 == ':') searchdrive = s2;
		else if (s2 == ':') searchdrive = s1;
		static std::string searchdrivestr;
		if (searchdrive != '\0')
		{
			searchdrivestr = searchdrive;
			searchdrivestr += ":";
			gotdrives = 1;
		}

		//OS << "searchPathCopy =" << searchPathCopy << "\n\n";

		tempath = searchdrivestr;
		tvtemp = tempath.c_str();
		laufwerke += tvtemp;

		// Create a mutable copy of drives for processing
		std::vector<std::string> drivesCopy = drives;

		if (!drivesCopy.empty() && searchdrive == '\0')
		{
			// Check if any drive is "*"
			bool hasWildcard = false;
			for (const auto& d : drivesCopy) {
				if (d.find('*') != std::string::npos) {
					hasWildcard = true;
					break;
				}
			}

			if (!hasWildcard)
			{
				for (size_t i = 0, e = drivesCopy.size(); i != e; ++i)
				{
					replaceAll(drivesCopy[i], "\\", "");
					replaceAll(drivesCopy[i], ":", "");
					replaceAll(drivesCopy[i], "|", "");
					boost::to_upper(drivesCopy[i]);
					driveletter = drivesCopy[i][0];
					if (diskdrives.find(driveletter) != std::tvstring::npos)
					{
						if (i < (e - 1))
							drivesCopy[i] += ":|";
						else
							drivesCopy[i] += ":";

						tempath = drivesCopy[i];
						tvtemp = tempath.c_str();
						laufwerke += tvtemp;
						gotdrives += 1;
					}
					else
					{
						OS << "\n\n";
						OS << "\tInvalid DRIVE LETTER:\t" + drivesCopy[i];
						OS << "\n\n";
						exit(-13);
					}

					//OS << drivesCopy[i];
				}

				//OS << '\n';
			}
			else
				laufwerke = L"*";
		}
		else if (searchdrive == '\0') laufwerke = L"*";

		static
			const std::string
			extopen = "(",
			extsep = "|",
			extclose = ")",
			caseoff = "(?i)",
			caseon = "(?-i)",
			escdot = "\\.";

		static std::string endung = "", exten = "";
		static std::string tempathstr = "";

		// Normalize / correct any path string
		if (!searchPathCopy.empty() && searchPathCopy[0] != '>')
		{
			// check if we need to UPPER the first letter
			if (searchPathCopy.size() > 1 && searchPathCopy[1] == ':')
			{
				//gotdrives += 1;
				searchPathCopy[0] = char(toupper(searchPathCopy[0]));
				//laufwerke = s2ws(searchPathCopy.substr(0, 2)).c_str();
				//tempathstr = searchPathCopy.substr(2, searchPathCopy.size());
				//searchPathCopy.setValue(tempathstr);
			}
			else
			{
				if (drivesCopy.size() == 1) searchPathCopy = std::string(laufwerke.begin(), laufwerke.end()) + searchPathCopy;
			}

			tempath = searchPathCopy;

			// do we have an EXTENTION?
			if (tempath.has_extension())
			{
				exten = tempath.extension().generic_string();
			};

			if (!extentions.empty())
			{
				exten.empty() ? endung = extopen : endung = extopen + +"\\" + exten + extsep;

				for (size_t i = 0, e = extentions.size(); i != e; ++i)
				{
					std::string neueendung = escdot + extentions[i];
					if (extentions[i] == "pictures")
						neueendung = "\\.jpg|\\.png|\\.tiff";

					else if (extentions[i] == "documents")
						neueendung = "\\.doc|\\.txt|\\.pdf";

					else if (extentions[i] == "videos")
						neueendung = "\\.mpeg|\\.mp4";

					else if (extentions[i] == "music")
						neueendung = "\\.mp3|\\.wav";

					endung = endung + neueendung + extsep;
				};

				if (endung[endung.size() - 1] == '|') endung.pop_back();

				endung += extclose;
			};

			if (tempath.has_extension())
			{
				tempath.replace_extension("");
			};

			tempathstr = tempath.u8string();
			static size_t last = tempathstr.size() - 1;
			static std::string tempathstrsubstring = "";
			static int testers = 0;
			// CASE **
			tempathstrsubstring = tempathstr.substr(0, last - 1);

			if (tempathstr[last] == '*' && tempathstr[last - 1] != '\\' && tempathstrsubstring.find('*') != 0 || !endung.empty())
			{ /*if (casepat)
					tempathstr = caseon + tempathstr;
				else
					tempathstr = caseoff + tempathstr; */

				tempathstr = ">" + tempathstr;
				tempathstr.pop_back();	//remove "*"

				replaceAll(tempathstr, "\\", "\\\\");

				if (!endung.empty())
				{
					tempathstr += "(.*" + endung + ")";
				}
				else
					tempathstr += ".*" + exten;

				searchPathCopy = tempathstr;
			}
			else

				if (exten.empty())
					searchPathCopy = tempathstr;
				else
				{
					tempathstr.pop_back();
					tempathstr += "*" + exten;
					searchPathCopy = tempathstr;
				};

		}

		//OS << "searchPathCopy befor processing \t" << searchPathCopy;

		bool changed = false;

		std::string originalsep = separator;
		std::string t = separator;
		std::transform(t.begin(), t.end(), t.begin(), [](char c) { return static_cast<char>(::toupper(static_cast<unsigned char>(c))); });
		separator = t;

		if (separator == "TAB")
		{
			separator = "\t";
			changed = true;
		}

		if (separator == "NEWLINE")
		{
			separator = "\n";
			changed = true;
		}

		if (separator == "NEW LINE")
		{
			separator = "\n";
			changed = true;
		}

		if (separator == "SPACE")
		{
			separator = " ";
			changed = true;
		}

		if (separator == "RETURN")
		{
			separator = "\r";
			changed = true;
		}

		if (separator == "DOUBLE")
		{
			separator = "\"";
			changed = true;
		}

		if (separator == "SINGLE")
		{
			separator = "\'";
			changed = true;
		}

		if (separator == "NULL")
		{
			separator = std::string(1, '\0');
			changed = true;
		}

		if (!changed) separator = originalsep;

		// << "\n Done PROCESSING arguments \n";

		int
			const
			stderr_fd = _fileno(stderr),
			stdout_fd = _fileno(stdout);

		(void)stderr_fd;
		(void)stdout_fd;
		result = 0;

		try
		{
			static time_t
				const tbegin                    = clock();
			static time_t tend1                 = clock();
			static time_t lap                   = clock();
			static time_t lapmatch              = clock();
			static unsigned int timelapsed1;
			static std::tvstring root_path1;
			static std::string rootstr;
			static bool firstround              = true;
			static bool firstroundmatch         = true;
			static bool firstroundmatchfinished = true;

			long long
				const time_zone_bias = get_time_zone_bias();
			LCID
				const lcid = GetThreadLocale();
			NFormat nformat_io((std::locale()));
			NFormat
				const& nformat = nformat_io;
			MatchOperation matchop;
			//OS << "\n\nSEARCH pattern passed to MATCHER: \t" << searchPathCopy;
			if (gotdrives > 0) OS << "\nDrives? \t" << gotdrives << "\t" << std::string(laufwerke.begin(), laufwerke.end());
			OS << "\n\n";

			// FIRST argument (check for regex etc.)
			matchop.init(converter.from_bytes(searchPathCopy));
			//matchop.init(L">C:\\TemP.*\.txt");

			IoCompletionPort iocp;
			std::vector<intrusive_ptr < NtfsIndex>> indices;

			static std::tvstring sep;
			sep = converter.from_bytes(separator).c_str();

			static std::string sepstr;
			sepstr = separator.c_str();

			static std::tvstring pos;
			pos = converter.from_bytes(positive).c_str();

			static std::tvstring neg;
			neg = converter.from_bytes(negative).c_str();

			static std::tvstring quote;
			quote = converter.from_bytes(quotes).c_str();

			static std::string quotestr;
			quotestr = quotes.c_str();

			static std::tvstring empty;
			empty = L"";

			{

				// Which DRIVES are used
				std::vector<std::tvstring > path_names;
				if (gotdrives)
				{
					//std::tvstring arg(argv[2]);
					std::tvstring arg(laufwerke);
					for (size_t i = 0;; ++i)
					{
						size_t
							const j = arg.find(_T('|'), i);
						std::tvstring path_name(arg.data() + static_cast<ptrdiff_t> (i), (~j ? j : arg.size()) - i);
						adddirsep(path_name);
						path_names.push_back(path_name);
						if (!~j)
						{
							break;
						}

						i = j;
					}
				}
				else
				{
					get_volume_path_names().swap(path_names);
				}

				// Fill the queue with all the MFT to read
				for (const auto& path_name : path_names)
				{
					if (matchop.prematch(path_name))
					{
						indices.push_back(static_cast<intrusive_ptr<NtfsIndex>>(new NtfsIndex(path_name)));
					}
				}
			}

			bool
				const match_attributes = false;
			std::vector<IoPriority> set_priorities(indices.size());
			Handle closing_event;
			std::vector<size_t> pending;
			for (size_t i = 0; i != indices.size(); ++i)
			{
				if (void* const volume = indices[i]->volume())
				{
					IoPriority(reinterpret_cast<uintptr_t> (volume), winnt::IoPriorityLow).swap(set_priorities[i]);
				}

				typedef OverlappedNtfsMftReadPayload T;
				intrusive_ptr<T> p(new T(iocp, indices[i], closing_event));
				iocp.post(0, static_cast<uintptr_t> (i), p);
				pending.push_back(i);
			}

			while (!pending.empty())
			{ /*
				if (firstround)
				{
					OS << "Start \t\treading all the MTF on selected drives: " << std::string(laufwerke.begin(), laufwerke.end()) << "\n\n";
				};
	 */

				HANDLE wait_handles[MAXIMUM_WAIT_OBJECTS];
				unsigned int nwait_handles = 0;
				intrusive_ptr<NtfsIndex> i;
				{
					IoPriority
						const raise_first_priority(*pending.begin(), set_priorities[*pending.begin()].old());
					for (nwait_handles = 0; nwait_handles != pending.size() && nwait_handles != sizeof(wait_handles) / sizeof(*wait_handles); ++nwait_handles)
					{
						wait_handles[nwait_handles] = reinterpret_cast<HANDLE> (indices[pending[nwait_handles]]->finished_event());
					}

					DWORD
						const wait_result = WaitForMultipleObjects(nwait_handles, wait_handles, FALSE, INFINITE);
					CheckAndThrow(wait_result != WAIT_FAILED /*this is the only case in which we call GetLastError */);
					if (wait_result >= WAIT_ABANDONED_0)
					{
						CppRaiseException(WAIT_ABANDONED);
					}

					if (wait_result < WAIT_OBJECT_0 + nwait_handles)
					{
						i = indices[pending[wait_result - WAIT_OBJECT_0]];
						pending.erase(pending.begin() + static_cast<ptrdiff_t> (wait_result - WAIT_OBJECT_0));
					}
				}

				/*
				tend1 = clock();
				!firstround ? lap = tend1 : lap = tbegin;
				root_path1 = i->root_path();
				rootstr = std::string(root_path1.begin(), root_path1.end());
				timelapsed1 = static_cast< unsigned int>((tend1 - tbegin) / CLOCKS_PER_SEC);
				OS << "Finished \tReading the MTF of " << rootstr << " in " << timelapsed1 << " seconds !\n\n" ;
				lap = tend1; firstround = false; */

				if (i)	// results of scan ... one at a time
				{
					std::tvstring
						const root_path = i->root_path();
					std::tvstring current_path = matchop.get_current_path(root_path);

					//tend1 = clock();
					//!firstroundmatch ? lapmatch = tend1 : lapmatch = lap;
					//lapmatch = tend1; firstroundmatch = false;
					//root_path1 = i->root_path();
					//rootstr = std::string(root_path.begin(), root_path.end());
					//timelapsed1 = static_cast< unsigned int>((tend1 - lapmatch) / CLOCKS_PER_SEC);
					//OS << "Start \t\tMATCHING on " << rootstr << " Drive.\n\n";

					class Writer
					{
						HANDLE output;
						bool output_isatty;
						bool output_isdevnull;
						size_t max_buffer_size;
						unsigned long long second_fraction_denominator /*NOTE: Too large a value is useless with clock(); use a more precise timer if needed */;
						clock_t tprev;
						unsigned long long nwritten_since_update;
						std::string line_buffer_utf8;

					public:
						explicit Writer(int
							const output, size_t
							const max_buffer_size = 0) : output(reinterpret_cast<HANDLE> (_get_osfhandle(output))), output_isatty(!!isatty(output)), output_isdevnull(isdevnull(output)), max_buffer_size(max_buffer_size), second_fraction_denominator(1 << 11), tprev(clock()), nwritten_since_update() {}

						void operator()(std::tvstring& line_buffer, bool
							const force = false, HANDLE* outHandle = nullptr, std::ofstream* outputfile = nullptr)
						{
							if (!output_isdevnull)
							{
								if (!console) output = *outHandle;

								clock_t
									const tnow = clock();
								unsigned long nwritten;
								if (force || line_buffer.size() >= max_buffer_size || (tnow - tprev) * 1000 * second_fraction_denominator >= CLOCKS_PER_SEC)
								{
									if ((!output_isatty || (!WriteConsole(output, line_buffer.data(), static_cast<unsigned int> (line_buffer.size()), &nwritten, nullptr) && GetLastError() == ERROR_INVALID_HANDLE)))
									{
#if defined(_UNICODE) &&_UNICODE
											using std::max;
										line_buffer_utf8.resize(max(line_buffer_utf8.size(), (line_buffer.size() + 1) * 6), _T('\0'));
										if (!line_buffer.empty() && !line_buffer_utf8.empty())
										{
											int
												const cch = WideCharToMultiByte(CP_UTF8, 0, line_buffer.data(), static_cast<int> (line_buffer.size()), &line_buffer_utf8[0], static_cast<int> (line_buffer_utf8.size()), nullptr, nullptr);
											if (cch > 0)
											{
												nwritten_since_update += WriteFile(output, line_buffer_utf8.data(), sizeof(*line_buffer_utf8.data()) * static_cast<size_t> (cch), &nwritten, nullptr);
											}
										}
#else
											nwritten_since_update += WriteFile(output, line_buffer.data(), sizeof(*line_buffer.data()) * line_buffer.size(), & nwritten, nullptr);
#endif
									}

									line_buffer.clear();
									tprev = clock();
								}
							}
							else
							{
								line_buffer.clear();
							}
						}
					}

					flush_if_needed(stdout_fd, 1 << 15);

					std::tvstring line_buffer;
					std::string line_buffer_utf8;
					/*std::tvstring tmpcreated;
					std::tvstring tmpwritten;
					std::tvstring tmpaccessed;
					std::string tempstr;
					std::tvstring tvtempstr;
					std::stringstream ss; */

					static std::tvstring PathN, NameN, PathonlyN, SizeN, SizeondiskN, CreatedN, writtenN, AccessedN, DescendantsN,
						ReadonlyN, ArchiveN, SystemN, HiddenN, OfflineN, NotcontentN, NoscrubN, IntegrityN, PinnedN, UnpinnedN,
						DirectoryN, CompressedN, EncryptedN, SparseN, ReparseN, AttributesN, NewLine;

					PathN        = L"Path";
					NameN        = L"Name";
					PathonlyN    = L"Path Only";
					SizeN        = L"Size";
					SizeondiskN  = L"Size on Disk";
					CreatedN     = L"Created";
					writtenN     = L"Last Written";
					AccessedN    = L"Last Accessed";
					DescendantsN = L"Descendants";
					ReadonlyN    = L"Read-only";
					ArchiveN     = L"Archive";
					SystemN      = L"System";
					HiddenN      = L"Hidden";
					OfflineN     = L"Offline";
					NotcontentN  = L"Not content indexed file";
					NoscrubN     = L"No scrub file";
					IntegrityN   = L"Integrity";
					PinnedN      = L"Pinned";
					UnpinnedN    = L"Unpinned";
					DirectoryN   = L"Directory Flag";
					CompressedN  = L"Compressed";
					EncryptedN   = L"Encrypted";
					SparseN      = L"Sparse";
					ReparseN     = L"Reparse";
					AttributesN  = L"Attributes";
					NewLine      = L"\n";

					i->matches([&](TCHAR
						const* const name2, size_t
						const name_length, bool
						const ascii, NtfsIndex::key_type
						const& key, size_t
						const depth)
						/*TODO: Factor out common code from here and GUI-based version! */
						{
							size_t high_water_mark = 0, * phigh_water_mark = matchop.is_path_pattern ? &high_water_mark : nullptr;
							TCHAR
								const* const path_begin = name2;
							bool
								const match = ascii ?
								matchop.matcher.is_match(static_cast<char
									const*> (static_cast<void
										const*> (path_begin)), name_length, phigh_water_mark) :
								matchop.matcher.is_match(path_begin, name_length, phigh_water_mark);
							if (match)
							{
								if ((output_columns_flags & COL_ALL) || (!columnsSpecified))
								{
									if (header)
									{
										line_buffer += quote + PathN        + quote + sep;
										line_buffer += quote + NameN        + quote + sep;
										line_buffer += quote + PathonlyN    + quote + sep;
										line_buffer += quote + SizeN        + quote + sep;
										line_buffer += quote + SizeondiskN  + quote + sep;
										line_buffer += quote + CreatedN     + quote + sep;
										line_buffer += quote + writtenN     + quote + sep;
										line_buffer += quote + AccessedN    + quote + sep;
										line_buffer += quote + DescendantsN + quote + sep;
										line_buffer += quote + ReadonlyN    + quote + sep;
										line_buffer += quote + ArchiveN     + quote + sep;
										line_buffer += quote + SystemN      + quote + sep;
										line_buffer += quote + HiddenN      + quote + sep;
										line_buffer += quote + OfflineN     + quote + sep;
										line_buffer += quote + NotcontentN  + quote + sep;
										line_buffer += quote + NoscrubN     + quote + sep;
										line_buffer += quote + IntegrityN   + quote + sep;
										line_buffer += quote + PinnedN      + quote + sep;
										line_buffer += quote + UnpinnedN    + quote + sep;
										line_buffer += quote + DirectoryN   + quote + sep;
										line_buffer += quote + CompressedN  + quote + sep;
										line_buffer += quote + EncryptedN   + quote + sep;
										line_buffer += quote + SparseN      + quote + sep;
										line_buffer += quote + ReparseN     + quote + sep;
										line_buffer += quote + AttributesN  + quote + NewLine + NewLine;

										flush_if_needed(line_buffer, false, &outHandle);

										header = false;
									}

									std::tvstring pathstr, namestr, pathonlystr, temp;
									i->get_path(key, temp, false);
									std::filesystem::path itempath = temp.c_str();

									pathstr = itempath.c_str();
									namestr = itempath.filename().c_str();
									pathonlystr = pathstr;
									pathonlystr.resize(pathstr.size() - namestr.size());

									line_buffer += quote + root_path;
									line_buffer += pathstr;
									line_buffer += quote + sep;	//path

									line_buffer += quote;
									line_buffer += namestr;
									line_buffer += quote + sep;	//name

									line_buffer += quote + root_path;
									line_buffer += pathonlystr;
									line_buffer += quote + sep;	//path ONLY

									NtfsIndex::size_info
										const& sizeinfo = i->get_sizes(key);
									line_buffer += nformat(sizeinfo.length);
									line_buffer += sep;
									line_buffer += nformat(sizeinfo.allocated);
									line_buffer += sep;

									NtfsIndex::standard_info
										const& stdinfo = i->get_stdinfo(key.frs());
									/* File attribute abbreviations: https://en.wikipedia.org/wiki/File_attribute#Types */
									SystemTimeToString(stdinfo.created, line_buffer, true, true, time_zone_bias, lcid);
									line_buffer += sep;
									SystemTimeToString(stdinfo.written, line_buffer, true, true, time_zone_bias, lcid);
									line_buffer += sep;
									SystemTimeToString(stdinfo.accessed, line_buffer, true, true, time_zone_bias, lcid);
									line_buffer += sep;

									line_buffer += nformat(static_cast<unsigned int> (sizeinfo.treesize));
									line_buffer += sep;

									(stdinfo.is_readonly        > 0)          ? line_buffer += pos : line_buffer += neg;
									line_buffer += sep;
									(stdinfo.is_archive         > 0)          ? line_buffer += pos : line_buffer += neg;
									line_buffer += sep;
									(stdinfo.is_system          > 0)          ? line_buffer += pos : line_buffer += neg;
									line_buffer += sep;
									(stdinfo.is_hidden          > 0)          ? line_buffer += pos : line_buffer += neg;
									line_buffer += sep;
									(stdinfo.is_offline         > 0)          ? line_buffer += pos : line_buffer += neg;
									line_buffer += sep;
									(stdinfo.is_notcontentidx   > 0)          ? line_buffer += pos : line_buffer += neg;
									line_buffer += sep;
									(stdinfo.is_noscrubdata     > 0)          ? line_buffer += pos : line_buffer += neg;
									line_buffer += sep;
									(stdinfo.is_integretystream > 0)          ? line_buffer += pos : line_buffer += neg;
									line_buffer += sep;
									(stdinfo.is_pinned          > 0)          ? line_buffer += pos : line_buffer += neg;
									line_buffer += sep;
									(stdinfo.is_unpinned        > 0)          ? line_buffer += pos : line_buffer += neg;
									line_buffer += sep;
									(stdinfo.is_directory       > 0)          ? line_buffer += pos : line_buffer += neg;
									line_buffer += sep;
									(stdinfo.is_compressed      > 0)          ? line_buffer += pos : line_buffer += neg;
									line_buffer += sep;
									(stdinfo.is_encrypted       > 0)          ? line_buffer += pos : line_buffer += neg;
									line_buffer += sep;
									(stdinfo.is_sparsefile      > 0)          ? line_buffer += pos : line_buffer += neg;
									line_buffer += sep;
									(stdinfo.is_reparsepoint    > 0)          ? line_buffer += pos : line_buffer += neg;
									line_buffer += sep;
									line_buffer += nformat(stdinfo.attributes());

									line_buffer += NewLine;

									flush_if_needed(line_buffer, false, &outHandle);
								}
								else	// only SELECTED columns
								{
									if (header)
									{
										if (output_columns_flags & COL_PATH)
										{
											line_buffer += quote + PathN        + quote + sep;
										}

										if (output_columns_flags & COL_NAME)
										{
											line_buffer += quote + NameN        + quote + sep;
										}

										if (output_columns_flags & COL_PATHONLY)
										{
											line_buffer += quote + PathonlyN    + quote + sep;
										}

										if (output_columns_flags & COL_SIZE)
										{
											line_buffer += quote + SizeN        + quote + sep;
										}

										if (output_columns_flags & COL_SIZEONDISK)
										{
											line_buffer += quote + SizeondiskN  + quote + sep;
										}

										if (output_columns_flags & COL_CREATED)
										{
											line_buffer += quote + CreatedN     + quote + sep;
										}

										if (output_columns_flags & COL_WRITTEN)
										{
											line_buffer += quote + writtenN     + quote + sep;
										}

										if (output_columns_flags & COL_ACCESSED)
										{
											line_buffer += quote + AccessedN    + quote + sep;
										}

										if (output_columns_flags & COL_DECENDENTS)
										{
											line_buffer += quote + DescendantsN + quote + sep;
										}

										if (output_columns_flags & COL_R)
										{
											line_buffer += quote + ReadonlyN    + quote + sep;
										}

										if (output_columns_flags & COL_A)
										{
											line_buffer += quote + ArchiveN     + quote + sep;
										}

										if (output_columns_flags & COL_S)
										{
											line_buffer += quote + SystemN      + quote + sep;
										}

										if (output_columns_flags & COL_H)
										{
											line_buffer += quote + HiddenN      + quote + sep;
										}

										if (output_columns_flags & COL_O)
										{
											line_buffer += quote + OfflineN     + quote + sep;
										}

										if (output_columns_flags & COL_NOTCONTENT)
										{
											line_buffer += quote + NotcontentN  + quote + sep;
										}

										if (output_columns_flags & COL_NOSCRUB)
										{
											line_buffer += quote + NoscrubN     + quote + sep;
										}

										if (output_columns_flags & COL_INTEGRITY)
										{
											line_buffer += quote + IntegrityN   + quote + sep;
										}

										if (output_columns_flags & COL_PINNED)
										{
											line_buffer += quote + PinnedN      + quote + sep;
										}

										if (output_columns_flags & COL_UNPINNED)
										{
											line_buffer += quote + UnpinnedN    + quote + sep;
										}

										if (output_columns_flags & COL_DIRECTORY)
										{
											line_buffer += quote + DirectoryN   + quote + sep;
										}

										if (output_columns_flags & COL_COMPRESSED)
										{
											line_buffer += quote + CompressedN  + quote + sep;
										}

										if (output_columns_flags & COL_ENCRYPTED)
										{
											line_buffer += quote + EncryptedN   + quote + sep;
										}

										if (output_columns_flags & COL_SPARSE)
										{
											line_buffer += quote + SparseN      + quote + sep;
										}

										if (output_columns_flags & COL_REPARSE)
										{
											line_buffer += quote + ReparseN     + quote + sep;
										}

										if (output_columns_flags & COL_ATTRVALUE)
										{
											line_buffer += quote + AttributesN  + quote + sep;
										}

										line_buffer.pop_back();
										line_buffer += NewLine + NewLine;

										flush_if_needed(line_buffer, false, &outHandle);

										header = false;
									}

									std::tvstring pathstr, namestr, pathonlystr, temp;
									i->get_path(key, temp, false);
									std::filesystem::path itempath = temp.c_str();

									pathstr = itempath.c_str();
									namestr = itempath.filename().c_str();
									pathonlystr = pathstr;
									pathonlystr.resize(pathstr.size() - namestr.size());

									if (output_columns_flags & COL_PATH)
									{
										line_buffer += quote + root_path;
										line_buffer += pathstr;
										line_buffer += quote + sep;
									}	//path

									if (output_columns_flags & COL_NAME)
									{
										line_buffer += quote;
										line_buffer += namestr;
										line_buffer += quote + sep;
									}	//name

									if (output_columns_flags & COL_PATHONLY)
									{
										line_buffer += quote + root_path;
										line_buffer += pathonlystr;
										line_buffer += quote + sep;
									}	//path only

									NtfsIndex::size_info
										const& sizeinfo = i->get_sizes(key);
									if (output_columns_flags & COL_SIZE)
									{
										line_buffer += nformat(sizeinfo.length);
										line_buffer += sep;
									}

									if (output_columns_flags & COL_SIZEONDISK)
									{
										line_buffer += nformat(sizeinfo.allocated);
										line_buffer += sep;
									}

									NtfsIndex::standard_info
										const& stdinfo = i->get_stdinfo(key.frs());
									/* File attribute abbreviations: https://en.wikipedia.org/wiki/File_attribute#Types */
									if (output_columns_flags & COL_CREATED)
									{
										SystemTimeToString(stdinfo.created, line_buffer, true, true, time_zone_bias, lcid);
										line_buffer += sep;
									}

									if (output_columns_flags & COL_WRITTEN)
									{
										SystemTimeToString(stdinfo.written, line_buffer, true, true, time_zone_bias, lcid);
										line_buffer += sep;
									}

									if (output_columns_flags & COL_ACCESSED)
									{
										SystemTimeToString(stdinfo.accessed, line_buffer, true, true, time_zone_bias, lcid);
										line_buffer += sep;
									}

									if (output_columns_flags & COL_DECENDENTS)
									{
										line_buffer += nformat(static_cast<unsigned int> (sizeinfo.treesize));
										line_buffer += sep;
									}

									if (output_columns_flags & COL_R)
									{
										(stdinfo.is_readonly        > 0) ? line_buffer += pos : line_buffer += neg;
										line_buffer += sep;
									}

									if (output_columns_flags & COL_A)
									{
										(stdinfo.is_archive         > 0) ? line_buffer += pos : line_buffer += neg;
										line_buffer += sep;
									}

									if (output_columns_flags & COL_S)
									{
										(stdinfo.is_system          > 0) ? line_buffer += pos : line_buffer += neg;
										line_buffer += sep;
									}

									if (output_columns_flags & COL_H)
									{
										(stdinfo.is_hidden          > 0) ? line_buffer += pos : line_buffer += neg;
										line_buffer += sep;
									}

									if (output_columns_flags & COL_O)
									{
										(stdinfo.is_offline         > 0) ? line_buffer += pos : line_buffer += neg;
										line_buffer += sep;
									}

									if (output_columns_flags & COL_NOTCONTENT)
									{
										(stdinfo.is_notcontentidx   > 0) ? line_buffer += pos : line_buffer += neg;
										line_buffer += sep;
									}

									if (output_columns_flags & COL_NOSCRUB)
									{
										(stdinfo.is_noscrubdata     > 0) ? line_buffer += pos : line_buffer += neg;
										line_buffer += sep;
									}

									if (output_columns_flags & COL_INTEGRITY)
									{
										(stdinfo.is_integretystream > 0) ? line_buffer += pos : line_buffer += neg;
										line_buffer += sep;
									}

									if (output_columns_flags & COL_PINNED)
									{
										(stdinfo.is_pinned          > 0) ? line_buffer += pos : line_buffer += neg;
										line_buffer += sep;
									}

									if (output_columns_flags & COL_UNPINNED)
									{
										(stdinfo.is_unpinned        > 0) ? line_buffer += pos : line_buffer += neg;
										line_buffer += sep;
									}

									if (output_columns_flags & COL_DIRECTORY)
									{
										(stdinfo.is_directory       > 0) ? line_buffer += pos : line_buffer += neg;
										line_buffer += sep;
									}

									if (output_columns_flags & COL_COMPRESSED)
									{
										(stdinfo.is_compressed      > 0) ? line_buffer += pos : line_buffer += neg;
										line_buffer += sep;
									}

									if (output_columns_flags & COL_ENCRYPTED)
									{
										(stdinfo.is_encrypted       > 0) ? line_buffer += pos : line_buffer += neg;
										line_buffer += sep;
									}

									if (output_columns_flags & COL_SPARSE)
									{
										(stdinfo.is_sparsefile      > 0) ? line_buffer += pos : line_buffer += neg;
										line_buffer += sep;
									}

									if (output_columns_flags & COL_REPARSE)
									{
										(stdinfo.is_reparsepoint    > 0) ? line_buffer += pos : line_buffer += neg;
										line_buffer += sep;
									}

									if (output_columns_flags & COL_ATTRVALUE)
									{
										line_buffer += nformat(stdinfo.attributes());
										line_buffer += sep;
									}

									line_buffer.pop_back();
									line_buffer += NewLine;
									flush_if_needed(line_buffer, false, &outHandle);
								}	// else case of ALL check

							}

							return match || !(matchop.is_path_pattern && phigh_water_mark && *phigh_water_mark < name_length);
						}, current_path, matchop.is_path_pattern, matchop.is_stream_pattern, match_attributes);

					flush_if_needed(line_buffer, true, &outHandle);
				}	// Any results (i)

				/*tend1 = clock();
				!firstroundmatchfinished ? lapmatch = tend1 : lapmatch = tbegin;
				timelapsed1 = static_cast< unsigned int>((tend1 - lapmatch) / CLOCKS_PER_SEC);
				OS << "Finished \tMATCHING on " << rootstr << " in " << timelapsed1 << " seconds !\n\n";
				lapmatch = tend1; firstroundmatchfinished = false; */

			}	// While pending

			time_t
				const tend = clock();
			const static unsigned int timelapsed = static_cast<unsigned int> ((tend - tbegin) / CLOCKS_PER_SEC);
			if (timelapsed <= 1) OS << "MMMmmm that was FAST ... maybe your searchstring was wrong?\t" << searchPathCopy << "\nSearch path. E.g. 'C:/' or 'C:\\Prog**' \n";
			_ftprintf(stderr, _T("\nFinished \tin %u s\n\n"), timelapsed);
			if (!console && outHandle != NULL && outHandle != INVALID_HANDLE_VALUE) CloseHandle(outHandle);
		}

		catch (std::invalid_argument& ex)
		{
			fprintf(stderr, "\n\n%s\n\n", ex.what());
			result = ERROR_BAD_PATHNAME;
		}

		catch (CStructured_Exception& ex)
		{
			result = ex.GetSENumber();
			_ftprintf(stderr, _T("\n\nError: %s\n\n"), GetAnyErrorText(result));
		}

		//}

		//else { 	result = ERROR_BAD_ARGUMENTS;	}

	return result;
}
