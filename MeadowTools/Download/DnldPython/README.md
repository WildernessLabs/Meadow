# Meadow File Downloader Tool

A Python tool for repeatedly downloading files to MCU boards using the Wilderness Labs CLI and tracking success/failure statistics.

## Features

- Continuously downloads files using `meadow file write -f <FilePath>` command
- Monitors CLI output in real-time for download progress
- Tracks success/failure statistics and displays them after each iteration
- Detects timeouts (>2 seconds without output) and marks them as failures
- Detects successful completion when "100%" is received
- Automatic 2-second pause between download attempts
- Clean exit with Ctrl+C

## Requirements

- Python 3.6 or higher
- Wilderness Labs CLI installed and available in PATH
- Windows operating system

## Installation

1. Ensure Python 3.6+ is installed
2. Ensure Wilderness Labs CLI is installed and `meadow` command is available in PATH
3. The tool is already placed in `C:\HcomFiles\Write\DnldPython\`

## Usage

```bash
python meadow_file_downloader.py -f <path_to_file>
```

### Examples

```bash
# Download a firmware file
python meadow_file_downloader.py -f C:\MyFiles\firmware.bin

# Download a hex file
python meadow_file_downloader.py -f C:\Projects\myapp.hex
```

## Output

The tool displays:
- Real-time CLI output from the meadow command
- Progress indicators showing download percentages
- Success/failure status for each iteration
- Running statistics including:
  - Total iterations
  - Number of successes
  - Number of failures
  - Success rate percentage
  - Current timestamp

### Sample Output

```
Starting continuous download of: C:\MyFiles\firmware.bin
Press Ctrl+C to stop...

--- Starting iteration 1 ---
CLI Output: Connecting to device...
CLI Output: Starting file transfer...
CLI Output: Progress: 25%
CLI Output: Progress: 50%
CLI Output: Progress: 75%
CLI Output: Progress: 100%
✓ Download completed successfully (100%)
✓ Iteration 1 SUCCESS

============================================================
STATISTICS - 2025-06-30 14:30:25
============================================================
File: C:\MyFiles\firmware.bin
Iterations: 1
Successes: 1
Failures:  0
Success Rate: 100.0%
============================================================

Waiting 2 seconds before next iteration...
```

## Error Handling

- **CLI not found**: If the `meadow` command is not available, the tool will display an error message
- **File not found**: If the specified file doesn't exist, the tool will exit with an error
- **Timeout**: If no output is received for >2 seconds, the iteration is marked as a failure
- **Interruption**: Ctrl+C cleanly stops the tool and displays final statistics

## Troubleshooting

1. **"meadow command not found"**: Ensure Wilderness Labs CLI is installed and added to your system PATH
2. **File permission errors**: Ensure the file you're trying to download exists and is readable
3. **Connection issues**: Ensure your MCU board is properly connected and recognized by the meadow CLI

## Technical Details

- Uses `subprocess.Popen` to execute CLI commands with real-time output monitoring
- Implements timeout detection using timestamp comparison
- Uses regular expressions to parse percentage indicators from CLI output
- Handles process termination gracefully on timeout or interruption
