TICC Binaries

This directory contains a binary version of the TICC sketch compiled for
the Arduino Mega 2560.  It can be uploaded to the Arduino via USB using the 
"avrdude" software that is available for many platforms.

For Windows users, included in this directory is a utility called 
"XLoader" which I obtained from http://russemotto.com/xloader/.  
It's an easy way to upload the .hex file from a Windows machine 
(at least, it worked on mine).

For Linux or other platform users, included in this directory is a
command line utility called "ticc_updater.py" which will upload the
hex file to an Arduino Mega 2560.  Use it as follows:
    ./ticc_updater /dev/ttyACM0 ./TICC.ino-20200412.1.hex

