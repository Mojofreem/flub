@REM This is a generated file.
@echo off
setlocal
SET srcdir="C:/Users/liam/projects/flub-hub/3rdparty/pcre"
SET pcre2test="C:/Users/liam/projects/flub-hub/3rdparty/pcre/pcre2test.exe"
if not [%CMAKE_CONFIG_TYPE%]==[] SET pcre2test="C:/Users/liam/projects/flub-hub/3rdparty/pcre\%CMAKE_CONFIG_TYPE%\pcre2test.exe"
call %srcdir%\RunTest.Bat
if errorlevel 1 exit /b 1
echo RunTest.bat tests successfully completed
