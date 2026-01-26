// ============================================================================
// main_dialog.cpp - CMainDlg method implementations
// ============================================================================
// Extracted from main_dialog.hpp as part of Phase 8.3 refactoring.
// This file contains the implementations of large methods that were previously
// defined inline in the header file.
//
// Moving these implementations to a .cpp file:
// 1. Reduces header file size (from ~3500 to ~1500 lines)
// 2. Improves compilation times (changes here don't recompile all includers)
// 3. Improves code organization and maintainability
// ============================================================================

#include "main_dialog_common.hpp"
#include "main_dialog.hpp"

// ============================================================================
// CMainDlg::Search() - Main search implementation
// ============================================================================
// This is the core search method that:
// 1. Reads the search pattern from the UI
// 2. Initializes the match operation
// 3. Waits for NTFS indices to be ready
// 4. Iterates through all matching files
// 5. Updates the results list
// ============================================================================
void CMainDlg::Search()
{
    WTL::CRegKeyEx key;
    if (key.Open(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer")) == ERROR_SUCCESS)
    {
        key.QueryDWORDValue(_T("AltColor"), compressedColor);
        key.QueryDWORDValue(_T("AltEncryptedColor"), encryptedColor);
        key.Close();
    }

    MatchOperation matchop;
    try
    {
        std::tstring pattern;
        {
            ATL::CComBSTR bstr;
            if (this->txtPattern.GetWindowText(bstr.m_str))
            {
                pattern.assign(bstr, bstr.Length());
            }
        }

        matchop.init(pattern);
    }
    catch (std::invalid_argument& ex)
    {
        RefCountedCString error = this->LoadString(IDS_INVALID_PATTERN);
        error += _T("\r\n");
        error += ex.what();
        this->MessageBox(error, this->LoadString(IDS_ERROR_TITLE), MB_ICONERROR);
        return;
    }

    bool preallocate = false;
    bool const shift_pressed = GetKeyState(VK_SHIFT) < 0;
    bool const control_pressed = GetKeyState(VK_CONTROL) < 0;
    int const selected = this->cmbDrive.GetCurSel();
    this->clear(false, false);
    WTL::CWaitCursor const wait_cursor;
    CProgressDialog dlg(this->m_hWnd);
    dlg.SetProgressTitle(this->LoadString(IDS_SEARCHING_TITLE));
    if (dlg.HasUserCancelled())
    {
        return;
    }

    using std::ceil;
    clock_t const tstart = clock();
    std::vector<uintptr_t> wait_handles;
    typedef NtfsIndex const volatile* index_pointer;
    std::vector<index_pointer> nonwait_indices, wait_indices, initial_wait_indices;
    bool any_io_pending = false;
    size_t expected_results = 0;
    size_t overall_progress_numerator = 0, overall_progress_denominator = 0;
    
    for (int ii = this->cmbDrive.GetCount(); ii > 0 && ((void)--ii, true);)
    {
        if (intrusive_ptr<NtfsIndex> const p = static_cast<NtfsIndex*>(this->cmbDrive.GetItemDataPtr(ii)))
        {
            bool wait = false;
            if (selected == ii || selected == 0)
            {
                if (matchop.prematch(p->root_path()))
                {
                    wait = true;
                    wait_handles.push_back(p->finished_event());
                    wait_indices.push_back(p.get());
                    expected_results += p->expected_records();
                    size_t const records_so_far = p->records_so_far();
                    any_io_pending |= records_so_far < p->mft_capacity;
                    overall_progress_denominator += p->mft_capacity * 2;
                }
            }

            if (!wait)
            {
                nonwait_indices.push_back(p.get());
            }
        }
    }

    initial_wait_indices = wait_indices;
    if (!any_io_pending)
    {
        overall_progress_denominator /= 2;
    }

    if (any_io_pending)
    {
        dlg.ForceShow();
    }

    if (preallocate)
    {
        try
        {
            this->results.reserve(this->results.size() + expected_results + expected_results / 8);
        }
        catch (std::bad_alloc&) {}
    }

    std::vector<IoPriority> set_priorities(nonwait_indices.size() + wait_indices.size());
    for (size_t i = 0; i != nonwait_indices.size(); ++i)
    {
        IoPriority(reinterpret_cast<uintptr_t>(nonwait_indices[i]->volume()), winnt::IoPriorityLow).swap(set_priorities.at(i));
    }

    for (size_t i = 0; i != wait_indices.size(); ++i)
    {
        IoPriority(reinterpret_cast<uintptr_t>(wait_indices[i]->volume()), winnt::IoPriorityLow).swap(set_priorities.at(nonwait_indices.size() + i));
    }

    IoPriority set_priority;
    Speed::second_type initial_time = Speed::second_type();
    Speed::first_type initial_average_amount = Speed::first_type();
    std::vector<Results> results_at_depths;
    results_at_depths.reserve(std::numeric_limits<unsigned short>::max() + 1);

    while (!wait_handles.empty())
    {
        if (uintptr_t const volume = reinterpret_cast<uintptr_t>(wait_indices.at(0)->volume()))
        {
            if (set_priority.volume() != volume)
            {
                IoPriority(volume, winnt::IoPriorityNormal).swap(set_priority);
            }
        }

        unsigned long const wait_result = dlg.WaitMessageLoop(wait_handles.empty() ? nullptr : &*wait_handles.begin(), wait_handles.size());
        if (wait_result == WAIT_TIMEOUT)
        {
            if (dlg.HasUserCancelled()) { break; }
            if (dlg.ShouldUpdate())
            {
                basic_fast_ostringstream<TCHAR> ss;
                ss << this->LoadString(IDS_TEXT_READING_FILE_TABLES) << this->LoadString(IDS_TEXT_SPACE);
                bool any = false;
                unsigned long long temp_overall_progress_numerator = overall_progress_numerator;
                for (const auto& j : wait_indices)
                {
                    size_t const records_so_far = j->records_so_far();
                    temp_overall_progress_numerator += records_so_far;
                    unsigned int const mft_capacity = j->mft_capacity;
                    if (records_so_far != mft_capacity)
                    {
                        if (any) { ss << this->LoadString(IDS_TEXT_COMMA) << this->LoadString(IDS_TEXT_SPACE); }
                        else { ss << this->LoadString(IDS_TEXT_SPACE); }
                        ss << j->root_path() << this->LoadString(IDS_TEXT_SPACE) << this->LoadString(IDS_TEXT_PAREN_OPEN) << nformat_ui(j->records_so_far());
                        ss << this->LoadString(IDS_TEXT_SPACE) << this->LoadString(IDS_TEXT_OF) << this->LoadString(IDS_TEXT_SPACE);
                        ss << nformat_ui(mft_capacity) << this->LoadString(IDS_TEXT_PAREN_CLOSE);
                        any = true;
                    }
                }

                bool const initial_speed = !initial_average_amount;
                Speed recent_speed, average_speed;
                for (const auto& idx : initial_wait_indices)
                {
                    Speed const speed = idx->speed();
                    average_speed.first += speed.first;
                    average_speed.second += speed.second;
                    if (initial_speed) { initial_average_amount += speed.first; }
                }

                clock_t const tnow = clock();
                if (initial_speed) { initial_time = tnow; }

                if (average_speed.first > initial_average_amount)
                {
                    ss << _T('\n');
                    ss << this->LoadString(IDS_TEXT_AVERAGE_SPEED) << this->LoadString(IDS_TEXT_COLON) << this->LoadString(IDS_TEXT_SPACE) <<
                        nformat_ui(static_cast<size_t>((average_speed.first - initial_average_amount) * static_cast<double>(CLOCKS_PER_SEC) / ((tnow != initial_time ? tnow - initial_time : 1) * static_cast<double>(1ULL << 20)))) <<
                        this->LoadString(IDS_TEXT_SPACE) << this->LoadString(IDS_TEXT_MIB_S);
                    ss << this->LoadString(IDS_TEXT_SPACE);
                    ss << this->LoadString(IDS_TEXT_PAREN_OPEN) << nformat_ui(average_speed.first / (1 << 20)) << this->LoadString(IDS_TEXT_SPACE) << this->LoadString(IDS_TEXT_MIB_READ) << this->LoadString(IDS_TEXT_PAREN_CLOSE);
                }

                std::tstring const text = ss.str();
                dlg.SetProgressText(text);
                dlg.SetProgress(static_cast<long long>(temp_overall_progress_numerator), static_cast<long long>(overall_progress_denominator));
                dlg.Flush();
            }
        }
        else
        {
            if (wait_result < wait_handles.size())
            {
                index_pointer const i = wait_indices[wait_result];
                size_t current_progress_numerator = 0;
                size_t const current_progress_denominator = i->total_names_and_streams();
                std::tvstring root_path = i->root_path();
                std::tvstring current_path = matchop.get_current_path(root_path);
                unsigned int const task_result = i->get_finished();
                if (task_result != 0)
                {
                    if (selected != 0)
                    {
                        ATL::CWindow(dlg.IsWindow() ? dlg.GetHWND() : this->m_hWnd).MessageBox(GetAnyErrorText(task_result), this->LoadString(IDS_ERROR_TITLE), MB_OK | MB_ICONERROR);
                    }
                }

                try
                {
                    lock(i)->matches([&dlg, &results_at_depths, &root_path, shift_pressed, this, i, &wait_indices, any_io_pending, &current_progress_numerator, current_progress_denominator,
                        overall_progress_numerator, overall_progress_denominator, &matchop
                    ](TCHAR const* const name, size_t const name_length, bool const ascii, NtfsIndex::key_type const& key, size_t const depth)
                    {
                        unsigned long long const now = GetTickCount64();
                        if (dlg.ShouldUpdate(now) || current_progress_denominator - current_progress_numerator <= 1)
                        {
                            if (dlg.HasUserCancelled(now)) { throw CStructured_Exception(ERROR_CANCELLED, nullptr); }
                            size_t temp_overall_progress_numerator = overall_progress_numerator;
                            if (any_io_pending)
                            {
                                for (const auto& idx : wait_indices) { temp_overall_progress_numerator += idx->records_so_far(); }
                            }

                            std::tvstring text(0x100 + root_path.size(), _T('\0'));
                            text.resize(static_cast<size_t>(_sntprintf(&*text.begin(), text.size(), _T("%s%s%.*s%s%s%s%s%s%s%s%s%s\r\n"),
                                this->LoadString(IDS_TEXT_SEARCHING).c_str(),
                                this->LoadString(IDS_TEXT_SPACE).c_str(),
                                static_cast<int>(root_path.size()), root_path.c_str(),
                                this->LoadString(IDS_TEXT_SPACE).c_str(),
                                this->LoadString(IDS_TEXT_PAREN_OPEN).c_str(),
                                static_cast<std::tstring>(nformat_ui(current_progress_numerator)).c_str(),
                                this->LoadString(IDS_TEXT_SPACE).c_str(),
                                this->LoadString(IDS_TEXT_OF).c_str(),
                                this->LoadString(IDS_TEXT_SPACE).c_str(),
                                static_cast<std::tstring>(nformat_ui(current_progress_denominator)).c_str(),
                                this->LoadString(IDS_TEXT_PAREN_CLOSE).c_str(),
                                this->LoadString(IDS_TEXT_ELLIPSIS).c_str())));
                            if (name_length) { append_directional(text, name, name_length, ascii ? -1 : 0); }

                            dlg.SetProgressText(text);
                            dlg.SetProgress(temp_overall_progress_numerator + static_cast<unsigned long long>(i->mft_capacity) * static_cast<unsigned long long>(current_progress_numerator) / static_cast<unsigned long long>(current_progress_denominator), static_cast<long long>(overall_progress_denominator));
                            dlg.Flush();
                        }

                        ++current_progress_numerator;
                        if (current_progress_numerator > current_progress_denominator) { throw std::logic_error("current_progress_numerator > current_progress_denominator"); }

                        TCHAR const* const path_begin = name;
                        size_t high_water_mark = 0, *phigh_water_mark = matchop.is_path_pattern && this->trie_filtering ? &high_water_mark : nullptr;
                        bool const match = ascii ?
                            matchop.matcher.is_match(static_cast<char const*>(static_cast<void const*>(path_begin)), name_length, phigh_water_mark) :
                            matchop.matcher.is_match(path_begin, name_length, phigh_water_mark);
                        if (match)
                        {
                            unsigned short const depth2 = static_cast<unsigned short>(depth * 2U);
                            if (shift_pressed)
                            {
                                if (depth2 >= results_at_depths.size()) { results_at_depths.resize(depth2 + 1); }
                                results_at_depths[depth2].push_back(i, Results::value_type(key, depth2));
                            }
                            else { this->results.push_back(i, Results::value_type(key, depth2)); }
                        }
                        return match || !(matchop.is_path_pattern && phigh_water_mark && *phigh_water_mark < name_length);
                    }, current_path, matchop.is_path_pattern, matchop.is_stream_pattern, control_pressed);
                }
                catch (CStructured_Exception& ex)
                {
                    if (ex.GetSENumber() != ERROR_CANCELLED) { throw; }
                }

                if (any_io_pending) { overall_progress_numerator += i->mft_capacity; }
                if (current_progress_denominator)
                {
                    overall_progress_numerator += static_cast<size_t>(static_cast<unsigned long long>(i->mft_capacity) * static_cast<unsigned long long>(current_progress_numerator) / static_cast<unsigned long long>(current_progress_denominator));
                }
            }

            wait_indices.erase(wait_indices.begin() + static_cast<ptrdiff_t>(wait_result));
            wait_handles.erase(wait_handles.begin() + static_cast<ptrdiff_t>(wait_result));
        }
    }

    {
        size_t size_to_reserve = 0;
        for (const auto& results_at_depth : results_at_depths) { size_to_reserve += results_at_depth.size(); }

        this->results.reserve(this->results.size() + size_to_reserve);
        for (size_t j = results_at_depths.size(); j != 0 && ((void)--j, true);)
        {
            Results const& results_at_depth = results_at_depths[j];
            for (size_t i = results_at_depth.size(); i != 0 && ((void)--i, true);)
            {
                this->results.push_back(results_at_depth.item_index(i), results_at_depth[i]);
            }
        }

        this->SetItemCount(static_cast<int>(this->results.size()));
    }

    clock_t const tend = clock();
    TCHAR buf[0x100];
    safe_stprintf(buf, this->LoadString(IDS_STATUS_FOUND_RESULTS), static_cast<std::tstring>(nformat_ui(this->results.size())).c_str(), (tend - tstart) * 1.0 / CLOCKS_PER_SEC);
    this->statusbar.SetText(0, buf);
}

// ============================================================================
// CMainDlg::dump_or_save() - Export results to file or clipboard
// ============================================================================
void CMainDlg::dump_or_save(std::vector<size_t> const& locked_indices, int const mode, int const single_column)
{
    std::tstring file_dialog_save_options;
    if (mode <= 0)
    {
        std::tstring const null_char(1, _T('\0'));
        file_dialog_save_options += this->LoadString(IDS_SAVE_OPTION_UTF8_CSV);
        file_dialog_save_options += null_char;
        file_dialog_save_options += _T("*.csv");
        file_dialog_save_options += null_char;
        file_dialog_save_options += this->LoadString(IDS_SAVE_OPTION_UTF8_TSV);
        file_dialog_save_options += null_char;
        file_dialog_save_options += _T("*.tsv");
        file_dialog_save_options += null_char;
        file_dialog_save_options += null_char;
    }

    WTL::CFileDialog fdlg(FALSE, _T("csv"), nullptr, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, file_dialog_save_options.c_str(), *this);
    fdlg.m_ofn.lpfnHook = nullptr;
    fdlg.m_ofn.Flags &= ~OFN_ENABLEHOOK;
    fdlg.m_ofn.lpstrTitle = this->LoadString(IDS_SAVE_TABLE_TITLE);
    if (mode <= 0 ? GetSaveFileName(&fdlg.m_ofn) : (locked_indices.size() <= 1 << 16 || this->MessageBox(this->LoadString(IDS_COPY_MAY_USE_TOO_MUCH_MEMORY_BODY), this->LoadString(IDS_WARNING_TITLE), MB_OKCANCEL | MB_ICONWARNING) == IDOK))
    {
        bool const tabsep = mode > 0 || fdlg.m_ofn.nFilterIndex > 1;
        WTL::CWaitCursor wait;
        int const ncolumns = this->lvFiles.GetHeader().GetItemCount();
        File const output = { mode <= 0 ? _topen(fdlg.m_ofn.lpstrFile, _O_BINARY | _O_TRUNC | _O_CREAT | _O_RDWR | _O_SEQUENTIAL, _S_IREAD | _S_IWRITE) : -1 };

        if (mode > 0 || output != -1)
        {
            bool const shell_file_list = mode == 2;
            if (shell_file_list) { assert(single_column == COLUMN_INDEX_PATH && "code below assumes path column is the only one specified"); }

            struct Clipboard
            {
                bool success;
                ~Clipboard() { if (success) { ::CloseClipboard(); } }
                explicit Clipboard(HWND owner) : success(owner ? !!::OpenClipboard(owner) : false) {}
                operator bool() const { return this->success; }
            };

            Clipboard clipboard(output ? nullptr : this->m_hWnd);
            if (clipboard) { EmptyClipboard(); }

            CProgressDialog dlg(this->m_hWnd);
            dlg.SetProgressTitle(this->LoadString(output ? IDS_DUMPING_TITLE : IDS_COPYING_TITLE));
            if (locked_indices.size() > 1 && dlg.HasUserCancelled()) { return; }

            std::string line_buffer_utf8;
            SingleMovableGlobalAllocator global_alloc;
            std::tvstring line_buffer(static_cast<std::tvstring::allocator_type>(output ? nullptr : &global_alloc));
            size_t const buffer_size = 1 << 22;
            line_buffer.reserve(buffer_size);

            if (shell_file_list)
            {
                WTL::CPoint pt;
                GetCursorPos(&pt);
                DROPFILES df = { sizeof(df), pt, FALSE, sizeof(*line_buffer.data()) > sizeof(char) };
                line_buffer.append(reinterpret_cast<TCHAR const*>(&df), sizeof(df) / sizeof(TCHAR));
            }

            if (!output && (single_column == COLUMN_INDEX_PATH || single_column < 0))
            {
                line_buffer.reserve(locked_indices.size() * MAX_PATH * (single_column >= 0 ? 2 : 3) / 4);
            }

            std::vector<int> displayed_columns(ncolumns, -1);
            this->lvFiles.GetColumnOrderArray(static_cast<int>(displayed_columns.size()), &*displayed_columns.begin());
            unsigned long long nwritten_since_update = 0;
            unsigned long long prev_update_time = GetTickCount64();

            try
            {
                bool warned_about_ads = false;
                for (size_t i = 0; i < locked_indices.size(); ++i)
                {
                    bool should_flush = i + 1 >= locked_indices.size();
                    size_t const entry_begin_offset = line_buffer.size();
                    bool any = false;
                    bool is_ads = false;

                    for (int c = 0; c < (single_column >= 0 ? 1 : ncolumns); ++c)
                    {
                        int const j = single_column >= 0 ? single_column : displayed_columns[c];
                        if (j == COLUMN_INDEX_NAME) { continue; }
                        if (any) { line_buffer.push_back(tabsep ? _T('\t') : _T(',')); }

                        size_t const begin_offset = line_buffer.size();
                        this->GetSubItemText(locked_indices[i], j, false, line_buffer, false);

                        if (j == COLUMN_INDEX_PATH)
                        {
                            size_t last_colon_index = line_buffer.size();
                            for (size_t k = begin_offset; k != line_buffer.size(); ++k)
                            {
                                if (line_buffer[k] == _T(':')) { last_colon_index = k; }
                            }

                            TCHAR const first_char = begin_offset < line_buffer.size() ? line_buffer[begin_offset] : _T('\0');
                            is_ads = is_ads || (begin_offset <= last_colon_index && last_colon_index < line_buffer.size() && !(last_colon_index == begin_offset + 1 && ((_T('a') <= first_char && first_char <= _T('z')) || (_T('A') <= first_char && first_char <= _T('Z'))) && line_buffer.size() > last_colon_index + 1 && line_buffer.at(last_colon_index + 1) == _T('\\')));

                            unsigned long long const update_time = GetTickCount64();
                            if (dlg.ShouldUpdate(update_time) || i + 1 == locked_indices.size())
                            {
                                if (dlg.HasUserCancelled(update_time)) { throw CStructured_Exception(ERROR_CANCELLED, nullptr); }
                                should_flush = true;
                                basic_fast_ostringstream<TCHAR> ss;
                                ss << this->LoadString(output ? IDS_TEXT_DUMPING_SELECTION : IDS_TEXT_COPYING_SELECTION) << this->LoadString(IDS_TEXT_SPACE);
                                ss << nformat_ui(i + 1);
                                ss << this->LoadString(IDS_TEXT_SPACE) << this->LoadString(IDS_TEXT_OF) << this->LoadString(IDS_TEXT_SPACE);
                                ss << nformat_ui(locked_indices.size());
                                if (update_time != prev_update_time)
                                {
                                    ss << this->LoadString(IDS_TEXT_SPACE) << this->LoadString(IDS_TEXT_PAREN_OPEN);
                                    ss << nformat_ui(nwritten_since_update * 1000U / ((update_time - prev_update_time) * 1ULL << 20));
                                    ss << this->LoadString(IDS_TEXT_SPACE) << this->LoadString(IDS_TEXT_MIB_S) << this->LoadString(IDS_TEXT_PAREN_CLOSE);
                                }
                                ss << this->LoadString(IDS_TEXT_COLON) << _T('\n');
                                ss.append(line_buffer.data() + static_cast<ptrdiff_t>(begin_offset), line_buffer.size() - begin_offset);
                                std::tstring const& text = ss.str();
                                dlg.SetProgressText(text);
                                dlg.SetProgress(static_cast<long long>(i), static_cast<long long>(locked_indices.size()));
                                dlg.Flush();
                            }
                        }

                        if (tabsep)
                        {
                            bool may_contain_tabs = false;
                            if (may_contain_tabs && line_buffer.find(_T('\t'), begin_offset) != std::tstring::npos)
                            {
                                line_buffer.insert(line_buffer.begin() + static_cast<ptrdiff_t>(begin_offset), _T('\"'));
                                line_buffer.push_back(_T('\"'));
                            }
                        }
                        else
                        {
                            if (line_buffer.find(_T(','), begin_offset) != std::tstring::npos ||
                                line_buffer.find(_T('\''), begin_offset) != std::tstring::npos)
                            {
                                line_buffer.insert(line_buffer.begin() + static_cast<ptrdiff_t>(begin_offset), _T('\"'));
                                line_buffer.push_back(_T('\"'));
                            }
                        }
                        any = true;
                    }

                    if (shell_file_list) { line_buffer.push_back(_T('\0')); }
                    else if (i < locked_indices.size() - 1) { line_buffer.push_back(_T('\r')); line_buffer.push_back(_T('\n')); }

                    if (shell_file_list && is_ads)
                    {
                        if (!warned_about_ads)
                        {
                            unsigned long long const tick_before = GetTickCount64();
                            ATL::CWindow(dlg.IsWindow() ? dlg.GetHWND() : this->m_hWnd).MessageBox(this->LoadString(IDS_COPYING_ADS_PROBLEM_BODY), this->LoadString(IDS_WARNING_TITLE), MB_OK | MB_ICONWARNING);
                            prev_update_time += GetTickCount64() - tick_before;
                            warned_about_ads = true;
                        }
                        line_buffer.erase(line_buffer.begin() + static_cast<ptrdiff_t>(entry_begin_offset), line_buffer.end());
                    }

                    should_flush |= line_buffer.size() >= buffer_size;
                    if (should_flush)
                    {
                        if (output)
                        {
#if defined(_UNICODE) && _UNICODE
                            using std::max;
                            line_buffer_utf8.resize(max(line_buffer_utf8.size(), (line_buffer.size() + 1) * 6), _T('\0'));
                            int const cch = WideCharToMultiByte(CP_UTF8, 0, line_buffer.empty() ? nullptr : &line_buffer[0], static_cast<int>(line_buffer.size()), &line_buffer_utf8[0], static_cast<int>(line_buffer_utf8.size()), nullptr, nullptr);
                            if (cch > 0) { nwritten_since_update += _write(output, line_buffer_utf8.data(), sizeof(*line_buffer_utf8.data()) * static_cast<size_t>(cch)); }
#else
                            nwritten_since_update += _write(output, line_buffer.data(), sizeof(*line_buffer.data()) * line_buffer.size());
#endif
                            line_buffer.clear();
                        }
                        else { nwritten_since_update = line_buffer.size() * sizeof(*line_buffer.begin()); }
                    }
                }

                if (clipboard)
                {
                    unsigned int const format = shell_file_list ? CF_HDROP : sizeof(*line_buffer.data()) > sizeof(char) ? CF_UNICODETEXT : CF_TEXT;
                    HGLOBAL const hGlobal = GlobalHandle(line_buffer.c_str());
                    if (hGlobal)
                    if (HGLOBAL const resized = GlobalReAlloc(hGlobal, (line_buffer.size() + 1) * sizeof(*line_buffer.data()), 0))
                    {
                        HANDLE const result = SetClipboardData(format, resized);
                        std::tvstring(line_buffer.get_allocator()).swap(line_buffer);
                        if (result == resized) { (void)global_alloc.disown(result); }
                    }
                }
            }
            catch (CStructured_Exception& ex)
            {
                if (ex.GetSENumber() != ERROR_CANCELLED) { throw; }
            }
        }
    }
}

// ============================================================================
// CMainDlg::RightClick() - Context menu handling
// ============================================================================
void CMainDlg::RightClick(std::vector<size_t> const& indices, POINT const& point, int const focused)
{
    Wow64Disable const wow64disable(true);
    LockedResults locked_results(*this);
    typedef std::vector<size_t> Indices;
    std::for_each<Indices::const_iterator, LockedResults&>(indices.begin(), indices.end(), locked_results);
    ATL::CComPtr<IShellFolder> desktop;
    HRESULT volatile hr = S_OK;
    UINT const minID = 1000;
    WTL::CMenu menu;
    menu.CreatePopupMenu();
    ATL::CComPtr<IContextMenu> contextMenu;
    std::unique_ptr<std::pair<std::pair<CShellItemIDList, ATL::CComPtr<IShellFolder>>, std::vector<CShellItemIDList>>> p(new std::pair<std::pair<CShellItemIDList, ATL::CComPtr<IShellFolder>>, std::vector<CShellItemIDList>>());
    p->second.reserve(indices.size());

    if (indices.size() <= (1 << 10))
    {
        SFGAOF sfgao = 0;
        std::tvstring path;
        for (size_t const iresult : indices)
        {
            NtfsIndex const* const index = this->results.item_index(iresult)->unvolatile();
            path = index->root_path();
            if (index->get_path(this->results[iresult].key(), path, false)) { remove_path_stream_and_trailing_sep(path); }

            CShellItemIDList itemIdList;
            hr = SHParseDisplayName(path.c_str(), nullptr, &itemIdList, sfgao, &sfgao);
            if (hr == S_OK)
            {
                if (p->first.first.IsNull())
                {
                    p->first.first.Attach(ILClone(itemIdList));
                    ILRemoveLastID(p->first.first);
                }
                while (!ILIsEmpty(static_cast<LPCITEMIDLIST>(p->first.first)) && !ILIsParent(p->first.first, itemIdList, FALSE))
                {
                    ILRemoveLastID(p->first.first);
                }
                p->second.push_back(CShellItemIDList());
                p->second.back().Attach(itemIdList.Detach());
            }
        }

        if (hr == S_OK)
        {
            hr = SHGetDesktopFolder(&desktop);
            if (hr == S_OK)
            {
                if (!p->first.first.IsNull() && !ILIsEmpty(static_cast<LPCITEMIDLIST>(p->first.first)))
                {
                    hr = desktop->BindToObject(p->first.first, nullptr, IID_IShellFolder, reinterpret_cast<void**>(&p->first.second));
                }
                else { hr = desktop.QueryInterface(&p->first.second); }
            }
        }

        if (hr == S_OK)
        {
            bool const desktop_relative = p->first.first.IsNull() || ILIsEmpty(static_cast<LPCITEMIDLIST>(p->first.first));
            std::vector<LPCITEMIDLIST> relative_item_ids(p->second.size());
            for (size_t i = 0; i < p->second.size(); ++i)
            {
                relative_item_ids[i] = desktop_relative ? static_cast<LPCITEMIDLIST>(p->second[i]) : ILFindChild(p->first.first, p->second[i]);
            }

            GUID const IID_IContextMenu = { 0x000214E4L, 0, 0, { 0xC0, 0, 0, 0, 0, 0, 0, 0x46 } };
            hr = (desktop_relative ? desktop : p->first.second)->GetUIObjectOf(*this, static_cast<UINT>(relative_item_ids.size()),
                relative_item_ids.empty() ? nullptr : &relative_item_ids[0], IID_IContextMenu, nullptr, &reinterpret_cast<void*&>(contextMenu.p));
        }

        if (hr == S_OK) { hr = contextMenu->QueryContextMenu(menu, 0, minID, UINT_MAX, 0x80); }
    }

    unsigned int ninserted = 0;
    UINT const openContainingFolderId = minID - 1, fileIdId = minID - 2, copyId = minID - 3, copyPathId = minID - 4, copyTableId = minID - 5, dumpId = minID - 6;

    if (indices.size() == 1)
    {
        MENUITEMINFO mii2 = { sizeof(mii2), MIIM_ID | MIIM_STRING | MIIM_STATE, MFT_STRING, MFS_ENABLED, openContainingFolderId, nullptr, nullptr, nullptr, 0, this->LoadString(IDS_MENU_OPEN_CONTAINING_FOLDER) };
        menu.InsertMenuItem(ninserted++, TRUE, &mii2);
        if (false) { menu.SetMenuDefaultItem(openContainingFolderId, FALSE); }
    }

    if (0 <= focused && static_cast<size_t>(focused) < this->results.size())
    {
        { RefCountedCString text = this->LoadString(IDS_MENU_FILE_NUMBER);
        text += static_cast<std::tstring>(nformat_ui(this->results[static_cast<size_t>(focused)].key().frs())).c_str();
        MENUITEMINFO mii = { sizeof(mii), MIIM_ID | MIIM_STRING | MIIM_STATE, MFT_STRING, MFS_DISABLED, fileIdId, nullptr, nullptr, nullptr, 0, text };
        menu.InsertMenuItem(ninserted++, TRUE, &mii); }

        { MENUITEMINFO mii = { sizeof(mii), MIIM_ID | MIIM_STRING | MIIM_STATE, MFT_STRING, 0, copyId, nullptr, nullptr, nullptr, 0, this->LoadString(IDS_MENU_COPY) };
        menu.InsertMenuItem(ninserted++, TRUE, &mii); }

        { MENUITEMINFO mii = { sizeof(mii), MIIM_ID | MIIM_STRING | MIIM_STATE, MFT_STRING, 0, copyPathId, nullptr, nullptr, nullptr, 0, this->LoadString(IDS_MENU_COPY_PATHS) };
        menu.InsertMenuItem(ninserted++, TRUE, &mii); }

        { MENUITEMINFO mii = { sizeof(mii), MIIM_ID | MIIM_STRING | MIIM_STATE, MFT_STRING, 0, copyTableId, nullptr, nullptr, nullptr, 0, this->LoadString(IDS_MENU_COPY_TABLE) };
        menu.InsertMenuItem(ninserted++, TRUE, &mii); }

        { MENUITEMINFO mii = { sizeof(mii), MIIM_ID | MIIM_STRING | MIIM_STATE, MFT_STRING, 0, dumpId, nullptr, nullptr, nullptr, 0, this->LoadString(IDS_MENU_DUMP_TO_TABLE) };
        menu.InsertMenuItem(ninserted++, TRUE, &mii); }
    }

    if (contextMenu && ninserted)
    {
        MENUITEMINFO mii = { sizeof(mii), 0, MFT_MENUBREAK };
        menu.InsertMenuItem(ninserted, TRUE, &mii);
    }

    UINT id = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_NONOTIFY | (GetKeyState(VK_SHIFT) < 0 ? CMF_EXTENDEDVERBS : 0) |
        (GetSystemMetrics(SM_MENUDROPALIGNMENT) ? TPM_RIGHTALIGN | TPM_HORNEGANIMATION : TPM_LEFTALIGN | TPM_HORPOSANIMATION),
        point.x, point.y, *this);

    if (!id) { /* User cancelled */ }
    else if (id == openContainingFolderId)
    {
        if (QueueUserWorkItem(&SHOpenFolderAndSelectItemsThread, p.get(), WT_EXECUTEINUITHREAD)) { p.release(); }
    }
    else if (id == dumpId || id == copyId || id == copyPathId || id == copyTableId)
    {
        this->dump_or_save(indices, id == dumpId ? 0 : id == copyId ? 2 : 1, id == dumpId || id == copyTableId ? -1 : COLUMN_INDEX_PATH);
    }
    else if (id >= minID)
    {
        CMINVOKECOMMANDINFO cmd = { sizeof(cmd), CMIC_MASK_ASYNCOK, *this, reinterpret_cast<LPCSTR>(static_cast<uintptr_t>(id - minID)), nullptr, nullptr, SW_SHOW };
        hr = contextMenu ? contextMenu->InvokeCommand(&cmd) : S_FALSE;
        if (hr != S_OK) { this->MessageBox(GetAnyErrorText(hr), this->LoadString(IDS_ERROR_TITLE), MB_OK | MB_ICONERROR); }
    }
}

// ============================================================================
// CMainDlg::OnInitDialog() - Dialog initialization
// ============================================================================
BOOL CMainDlg::OnInitDialog(CWindow /*wndFocus*/, LPARAM /*lInitParam*/)
{
    _Module.GetMessageLoop()->AddMessageFilter(this);
    this->setTopmostWindow.reset(::topmostWindow, this->m_hWnd);

    this->SetWindowText(this->LoadString(IDS_APPNAME));
    this->lvFiles.Attach(this->GetDlgItem(IDC_LISTFILES));
    this->btnBrowse.Attach(this->GetDlgItem(IDC_BUTTON_BROWSE));
    this->btnBrowse.SetWindowText(this->LoadString(IDS_BUTTON_BROWSE));
    this->btnOK.Attach(this->GetDlgItem(IDOK));
    this->btnOK.SetWindowText(this->LoadString(IDS_BUTTON_SEARCH));
    this->cmbDrive.Attach(this->GetDlgItem(IDC_LISTVOLUMES));
    this->accel.LoadAccelerators(IDR_ACCELERATOR1);
    this->txtPattern.SubclassWindow(this->GetDlgItem(IDC_EDITFILENAME));
    if (!this->txtPattern) { this->txtPattern.Attach(this->GetDlgItem(IDC_EDITFILENAME)); }

    this->txtPattern.EnsureTrackingMouseHover();
    this->txtPattern.SetCueBannerText(this->LoadString(IDS_SEARCH_PATTERN_BANNER), true);
    WTL::CHeaderCtrl hdr = this->lvFiles.GetHeader();

    // Insert all columns
    { int const icol = COLUMN_INDEX_NAME;
    LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_LEFT, 250, this->LoadString(IDS_COLUMN_NAME_HEADER) };
    this->lvFiles.InsertColumn(icol, &column); }

    { int const icol = COLUMN_INDEX_PATH;
    LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_LEFT, 280, this->LoadString(IDS_COLUMN_PATH_HEADER) };
    this->lvFiles.InsertColumn(icol, &column); }

    { int const icol = COLUMN_INDEX_TYPE;
    LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_LEFT, 120, this->LoadString(IDS_COLUMN_TYPE_HEADER) };
    this->lvFiles.InsertColumn(icol, &column); }

    { int const icol = COLUMN_INDEX_SIZE;
    LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_RIGHT, 80, this->LoadString(IDS_COLUMN_SIZE_HEADER) };
    this->lvFiles.InsertColumn(icol, &column); }

    { int const icol = COLUMN_INDEX_SIZE_ON_DISK;
    LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_RIGHT, 80, this->LoadString(IDS_COLUMN_SIZE_ON_DISK_HEADER) };
    this->lvFiles.InsertColumn(icol, &column); }

    { int const icol = COLUMN_INDEX_CREATION_TIME;
    LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_CENTER, 140, this->LoadString(IDS_COLUMN_CREATION_TIME_HEADER) };
    this->lvFiles.InsertColumn(icol, &column); }

    { int const icol = COLUMN_INDEX_MODIFICATION_TIME;
    LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_CENTER, 140, this->LoadString(IDS_COLUMN_WRITE_TIME_HEADER) };
    this->lvFiles.InsertColumn(icol, &column); }

    { int const icol = COLUMN_INDEX_ACCESS_TIME;
    LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_CENTER, 140, this->LoadString(IDS_COLUMN_ACCESS_TIME_HEADER) };
    this->lvFiles.InsertColumn(icol, &column); }

    { int const icol = COLUMN_INDEX_DESCENDENTS;
    LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_RIGHT, 80, this->LoadString(IDS_COLUMN_DESCENDENTS_HEADER) };
    this->lvFiles.InsertColumn(icol, &column); }

    { int const icol = COLUMN_INDEX_is_readonly;
    LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_CENTER, 25, this->LoadString(IDS_COLUMN_INDEX_IS_READONLY_HEADER) };
    this->lvFiles.InsertColumn(icol, &column); }

    { int const icol = COLUMN_INDEX_is_archive;
    LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_CENTER, 25, this->LoadString(IDS_COLUMN_INDEX_IS_ARCHIVE_HEADER) };
    this->lvFiles.InsertColumn(icol, &column); }

    { int const icol = COLUMN_INDEX_is_system;
    LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_CENTER, 25, this->LoadString(IDS_COLUMN_INDEX_IS_SYSTEM_HEADER) };
    this->lvFiles.InsertColumn(icol, &column); }

    { int const icol = COLUMN_INDEX_is_hidden;
    LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_CENTER, 25, this->LoadString(IDS_COLUMN_INDEX_IS_HIDDEN_HEADER) };
    this->lvFiles.InsertColumn(icol, &column); }

    { int const icol = COLUMN_INDEX_is_offline;
    LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_CENTER, 25, this->LoadString(IDS_COLUMN_INDEX_IS_OFFLINE_HEADER) };
    this->lvFiles.InsertColumn(icol, &column); }

    { int const icol = COLUMN_INDEX_is_notcontentidx;
    LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_CENTER, 25, this->LoadString(IDS_COLUMN_INDEX_IS_NOTCONTENTIDX_HEADER) };
    this->lvFiles.InsertColumn(icol, &column); }

    { int const icol = COLUMN_INDEX_is_noscrubdata;
    LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_CENTER, 25, this->LoadString(IDS_COLUMN_INDEX_IS_NOSCRUBDATA_HEADER) };
    this->lvFiles.InsertColumn(icol, &column); }

    { int const icol = COLUMN_INDEX_is_integretystream;
    LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_CENTER, 25, this->LoadString(IDS_COLUMN_INDEX_IS_INTEGRETYSTREAM_HEADER) };
    this->lvFiles.InsertColumn(icol, &column); }

    { int const icol = COLUMN_INDEX_is_pinned;
    LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_CENTER, 25, this->LoadString(IDS_COLUMN_INDEX_IS_PINNED_HEADER) };
    this->lvFiles.InsertColumn(icol, &column); }

    { int const icol = COLUMN_INDEX_is_unpinned;
    LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_CENTER, 25, this->LoadString(IDS_COLUMN_INDEX_IS_UNPINNED_HEADER) };
    this->lvFiles.InsertColumn(icol, &column); }

    { int const icol = COLUMN_INDEX_is_directory;
    LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_CENTER, 30, this->LoadString(IDS_COLUMN_INDEX_IS_DIRECTORY_HEADER) };
    this->lvFiles.InsertColumn(icol, &column); }

    { int const icol = COLUMN_INDEX_is_compressed;
    LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_CENTER, 80, this->LoadString(IDS_COLUMN_INDEX_IS_COMPRESSED_HEADER) };
    this->lvFiles.InsertColumn(icol, &column); }

    { int const icol = COLUMN_INDEX_is_encrypted;
    LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_CENTER, 70, this->LoadString(IDS_COLUMN_INDEX_IS_ENCRYPTED_HEADER) };
    this->lvFiles.InsertColumn(icol, &column); }

    { int const icol = COLUMN_INDEX_is_sparsefile;
    LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_CENTER, 50, this->LoadString(IDS_COLUMN_INDEX_IS_SPARSEFILE_HEADER) };
    this->lvFiles.InsertColumn(icol, &column); }

    { int const icol = COLUMN_INDEX_is_reparsepoint;
    LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_CENTER, 60, this->LoadString(IDS_COLUMN_INDEX_IS_REPARSEPOINT_HEADER) };
    this->lvFiles.InsertColumn(icol, &column); }

    { int const icol = COLUMN_INDEX_ATTRIBUTE;
    LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_CENTER, 60, this->LoadString(IDS_COLUMN_INDEX_IS_ATTRIBUTE_HEADER) };
    this->lvFiles.InsertColumn(icol, &column); }

    this->cmbDrive.SetCueBannerText(this->LoadString(IDS_SEARCH_VOLUME_BANNER));
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    this->SetIcon((HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0), FALSE);
    this->SetIcon((HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0), TRUE);

    {
        const int IMAGE_LIST_INCREMENT = 0x100;
        this->imgListSmall.Create(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CXSMICON), ILC_COLOR32, 0, IMAGE_LIST_INCREMENT);
        this->imgListLarge.Create(GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CXICON), ILC_COLOR32, 0, IMAGE_LIST_INCREMENT);
        this->imgListExtraLarge.Create(48, 48, ILC_COLOR32, 0, IMAGE_LIST_INCREMENT);
    }

    this->lvFiles.OpenThemeData(VSCLASS_LISTVIEW);
    SetWindowTheme(this->lvFiles, _T("Explorer"), nullptr);
    if (false)
    {
        WTL::CFontHandle font = this->txtPattern.GetFont();
        LOGFONT logFont;
        if (font.GetLogFont(logFont))
        {
            logFont.lfHeight = logFont.lfHeight * 100 / 85;
            this->txtPattern.SetFont(WTL::CFontHandle().CreateFontIndirect(&logFont));
        }
    }

    this->trie_filtering = this->GetMenu().GetMenuState(ID_OPTIONS_FAST_PATH_SEARCH, MF_BYCOMMAND) & MFS_CHECKED;
    this->lvFiles.SetExtendedListViewStyle(LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP | (this->GetMenu().GetMenuState(ID_VIEW_GRIDLINES, MF_BYCOMMAND) ? LVS_EX_GRIDLINES : 0) | LVS_EX_HEADERDRAGDROP | 0x80000000);
    {
        this->small_image_list((this->GetMenu().GetMenuState(ID_VIEW_LARGEICONS, MF_BYCOMMAND) & MFS_CHECKED) ? this->imgListLarge : this->imgListSmall);
        this->lvFiles.SetImageList(this->imgListLarge, LVSIL_NORMAL);
        this->lvFiles.SetImageList(this->imgListExtraLarge, LVSIL_NORMAL);
    }

    this->statusbar = CreateStatusWindow(WS_CHILD | SBT_TOOLTIPS | WS_VISIBLE, nullptr, *this, IDC_STATUS_BAR);
    int const rcStatusPaneWidths[] = { 360, -1 };
    this->statusbar.SetParts(sizeof(rcStatusPaneWidths) / sizeof(*rcStatusPaneWidths), const_cast<int*>(rcStatusPaneWidths));
    this->statusbar.SetText(0, this->LoadString(IDS_STATUS_DEFAULT));
    WTL::CRect rcStatusPane1;
    this->statusbar.GetRect(1, &rcStatusPane1);
    WTL::CRect clientRect;
    if (this->lvFiles.GetClientRect(&clientRect))
    {
        clientRect.bottom -= rcStatusPane1.Height();
        this->lvFiles.ResizeClient(clientRect.Width(), clientRect.Height(), FALSE);
    }

    WTL::CRect my_client_rect;
    bool const unresize = this->GetClientRect(&my_client_rect) && this->template_size.cx > 0 && this->template_size.cy > 0 && my_client_rect.Size() != this->template_size && this->ResizeClient(this->template_size.cx, this->template_size.cy, FALSE);
    this->DlgResize_Init(false, false);
    if (unresize) { this->ResizeClient(my_client_rect.Width(), my_client_rect.Height(), FALSE); }

    std::vector<std::tvstring> path_names = get_volume_path_names();
    this->cmbDrive.SetCurSel(this->cmbDrive.AddString(this->LoadString(IDS_SEARCH_VOLUME_ALL)));
    for (const auto& path_name : path_names) { this->cmbDrive.AddString(path_name.c_str()); }

    if (!this->ShouldWaitForWindowVisibleOnStartup())
    {
        if (!this->_initialized)
        {
            this->_initialized = true;
            this->PostMessage(WM_READY);
        }
    }

    return TRUE;
}

