# TICC
Sub-nanosecond time interval counter built on a shield for the
Arduino Mega 2560.  The TICC is available from 
https://tapr.org/product/tapr-ticc/.

NOTE: Going forward, the "main" brach is the current 
development branch.  Each release is in its own branch 
named with version number.

The TICC is a circuit board mated to an Arduino Mega 2560 
controller that allows very high resolution time interval 
measurements -- down to less than 60 picoseconds (trillionths 
of a second).  

The software in this repo runs on the Mega 2560 and controls 
the shield.  Measurement data is output in ASCII form on the 
Arduino's USB port.  In the latest version, there is also a 
high-speed mode that outputs binary data at nearly double the 
rate of the ASCII output.

Source files are contained in the "TICC" folder which is an 
Arduino sketch folder.

Binary ".hex" files are in the binaries/ folder.  Several upload
tools for Windows, Linux, and Mac are in the bin/ folder.  
Instructions for using one of them, AVRDUDESS, will be in an
appendix to the updated User Guide which will be in the docs/ folder
as soon as it's ready.

See the CHANGES.md file for more details about versions.  This
is just a short summary:

September 2025: Major new release that adds/fixes:
* Much greater measurement speed, including binary
  timestamp mode that can do over 1000 measurements/second
  on one channel
* Timestamps don't overflow for 68 years
* Fix to bug in printing long timestamps (misplaced "-" sign)
* Fully reworked menu system with submenus and multiple command
  execution for scripting

12 April 2020: New firmware version that adds:
* Change to 11 decimal place (10 ps) rather than 12 place (1 ps)
  resolution to better reflect real performance
* Selectable timestamp rollover
* Changes to support multi-TICC
* Other cleanup

