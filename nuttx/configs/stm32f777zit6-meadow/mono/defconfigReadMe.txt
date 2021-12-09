'defconfig.ethernet' contains a working Ethernet configuation the works on
    the early F7-embedded breakout board.
'defconfig.SDCard+NSH' contains NSH, working SDCard configuration with DMA
    enabled and no special Nuttx debugging enabled. In source code look for
    'PeterM' to see how to require CLI to send the mount information with
    the file name (e.g. /meadow0/filename.txt instead of filename.txt) to
    download.
