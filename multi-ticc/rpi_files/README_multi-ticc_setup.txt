Raspberry Pi configuration for multi-TICC -- 09 November 2020

0.  The device hostname has been set to "multi-ticc1" with DHCP
turned on for the ethernet port.  Use login "pi" and password 
"n8ga2020" (which you of course should change).  If your client 
supports avahi ("zeroconf") you should be able to ssh to the 
system as "multi-ticc1.local".  If not, you'll need to figure out 
what address your DHCP server has assigned.  

WiFi and Bluetooth are disabled.  To change that, edit /boot/config.txt
and comment either or both of these lines near the end of the file:

dtoverlay=disable-wifi
dtoverlay=disable-bt

1.  A shorting jumper should be installed on JP3 (near the left rear 
of the board) to provide 50 ohm termination for the clock signal.  
A shorting jumper should be installed on JP1 (near the front right 
of the board) to disable auto-reset when a serial connection is made.
Note that with JP1 installed the Arduino IDE programmer will not work
to update the firmware.  Either remove JP1 while updating, or use the
multi-ticc_updater.py program described below, which knows how to
program with the jumper in place.

2.  Install Raspbian packages you'll need or want (some of these
may be installed by default on the Raspbian image):

sudo apt-get install python3-serial python3-argparse python3-gpiozero
	 python3-serial avrdude dfu-programmer avrdude-doc

3.  These files should be in the /home/pi directory:
multi-ticc_reset.py        multi-ticc_term.sh           serial-symlinks.rules
multi-ticc_server.py       multi-ticc_updater.py        TICC-20191202.1.hex
multi-ticc_server.service  README_multi-ticc_setup.txt  ticc_avrdude.conf

4.  It's nice to have permanent device names assigned to each of the
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

5.  Put the file "multi-ticc_server.service" in /lib/systemd/system/
and issue "sudo systemctl enable multi-ticc_server".  This will tell 
the system to start /home/pi/multi-ticc_server.py automatically on 
boot.  To start the server manually, enter
"sudo systemctl start multi-ticc_server".

6.  I recommend (and this is done on the provided SD image) that the
two config files referenced above be located in the /home/pi directory
with symlinks to the correct locations:

    ln -s /home/pi/multi-ticc_server.service \
        /lib/systemd/system/multi-ticc_server.service 

    ln -s /home/piserial-symlinks.rules \
        /etc/udev/rules.d/serial-symlinks.rules

This will make it easier for you to find and edit these files if needed,
and keeps all the multi-ticc files in one place.
    
7.  When executed, the "multi-ticc_reset.py" program will send a 100 ms reset 
pulse to the TICCs.  The multi-ticc_server.py program automatically does this,
but it might be necessary if you need to reset the TICCs during operation,
for example if the 10 MHz clock source has been removed and reconnected.

8.  To update the firmware on all the TICCs, exit from the server program
with "sudo service multi-ticc_server stop" (so the /dev/ttyTICC* ports are
not in use) and then run "multi-ticc_updater.py <new_firmware_file.hex>"
which will step through the four boards updating each.  Note that new
firmware might require changes to the firmware configuration.

9.  To talk to the TICCs individually, exit from the server program as
described above and then use "multi-ticc_term.sh </dev/ttyTICCx>" where
x is 0 through 3.  This will bring up a very simple terminal program.

10.  Note that at least with the current version of the firmware,
setting the channel names in the config menu doesn't work quite 
correctly with the simple terminal program.  Just typing "N" then 
"C D" will not set both channels.  However, typing "CCC DDD" seems 
to work.  I have no idea why...

11.  When multi-ticc_server.py is running, it will make data available on
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

### END ###
