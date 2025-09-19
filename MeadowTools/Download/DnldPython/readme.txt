c:/wl/DnldPython/run_downloader.bat c:/wl/DnldPython/<filepath>

c:/wl/DnldPython/run_downloader.bat c:/wl/DnldPython/mscorlib.dll

c:/wl/DnldPython/run_downloader.bat c:/wl/DnldPython/TestFileGen1-26.txt

'meadow port select' to see available ports, if only 1, it is used.

For debugging
-f C:\\HcomFiles\\Write\\Text\\TestFileGen1-26.txt -i 20

For VSCode debugging of meadow_file_downloader.py
launch.json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Python Debugger: Current File with Arguments",
            "type": "debugpy",
            "request": "launch",
            "program": "${file}",
            "console": "integratedTerminal",
            "args": ["-f", "C:\\HcomFiles\\Write\\Text\\TestFileGen1-26.txt"]
        }
    ]
}

Direct download via CLI
meadow file write -f C:\HcomFiles\Code\Issue855\DnldPython\TestFileGen1-26.txt -t /meadow0/TestFileGen1-26.txt
