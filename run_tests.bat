@echo off
setlocal
cd /d "%~dp0" || exit /b 1
cmake -S . -B build || exit /b 1
cmake --build build --config Debug --target core_tests || exit /b 1
ctest --test-dir build -C Debug --output-on-failure --no-tests=error
exit /b %ERRORLEVEL%
