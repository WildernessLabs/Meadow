@echo off
REM Meadow File Downloader Batch Script
REM Usage: run_downloader.bat <path_to_file>

if "%1"=="" (
    echo Usage: run_downloader.bat ^<path_to_file^>
    echo Example: run_downloader.bat C:\MyFiles\firmware.bin
    pause
    exit /b 1
)

echo Starting Meadow File Downloader...
echo File: %1
echo.

python "%~dp0meadow_file_downloader.py" -f "%1"

echo.
echo Downloader finished.
pause
