# TICC
Sub-nanosecond time interval counter built on a shield for the
Arduino Mega 2560.

NOTE: Going forward, "main" branch is current development branch.
Each release is in its own branch named with version number.

See the CHANGES.md file for more details about versions.  This
is just a short summary:

September 2025: Major new release that adds/fixes:
* Much greater measurement speed, including binary
  timestamp mode that can do over 1000 measurements/second
  on one channel
* Timestamps don't overflow for 68 years
* Fix to bug in printing long timestamps (misplaced "-" sign)
* Fully rework menu system with submenus and multiple command
  execution for scripting

12 April 2020: New firmware version that adds:
* Change to 11 decimal place (10 ps) rather than 12 place (1 ps)
  resolution to better reflect real performance
* Selectable timestamp rollover
* Changes to support multi-TICC
* Other cleanup

NOTE: To build the TICC firmware, you will need to install the
EnableInterrupt library, which is included in this directory.  The
simplest way to install is to open the Arduino IDE and go to
sketch/Include Library/Add Zip Library and select EnableInterrupts.zip.

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

The TICC is available from https://tapr.org/product/tapr-ticc/.
