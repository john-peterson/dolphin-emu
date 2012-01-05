set BUILDCONFIG=%~1
set build_type=%2
set PLATFORM=%3
set PROJECT=%4
set BUILDTYPE=Build

call "%VS100COMNTOOLS%..\..\VC\vcvarsall.bat" %build_type%

devenv /nologo Dolphin_2010.sln /Project %PROJECT% /%BUILDTYPE% "%BUILDCONFIG%|%PLATFORM%"

if %ERRORLEVEL% neq 0 (
@echo %~1 %3 %4 failed
pause
exit
)