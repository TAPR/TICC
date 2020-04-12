# TICC
Sub-nanosecond time interval counter built on a shield for the
Arduino Mega 2560.

12 April 2020: New firmware version that adds:

* Change to 11 decimal place (10 ps) rather than 12 place (1 ps)
  resolution to better reflect real performance
* Selectable timestamp rollover
* Changes to support multi-TICC
* Other cleanup

The TICC is a circuit board mated to an Arduino Mega 2560 controller that 
allows very high resolution time interval measurements -- down to less than 
60 picoseconds (trillionths of a second).  

The software here runs on the Mega 2560 and controls the shield.  Measurement 
data is output in ASCII form on the Arduino's USB port.

Source files are contained in the "TICC" folder which is an Arduino
sketch folder.

Binary files as well as a Windows-based upload tool are contained in
the "binaries" folder.

A user's manual is contained in the "docs" folder.

The TICC is available from http://tapr.org/kits_ticc.html.
