#pragma once

// ============================================================================
// GUI Entry Point - _tWinMain()
// ============================================================================
// Extracted from UltraFastFileSearch.cpp as part of Phase 6 refactoring.
// This header is included at the end of UltraFastFileSearch.cpp where all
// dependencies (CMainDlg, extract_and_run_if_needed, etc.) are already defined.
// ============================================================================

int WINAPI _tWinMain(_In_ HINSTANCE
	const hInstance, _In_opt_ HINSTANCE /*hPrevInstance*/, _In_ LPTSTR /*lpCmdLine*/, _In_ int nShowCmd)
{
	(void)hInstance;
	(void)nShowCmd;
	int
		const argc = __argc;
	TCHAR** const argv = __targv;
	std::pair<int, std::tstring > extraction_result = extract_and_run_if_needed(hInstance, argc, argv);
	if (extraction_result.first != -1)
	{
		return extraction_result.first;
	}

	std::tstring const& module_path = extraction_result.second;
	(void)argc;
	(void)argv;
	//if (get_subsystem(&__ImageBase) != IMAGE_SUBSYSTEM_WINDOWS_GUI) { 	return _tmain(argc, argv);}

	//if (get_subsystem(&__ImageBase) != IMAGE_SUBSYSTEM_WINDOWS_GUI) { 	return main(argc, argv); }

int result = 0;
bool right_to_left = false;
unsigned long reading_layout;
if (GetLocaleInfo(LOCALE_USER_DEFAULT, 0x00000070 /*LOCALE_IREADINGLAYOUT*/ | LOCALE_RETURN_NUMBER, reinterpret_cast<LPTSTR> (&reading_layout), static_cast<int>(sizeof(reading_layout) / sizeof(TCHAR))) >= static_cast<int>(sizeof(reading_layout) / sizeof(TCHAR)))
{
	right_to_left = reading_layout == 1;
}
else
{
	right_to_left = !!(GetWindowLongPtr(FindWindow(_T("Shell_TrayWnd"), nullptr), GWL_EXSTYLE) & WS_EX_LAYOUTRTL);
}

if (right_to_left)
{
	SetProcessDefaultLayout(LAYOUT_RTL);
}

__if_exists(_Module)
{
	_Module.Init(nullptr, hInstance);
}

{
	RefCountedCString appname;
	(void)appname.LoadString(IDS_APPNAME);
	std::tstring eventName = _T("Local\\");
	eventName += static_cast<LPCTSTR>(appname);
	eventName += _T(".");
	eventName += get_app_guid();
	HANDLE hEvent = CreateEvent(nullptr, FALSE, FALSE, eventName.c_str());
	if (hEvent != nullptr && GetLastError() != ERROR_ALREADY_EXISTS)
	{
		WTL::CMessageLoop msgLoop;
		_Module.AddMessageLoop(&msgLoop);
		if (!module_path.empty())
		{
			// https://blogs.msdn.microsoft.com/jsocha/2011/12/14/allowing-localizing-after-the-fact-using-mui/
			std::tstring module_name = module_path;
			module_name.erase(module_name.begin(), basename(module_name.begin(), module_name.end()));
			std::tstring module_directory = module_path;
			module_directory.erase(dirname(module_directory.begin(), module_directory.end()), module_directory.end());
			if (!module_directory.empty())
			{
				module_directory += getdirsep();
			}

			std::tstring
				const period = TEXT(".");
			std::tstring
				const mui_ext = TEXT("mui");
			std::tstring
				const locale_name = static_cast<LPCTSTR> (get_ui_locale_name());
			std::tstring
				const mui_paths[] = { module_directory + locale_name + getdirsep() + module_name + period + mui_ext,
					module_directory + module_name + period + locale_name + period + mui_ext,
					module_directory + locale_name + getdirsep() + module_name + period + locale_name + period + mui_ext,
					module_directory + module_name + period + mui_ext,
			};

			for (size_t i = 0; i != sizeof(mui_paths) / sizeof(*mui_paths) && !mui_module; ++i)
			{
				unsigned int
					const mui_load_flags = LOAD_LIBRARY_AS_DATAFILE;
				for (int pass = 0; pass < 2 && !mui_module; ++pass)
				{
					mui_module = LoadLibraryEx(mui_paths[i].c_str(), nullptr, mui_load_flags | (!pass ? 0x00000020 /*LOAD_LIBRARY_AS_IMAGE_RESOURCE only works on Vista and later*/ : 0));
				}
			}
		}

		CMainDlg wnd(hEvent, right_to_left);
		wnd.Create(reinterpret_cast<HWND> (nullptr), 0);
		wnd.ShowWindow((wnd.GetStyle() & SW_MAXIMIZE) ? SW_SHOW : SW_SHOWDEFAULT);
		msgLoop.Run();
		_Module.RemoveMessageLoop();
	}
	else
	{
		AllowSetForegroundWindow(ASFW_ANY);
		if (hEvent != nullptr)
		{
			PulseEvent(hEvent);	// PulseThread() is normally unreliable, but we don't really care here...
		}
		result = GetLastError();
	}
}

__if_exists(_Module)
{
	_Module.Term();
}

return result;
}
