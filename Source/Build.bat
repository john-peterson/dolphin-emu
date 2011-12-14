set build_type=%1
set BUILDTYPE=Build
set BUILDCONFIG=Release
set PLATFORM=%2
set PROJECT=%3

call "%VS110COMNTOOLS%..\..\VC\vcvarsall.bat" %build_type%

devenv /nologo Dolphin.sln /Project %PROJECT% /%BUILDTYPE% "%BUILDCONFIG%|%PLATFORM%"