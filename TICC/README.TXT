This is version 1.0 of the software for the TAPR TICC timestamping
counter.

It is called version 1.0 because it has basic functionality, but the
code is still a mess and requires cleanup/optimization.

This is an Arduino sketch.  To build, unpack all the files into
a folder named TICC and point the Arduino IDE at it.  The TICC shield
(and this software) work with an Arduino Mega 2560 board.

HARDWARE SETUP
The TICC is powered by the USB cable from its host computer.  It requires
a 10 MHz clock signal connected to the "REF IN" SMA jack.  Test signals
are applied to the SMA jacks for channels A and B.

TALKING TO THE TICC
Serial comms with the TICC are at 115200 N81.  All data is 7 bit ASCII.

If you press a key at startup, you will go into a configuration menu.
At present, the only thing that can be set/changed is the mode.  You
may select:

(T) -- Timestamp (print sequential timestamp of A or B if present)
(I) -- Time Interval (print timestamp(B) - timestamp(A) )
(P) -- Period (print timestamp - last_timestamp)
(L) -- TimeLab (print timestamp A, timestamp B, B - A
(D) -- Debug (print raw registers, intermediate results, and timestamp

The mode is written to eeprom so will hold over power cycles/resets.

OTHER CONFIGURATION
Nearly all of the variables that might want configuration are in a
struct that's cleverly called "config".  Calling the function
eeprom_write_config_default() will write default values into the
Arduino EEPROM.  The first byte of the struct is a version number.
At startup, the sketch reads byte 0 of the eeprom and if the version
number does not match, calls eeprom_write_config_default().  Then,
the sketch reads the configuration back into the config struct for use
by the program.

Ultimately the startup config menu will allow you to set any of the
variables.

John Ackermann   N8UR
jra@febo.com
28 October 2016

