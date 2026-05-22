@echo off
setlocal

set SCRIPT_DIR=%~dp0
set BUILD_DIR=%SCRIPT_DIR%build_mingw
set DIST_DIR=%SCRIPT_DIR%dist

if not exist "%DIST_DIR%" mkdir "%DIST_DIR%"

where cmake >nul 2>&1 || (
    echo ERROR: cmake not found in PATH
    echo Add cmake to PATH, e.g.: set PATH=C:\Program Files\CMake\bin;%%PATH%%
    exit /b 1
)

where g++ >nul 2>&1 || (
    echo ERROR: g++ not found in PATH
    echo Add MinGW to PATH, e.g.: set PATH=D:\Tool\TDM64-gcc\bin;%%PATH%%
    exit /b 1
)

if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
mkdir "%BUILD_DIR%"

cd /d "%BUILD_DIR%"

cmake -G "MinGW Makefiles" ^
      -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_C_FLAGS="-O2 -DNDEBUG -D_WIN32_WINNT=0x0601" ^
      -DCMAKE_CXX_FLAGS="-O2 -DNDEBUG -D_WIN32_WINNT=0x0601 -static-libgcc -static-libstdc++" ^
      -DCMAKE_EXE_LINKER_FLAGS="-s -static" ^
      -DCMAKE_SHARED_LINKER_FLAGS="-s -static" ^
      ..

mingw32-make -j%NUMBER_OF_PROCESSORS%

where strip >nul 2>&1 && (
    strip TextExtractionCLI\TextExtraction.exe
    echo Stripped debug symbols
) || echo strip not found, skipping

where upx >nul 2>&1 && (
    upx --best TextExtractionCLI\TextExtraction.exe
    echo UPX compression applied
) || echo UPX not found, skipping compression. Install from https://upx.github.io/

copy /y TextExtractionCLI\TextExtraction.exe "%DIST_DIR%\TextExtraction-win-x64.exe" >nul

echo.
echo Build complete: %DIST_DIR%\TextExtraction-win-x64.exe
for %%A in ("%DIST_DIR%\TextExtraction-win-x64.exe") do echo Size: %%~zA bytes
