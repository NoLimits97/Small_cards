@echo off
setlocal
cd /d "%~dp0" || exit /b 1

where cmake >nul 2>&1
if errorlevel 1 (
    echo ERROR: CMake not found. Start aider from Developer PowerShell.
    exit /b 1
)

echo === Configure ===
cmake -S . -B build
if errorlevel 1 exit /b 1

echo === Build core_tests Debug ===
cmake --build build --config Debug --target core_tests
if errorlevel 1 exit /b 1

echo === Build storage_tests Debug ===
cmake --build build --config Debug --target storage_tests
if errorlevel 1 exit /b 1

echo === Build gui_tests Debug ===
cmake --build build --config Debug --target gui_tests
if errorlevel 1 exit /b 1

echo === Build mk_app Debug ===
cmake --build build --config Debug --target mk_app
if errorlevel 1 exit /b 1

echo === Tests ===
ctest --test-dir build -C Debug --output-on-failure --no-tests=error
if errorlevel 1 exit /b 1

echo === SUCCESS ===
exit /b 0
