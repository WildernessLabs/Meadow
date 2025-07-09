#!/usr/bin/env python3
"""
Meadow File Downloader Tool
Repeatedly downloads a file to MCU board using Wilderness Labs CLI
and tracks success/failure statistics.
"""

import subprocess
import threading
import time
import datetime
import argparse
import sys
import re
from datetime import datetime
from datetime import timedelta

BRIEF_PAUSE_BETWEEN_DOWNLOADS = 0.1
POLL_SECONDS_BEFORE_FAILURE = 3
POLL_TOTAL_QUIT_DNLD_COUNT = POLL_SECONDS_BEFORE_FAILURE / BRIEF_PAUSE_BETWEEN_DOWNLOADS

class MeadowFileDownloader:
    def __init__(self, file_path, fileSize):
        self.file_path = file_path
        self.fileSize = fileSize
        self.success_count = 0
        self.failure_count = 0
        self.iteration_count = 0
        self.running = False
        self.download_completed = False
        self.current_percent = 0
        self.prev_percent = 0
        self.pollActiveCount = 0.0
        self.dnldExecutionTime = 0.0
        self.totExecutionTime = 0.0

    def run_cli_command(self):
        """Execute the meadow CLI command and monitor output"""
        cmd = ["meadow", "file", "write", "-f", self.file_path]

        try:
            # Start the process
            process = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,   # stderr added to stdout
                universal_newlines=True,
                bufsize=1
            )

            # Monitor output in real-time
            while True:
                time.sleep(BRIEF_PAUSE_BETWEEN_DOWNLOADS)       # like 100 ms
                self.pollActiveCount = self.pollActiveCount + 1

                if self.pollActiveCount > POLL_TOTAL_QUIT_DNLD_COUNT:
                    print(f"TIMEOUT: No output for > {POLL_SECONDS_BEFORE_FAILURE} seconds - marking as failure", flush=True)
                    process.terminate()
                    return False

                output = process.stdout.readline()

                # Is there stdout text?
                if output != "":
                    # Looks like text
                    self.pollActiveCount = 0
                    self.process_if_percent(output.strip())

                # Check if CLI exited?
                return_code = process.poll()

                # Did CLI return a return code (i.e. it stopped)
                if return_code is None:
                    continue        # No return code, CLI still running

                # Got return code CLI exited, check return code
                if return_code == 0:       # Clean exit?
                    if self.download_completed: # and 100% complete
                        return True         # Success exit
                    else:
                        # CLI exited with no errors but download not completed
                        if self.current_percent == 0:
                            # No CLI update sent yet, just starting
                            continue
                        else:
                            print(f"CLI exited no error, but {self.current_percent}% completed", flush=True)
                            return False
                else:
                    print(f"CLI exited with error code: {-return_code} at {self.current_percent}%", flush=True)
                    return False
                # Continue running

        except FileNotFoundError:
            print("ERROR: 'meadow' CLI not found. Please ensure Wilderness Labs CLI is installed and in PATH.", flush=True)
            return False
        except Exception as e:
            print(f"ERROR: Exception during CLI execution: {e}", flush=True)
            return False

    def process_if_percent(self, output):
        """Parse CLI output to extract download progress"""
        # Look for percentage indicators
        percent_match = re.search(r'(\d+)%', output)
        if percent_match:
            # We found a '%' sign
            percent = int(percent_match.group(1))
            self.current_percent = percent

            if percent == 100:
                print(f"Percent downloaded: {percent}%", end="\r", flush=True)
                self.download_completed = True
                return True     #Found '%'
            else:
                # Only show if different that previous
                if percent != self.prev_percent:
                    self.prev_percent = percent
                    print(f"Percent downloaded: {percent}%", end="\r", flush=True)
                return True     #Found '%'
        else:
            return False

    def display_stats(self):
        """Display current statistics"""
        total = self.success_count + self.failure_count
        success_rate = (self.success_count / total * 100) if total > 0 else 0
        downloadRate = (self.fileSize / self.dnldExecutionTime) / 1024

        print(f"DOWNLOAD STATISTICS - {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        print(f"File: {self.file_path}, size: {self.fileSize/1024:.1f} KB")
        print(f"Iterations: {self.iteration_count}")
        print(f"Successes: {self.success_count}")
        print(f"Failures:  {self.failure_count}")
        print(f"Success Rate: {success_rate:.1f}%")
        print(f"Download Time: {self.dnldExecutionTime:.2f} seconds")
        print(f"Download KB/Second: {downloadRate:.1f} KB/Sec")

    def display_final_stats(self):
        """Display summary statistics at the end"""
        total = self.success_count + self.failure_count
        success_rate = (self.success_count / total * 100) if total > 0 else 0
        averageDownloadRate = ((self.fileSize * self.iteration_count) / self.totExecutionTime) / 1024
        exeTimeMin, exeTimeSec = divmod(self.totExecutionTime, 60)
        exeTimeHour, exeTimeMin = divmod(exeTimeMin, 60)

        print("="*60)
        print(f"FINAL STATISTICS - {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        print(f"File: {self.file_path}, size: {self.fileSize/1024:.1f} KB")
        print(f"Iterations: {self.iteration_count}")
        print(f"Successes: {self.success_count}")
        print(f"Failures:  {self.failure_count}")
        print(f"Success Rate: {success_rate:.1f}%")
        print(f"Total Download Time: {exeTimeHour:1.0f}:{exeTimeMin:02.0f}:{exeTimeSec:02.1f}")
        print(f"Average KB/Second: {averageDownloadRate:.1f} KB/Sec")
        print("="*60, flush=True)
        
    def run_continuous(self):
        """Main loop to continuously download the file"""
        self.running = True
        print(f"Starting continuous download of: {self.file_path}")
        print("Press Ctrl+C to stop...", flush=True)

        try:
            while self.running:

                self.iteration_count += 1
                print("\n" + "="*20, end=" ")
                print(f"Starting iteration {self.iteration_count} ", end=" ")
                print("="*20)
                
                # Execute the download
                startTime = time.perf_counter()
                success = self.run_cli_command()
                endTime = time.perf_counter()

                self.dnldExecutionTime = endTime - startTime
                self.totExecutionTime = self.totExecutionTime + self.dnldExecutionTime

                if self.running:
                    if success:
                        self.success_count += 1
                        print(f"\n✓ Iteration {self.iteration_count} SUCCESS")
                    else:
                        self.failure_count += 1
                        print(f"\n✗ Iteration {self.iteration_count} FAILURE")

                # Display download statistics
                self.display_stats()

                # Pause before next download
                time.sleep(2.0)

        except KeyboardInterrupt:
            print("\nStopping tool (Ctrl+C received)")
            self.dnldExecutionTime = 0.0
            self.iteration_count = self.iteration_count - 1
            self.running = False

        except Exception as e:
            print(f"\nStopping unexpected exception: {e}")
            self.dnldExecutionTime = 0.0
            self.iteration_count = self.iteration_count - 1
            self.running = False

        print("\nFinal Statistics:")
        self.display_final_stats()


def main():
    parser = argparse.ArgumentParser(
        description="Meadow File Downloader - Continuously download files to MCU board",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python meadow_file_downloader.py -f C:\\MyFiles\\firmware.bin
  python meadow_file_downloader.py -f /path/to/file.hex
        """
    )

    parser.add_argument(
        '-f', '--file',
        required=True,
        help='Path to the file to download to the MCU board'
    )

    args = parser.parse_args()
    
    # Validate file path
    import os
    if not os.path.exists(args.file):
        print(f"ERROR: File does not exist: {args.file}")
        sys.exit(1)

    fileSize = os.path.getsize(args.file)
    
    # Create and run the downloader
    downloader = MeadowFileDownloader(args.file, fileSize)
    downloader.run_continuous()

if __name__ == "__main__":
    main()
