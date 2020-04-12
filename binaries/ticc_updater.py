#!/usr/bin/env python3

# ticc_uploader.py
# this script will upload a hexfile to an Arduino Mega2560
# syntax: ./ticc_updater /dev/ttyACM0 ./TICC.ino-20200412.1.hex

# REQUIRES avdrdude package to be installed

# sends command: avrdude -v -patmega2560 -cwiring \
#        -P<comport> -b115200 -D -Uflash:w:<hexfile>:i

import sys
import subprocess
import shlex
from time import sleep

comport = sys.argv[1]
hexfile = sys.argv[2]

cmd = 'avrdude -v -patmega2560 -cwiring -b115200 -D -P' + comport + \
    ' -Uflash:w:' + hexfile + ':i'

subprocess.run(shlex.split(cmd))
