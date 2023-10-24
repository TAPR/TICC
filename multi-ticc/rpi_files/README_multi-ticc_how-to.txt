multi-TICC how-to -- 15 November 2020

############################### Hardware Notes #################################

1.  A shorting jumper should be installed on JP3 (near the left
rear of the board) of each TICC to provide 50 ohm termination for the 
clock signal.  

2.  A shorting jumper should be installed on JP1 (near the front right 
of the board) of each TICC to disable auto-reset when a serial connection 
is made.  Note that with JP1 installed the Arduino IDE programmer will 
not work to update the firmware.  Either remove JP1 while updating, or
use the multi-ticc_updater.py program described below, which knows how 
to program with the jumper in place (by sending a properly timed reset
pulse before starting to program).

3.  The multi-TICC draws between 300 and 600 ma at 13.8 volts, depending
on how busy the RPi is and how many LEDs are lit.  The rear panel fuse
is a 2A fast blow.

4.  Only the front or the rear REFERENCE IN BNC connector should be
connected to the input of the four-way splitter.

5.  Shipped with the unit are a power cable and the three 12F675 PicDIV
chips removed from the slave TICCs.

6.  The USB port on the rear panel is not currently used and is there
"just in case".

7.  To remove the top cover of the multi-ticc:

*   Remove the four M5 screws securing the rear panel.
*   Tilt the rear panel back so that the top lip of the
    box is clear.
*   Loosen (BUT DO NOT REMOVE) the eight screws in the top cover.
    Turn them only far enough to loosen!
*   Carefully slide the top cover backwards out of the retaining
    channel.
*   You will note that the top cover screws are *very* short
    (either 4 or 5 mm) and go into square nuts.  The nuts
    slide into the channel.
*   To reassemble, carefully line up the top cover so that the
    first pair of square nuts are just behind the channels.
    Then align the nuts to be square with the channel and slowly slide
    the cover in until the second set of nuts are ready to enter the channel.
    Align them, push the cover further, etc. until the cover is fully in
    place.  Then, replace the rear panel and tighten the four M5 screws.
    Finally, tighten the eight screws on the top.

############################## Operation Notes #################################

1.  The device hostname has been set to "multi-ticc1" with DHCP
turned on for the ethernet port and ssh enabled.  Use login "pi" 
and the appropriate password.  If your client supports avahi 
("zeroconf") you should be able to ssh to the system as 
"multi-ticc1.local".  If not, you'll need to figure out what address 
your DHCP server has assigned.  

2.  WiFi and Bluetooth are disabled to reduce power consumption.  To 
change that, edit /boot/config.txt and comment either or both of 
these lines near the end of the file: 

dtoverlay=disable-wifi
dtoverlay=disable-bt

################################# Software #####################################

1.  When executed, the "multi-ticc_reset.py" program will send a 100 ms reset 
pulse to the TICCs.  The multi-ticc_server.py program automatically does this,
but it might be necessary if you need to reset the TICCs during operation,
for example if the 10 MHz clock source has been removed and reconnected.

2.  To update the firmware on all the TICCs, exit from the server program
with "sudo service multi-ticc_server stop" (so the /dev/ttyTICC* ports are
not in use) and then run "multi-ticc_updater.py <new_firmware_file.hex>"
which will step through the four boards updating each.  Note that new
firmware might require changes to the firmware configuration.

3.  To talk to the TICCs individually, exit from the server program as
described above and then use "multi-ticc_term.sh </dev/ttyTICCx>" where
x is 0 through 3.  This will bring up a very simple terminal program.

4.  Note that at least with the current version of the firmware,
setting the channel names in the config menu doesn't work quite 
correctly with the simple terminal program.  Just typing "N" then 
"C D" will not set both channels.  However, typing "CCC DDD" seems 
to work.  I have no idea why...

5.  When multi-ticc_server.py is running, it will make data available on
nine TCP ports, starting with the defined BASE_PORT (by default 9190) and
incrementing up to BASE_PORT + 9 (by default, 9199).  Each port carries
a separate data stream:

Multiplexed data from all channels on port 9190
Sorted multiplexed data on port 9191
chA on port 9192
chB on port 9193
chC on port 9194
chD on port 9195
chE on port 9196
chF on port 9197
chG on port 9198
chH on port 9199

The sorted stream on port 9191 is intended to provide proper channel
ordering to feed into TimeLab by ensuring that data is output in
channel sequence (e.g., chA followed by chB followed by chC, etc.).
Note that this is experimental and not thoroughly tested.  It uses the 
sorted queue functionality available in Python 3.  To do the sorting, 
the queue has a buffer that needs to fill before any data is output, 
so it will take a little while before you'll see anything.  And, it's 
entirely possible that there will be a bunch of missing or scrambled 
lines of output before it settles down.  It's also entirely possible
that the algorithm might not work properly.  You Have Been Warned.