// ============================================================================
// CMainDlg::GetSubItemText() - Get text for a list view subitem
// ============================================================================
void CMainDlg::GetSubItemText(size_t const j, int const subitem, bool const for_ui, std::tvstring& text, bool const lock_index) const
{
    lock_ptr<NtfsIndex const> i(this->results.item_index(j), lock_index);
    if (i->records_so_far() < this->results[j].key().frs()) { __debugbreak(); }

    NFormat const& nformat = for_ui ? nformat_ui : nformat_io;
    long long svalue;
    unsigned long long uvalue;
    NtfsIndex::key_type const key = this->results[j].key();
    NtfsIndex::key_type::frs_type const frs = key.frs();

    switch (subitem)
    {
    case COLUMN_INDEX_NAME:
        i->get_path(key, text, true);
        deldirsep(text);
        break;
    case COLUMN_INDEX_PATH:
        text += i->root_path();
        i->get_path(key, text, false);
        break;
    case COLUMN_INDEX_TYPE:
        { size_t const k = text.size(); text += i->root_path(); unsigned int attributes = 0; i->get_path(key, text, true, &attributes); this->get_file_type_blocking(text, k, attributes); }
        break;
    case COLUMN_INDEX_SIZE:
        uvalue = static_cast<unsigned long long>(i->get_sizes(key).length);
        text += nformat(uvalue);
        break;
    case COLUMN_INDEX_SIZE_ON_DISK:
        uvalue = static_cast<unsigned long long>(i->get_sizes(key).allocated);
        text += nformat(uvalue);
        break;
    case COLUMN_INDEX_CREATION_TIME:
        svalue = i->get_stdinfo(frs).created;
        SystemTimeToString(svalue, text, !for_ui);
        break;
    case COLUMN_INDEX_MODIFICATION_TIME:
        svalue = i->get_stdinfo(frs).written;
        SystemTimeToString(svalue, text, !for_ui);
        break;
    case COLUMN_INDEX_ACCESS_TIME:
        svalue = i->get_stdinfo(frs).accessed;
        SystemTimeToString(svalue, text, !for_ui);
        break;
    case COLUMN_INDEX_DESCENDENTS:
        uvalue = static_cast<unsigned long long>(i->get_sizes(key).treesize);
        if (uvalue) { text += nformat(uvalue - 1); }
        break;
    case COLUMN_INDEX_is_readonly:     if (i->get_stdinfo(frs).is_readonly        > 0) text.push_back(_T('+')); break;
    case COLUMN_INDEX_is_archive:      if (i->get_stdinfo(frs).is_archive         > 0) text.push_back(_T('+')); break;
    case COLUMN_INDEX_is_system:       if (i->get_stdinfo(frs).is_system          > 0) text.push_back(_T('+')); break;
    case COLUMN_INDEX_is_hidden:       if (i->get_stdinfo(frs).is_hidden          > 0) text.push_back(_T('+')); break;
    case COLUMN_INDEX_is_offline:      if (i->get_stdinfo(frs).is_offline         > 0) text.push_back(_T('+')); break;
    case COLUMN_INDEX_is_notcontentidx: if (i->get_stdinfo(frs).is_notcontentidx  > 0) text.push_back(_T('+')); break;
    case COLUMN_INDEX_is_noscrubdata:  if (i->get_stdinfo(frs).is_noscrubdata     > 0) text.push_back(_T('+')); break;
    case COLUMN_INDEX_is_integretystream: if (i->get_stdinfo(frs).is_integretystream > 0) text.push_back(_T('+')); break;
    case COLUMN_INDEX_is_pinned:       if (i->get_stdinfo(frs).is_pinned          > 0) text.push_back(_T('+')); break;
    case COLUMN_INDEX_is_unpinned:     if (i->get_stdinfo(frs).is_unpinned        > 0) text.push_back(_T('+')); break;
    case COLUMN_INDEX_is_directory:    if (i->get_stdinfo(frs).is_directory       > 0) text.push_back(_T('+')); break;
    case COLUMN_INDEX_is_compressed:   if (i->get_stdinfo(frs).is_compressed      > 0) text.push_back(_T('+')); break;
    case COLUMN_INDEX_is_encrypted:    if (i->get_stdinfo(frs).is_encrypted       > 0) text.push_back(_T('+')); break;
    case COLUMN_INDEX_is_sparsefile:   if (i->get_stdinfo(frs).is_sparsefile      > 0) text.push_back(_T('+')); break;
    case COLUMN_INDEX_is_reparsepoint: if (i->get_stdinfo(frs).is_reparsepoint    > 0) text.push_back(_T('+')); break;
    case COLUMN_INDEX_ATTRIBUTE:
        uvalue = i->get_stdinfo(frs).attributes();
        text += nformat(uvalue);
        break;
    default:
        break;
    }
}

