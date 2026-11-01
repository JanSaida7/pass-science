@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
if %errorlevel% neq 0 exit /b %errorlevel%

set CMAKE="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

echo [1/3] Configuring CMake...
%CMAKE% -B build -G "Visual Studio 17 2022" -A x64
if %errorlevel% neq 0 exit /b %errorlevel%

echo [2/3] Building Release targets...
%CMAKE% --build build --config Release
if %errorlevel% neq 0 exit /b %errorlevel%

echo [3/3] Staging executables...
if not exist "bin" mkdir "bin"
copy /Y "build\Release\Launch.exe" "bin\Launch.exe"
copy /Y "build\Release\run_tests.exe" "bin\run_tests.exe"
copy /Y "build\Release\Launch.exe" "Launch.exe"

echo Build completed successfully.
