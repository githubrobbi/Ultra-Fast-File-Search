@SetLocal
@Set "BASEDIR=%SystemDrive%\WinDDK\3790.1830"
@Set "INCLUDE=%BASEDIR%\inc\crt;%BASEDIR%\inc\wnet;%BASEDIR%\inc\atl30;%BASEDIR%\inc\mfc42"
@Set "LIB=%BASEDIR%\lib\crt\amd64;%BASEDIR%\lib\atl\amd64;%BASEDIR%\lib\wnet\amd64"
@Set "OUTDIR=x64\Release (Windows 2003 DDK)"
@If ""=="%ProgramW6432%" @Set "ProgramW6432=%ProgramFiles%"
"%BASEDIR%\bin\x86\rc.exe" /fo "%OUTDIR%\SwiftSearch.res" "SwiftSearch.rc" || Goto :EOF
"%BASEDIR%\bin\x86\cvtres.exe" /machine:amd64 /nologo /out:"%OUTDIR%\SwiftSearch.res.obj" /readonly "%OUTDIR%\SwiftSearch.res" || Goto :EOF
@REM -DGetExceptionCode=__exception_code -DGetExceptionInformation=__exception_info -D_M_X64 -D_M_AMD64 -D_AMD64_ -fms-extensions -fms-compatibility -fdelayed-template-parsing -fms-compatibility-version=13.10.4035
clang-cl -fmsc-version=1300 /Og /Oi /Os /Oy /Ob2 /Gs /GF /Gy /DUNICODE=1 /D_UNICODE=1 /EHsc /MD /Zc:sizedDealloc- /Zc:threadSafeInit- /I "%WTL_ROOT%" /I "%BOOST_ROOT%\." /I "%ProgramFiles(x86)%\Microsoft SDKs\Windows\v5.0\Include" /I "%ProgramW6432%\Microsoft SDKs\Windows\v6.0A\Include" /I compat -Wno-nonportable-include-path -ferror-limit=1024 /FI stdafx.h SwiftSearch.cpp string_matcher.cpp "%OUTDIR%\SwiftSearch.res.obj" "%OUTDIR%\CxxFrameHandler.obj" %* /link /Debug /Subsystem:Windows /NoDefaultLib:libcpmt /NoDefaultLib:libc /DefaultLib:msvcprt /DefaultLib:BufferOverflowU /DefaultLib:comdlg32 /DefaultLib:oleaut32 /DefaultLib:shell32 /DefaultLib:shlwapi || Goto :EOF
@EndLocal