// ============================================================================
// CMainDlg::Refresh() - Refresh the NTFS indices
// ============================================================================
void CMainDlg::Refresh(bool const initial)
{
    if (!initial && this->indices_created < this->cmbDrive.GetCount() - 1) { return; }

    this->clear(true, true);
    int const selected = this->cmbDrive.GetCurSel();
    for (int ii = 0; ii < this->cmbDrive.GetCount(); ++ii)
    {
        if (selected == 0 || ii == selected)
        {
            intrusive_ptr<NtfsIndex> q = static_cast<NtfsIndex*>(this->cmbDrive.GetItemDataPtr(ii));
            if (q || (initial && ii != 0))
            {
                std::tvstring path_name;
                if (initial)
                {
                    WTL::CString path_name_;
                    this->cmbDrive.GetLBText(ii, path_name_);
                    path_name = static_cast<LPCTSTR>(path_name_);
                }
                else
                {
                    path_name = q->root_path();
                    q->cancel();
                    if (this->cmbDrive.SetItemDataPtr(ii, nullptr) != CB_ERR)
                    {
                        --this->indices_created;
                        intrusive_ptr_release(q.get());
                    }
                }

                q.reset(new NtfsIndex(path_name), true);
                struct OverlappedNtfsMftReadPayloadDerived : OverlappedNtfsMftReadPayload
                {
                    HWND m_hWnd;
                    explicit OverlappedNtfsMftReadPayloadDerived(IoCompletionPort volatile& iocp, intrusive_ptr<NtfsIndex volatile> p, HWND const& m_hWnd, Handle const& closing_event)
                        : OverlappedNtfsMftReadPayload(iocp, p, closing_event), m_hWnd(m_hWnd) {}

                    void preopen() override
                    {
                        this->OverlappedNtfsMftReadPayload::preopen();
                        if (this->m_hWnd)
                        {
                            DEV_BROADCAST_HANDLE dev = { sizeof(dev), DBT_DEVTYP_HANDLE, 0, p->volume(), reinterpret_cast<HDEVNOTIFY>(this->m_hWnd) };
                            dev.dbch_hdevnotify = RegisterDeviceNotification(this->m_hWnd, &dev, DEVICE_NOTIFY_WINDOW_HANDLE);
                        }
                    }
                };

                intrusive_ptr<OverlappedNtfsMftReadPayload> p(new OverlappedNtfsMftReadPayloadDerived(this->iocp, q, this->m_hWnd, this->closing_event));
                this->iocp.post(0, static_cast<uintptr_t>(ii), p);
                if (this->cmbDrive.SetItemDataPtr(ii, q.get()) != CB_ERR)
                {
                    (void)q.detach();
                    ++this->indices_created;
                }
            }
        }
    }
}