################################## Firmware ####################################

1.  The firmware loaded into the TICCs is version 20191202.1 which was not
a formal release, but is the one that implements the multi-TICC functionality
and is what I've been using in my system.

2.  The current release version is 20200412.1 which cleans up a few things
and most importantly makes two operational changes: (a) by default the
output data lines now have 11 decimal places (10 ps resolution) instead of
12; and (b) you can set the counter to wrap at a given number of seconds
in timestamp mode.  Default is to keep counting and never wrap.

3.  Version 20200412.1 has been well tested on single TICCs, but hasn't been
used with the multi-ticc.  That's why I didn't install it on your systems.

4.  If you want to update to 20200412.1, the hex file is in the /home/pi
directory and you can update all four boards as described below.
HOWEVER -- you will need to reset and redo the configuration for
master/slave, channel ID, etc., as the config variable storage layout in
EPROM has been changed due to the new command for the (W)rap value.
Some config values have had their name or selection key changed as well.

(NOTE that I've never tried updating firmware on a TICC that is in slave
mode.  It *should* work, but if you have trouble getting into the config
menu you might need to reinstall the 8-pin PIC chip and remove the rear
daisy chain cable to put the board back in master mode, do the programming,
and then reconfigure as slave in the startup menu, then power down, remove
the PIC chip, reconnect the cable, and power cycle.)

############################ Rebuilding the OS #################################

If you need to reinstall the multi-ticc software on a new Raspbian image:

1.  Install Raspbian 
packages you'll need or want (some of these may be installed by default on 
the Raspbian image):

sudo apt-get install python3-serial python3-argparse python3-gpiozero
	 python3-serial avrdude dfu-programmer avrdude-doc

2.  These files, available in the TAPR/TICC github repo, should be in 
the /home/pi directory:
multi-ticc_reset.py        multi-ticc_term.sh            serial-symlinks.rules
multi-ticc_server.py       multi-ticc_updater.py         TICC-20191202.1.hex
multi-ticc_server.service  README_multi-ticc_how-to.txt  ticc_avrdude.conf

3.  It's nice to have permanent device names assigned to each of the
four TICCs such as /dev/ttyTICC0, etc., and the utilities in this
directory depend on that.  Unfortunately the device assigned to each 
physical USB connector isn't consistently set across multiple units.  
But it appears that once you set the ports on a given system they will 
remain the same as long as you don't change anything.

The command "udevadm info /dev/ttyACMx | grep ID_PATH=" will
list an ID for device ttyACMx.  You can then plug that into a 
file called /etc/udev/rules.d/serial-symlinks.rules like so:

###############################################################################
# /etc/udev/rules.d/serial-symlinks.rules
# top left USB port
SUBSYSTEM=="tty",  ENV{ID_PATH}=="platform-3f980000.usb-usb-0:1.2:1.0", \
SYMLINK+="ttyTICC0"
# bottom left USB port
SUBSYSTEM=="tty",  ENV{ID_PATH}=="platform-3f980000.usb-usb-0:1.3:1.0", \
SYMLINK+="ttyTICC1"
# top right USB port
SUBSYSTEM=="tty",  ENV{ID_PATH}=="platform-3f980000.usb-usb-0:1.4:1.0", \
SYMLINK+="ttyTICC2"
# bottom right USB port
SUBSYSTEM=="tty",  ENV{ID_PATH}=="platform-3f980000.usb-usb-0:1.5:1.0", \
SYMLINK+="ttyTICC3"
###############################################################################

and that will assign /dev/ttyTICC0 to the first board, etc. at startup.

4.  Put the file "multi-ticc_server.service" in /lib/systemd/system/
and issue "sudo systemctl enable multi-ticc_server".  This will tell 
the system to start /home/pi/multi-ticc_server.py automatically on 
boot.  To start the server manually, enter
"sudo systemctl start multi-ticc_server".

5.  I recommend (and this is done on the provided SD image) that the
two config files referenced above be located in the /home/pi directory
with symlinks to the correct locations:

    ln -s /home/pi/multi-ticc_server.service \
        /lib/systemd/system/multi-ticc_server.service 

    ln -s /home/piserial-symlinks.rules \
        /etc/udev/rules.d/serial-symlinks.rules

This will make it easier for you to find and edit these files if needed,
and keeps all the multi-ticc files in one place.
    
### END ###
