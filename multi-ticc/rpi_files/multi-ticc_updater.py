#!/usr/bin/env python3

# CONFFILE=/home/pi/ticc_avrdude.conf
# avrdude -C$CONFFILE -v -patmega2560 -cwiring \
#        -P/dev/ttyTICC0 -b115200 -D -Uflash:w:$1:i

import sys
import subprocess
import shlex
import gpiozero
import time

pin21 = gpiozero.OutputDevice(21)
conffile = '/home/pi/ticc_avrdude.conf'
hexfile = sys.argv[1]

for comport in \
    ['/dev/ttyACM0','/dev/ttyACM1','/dev/ttyACM2','/dev/ttyACM3']:
    cmd = 'avrdude -v -patmega2560 -cwiring -b115200 -D -P' + comport + ' -C' \
        + conffile + ' -Uflash:w:' + hexfile + ':i'

    pin21.on()
    time.sleep(0.01)
    pin21.off()
    subprocess.run(shlex.split(cmd))

pin21.off()
pin21.close

