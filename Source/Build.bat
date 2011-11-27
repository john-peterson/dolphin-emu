set build_type=%1
set BUILDTYPE=Build
set BUILDCONFIG=Release
set PLATFORM=%2
set PROJECT=%3

call "%VS100COMNTOOLS%..\..\VC\vcvarsall.bat" %build_type%

devenv /nologo Dolphin_2010.sln /Project %PROJECT% /%BUILDTYPE% "%BUILDCONFIG%|%PLATFORM%"