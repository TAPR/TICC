#!/usr/bin/env python3

# CONFFILE=/home/jra/ticc_avrdude.conf
# avrdude -C$CONFFILE -v -patmega2560 -cwiring \
#        -P/dev/ttyTICC0 -b115200 -D -Uflash:w:$1:i

import sys
import subprocess
import shlex
from gpiozero import OutputDevice
from time import sleep

pin21 = OutputDevice(21)
comport = '/dev/ttyTICC0'
hexfile = sys.argv[1]

for comport in \
    ['/dev/ttyTICC0','/dev/ttyTICC1','/dev/ttyTICC2','/dev/ttyTICC3']:
    cmd = 'avrdude -v -patmega2560 -cwiring -b115200 -D -P' + comport + \
        ' -Uflash:w:' + hexfile + ':i'

    pin21.on()
    sleep(0.01)
    pin21.off()
    subprocess.run(shlex.split(cmd))

pin21.off()
pin21.close

