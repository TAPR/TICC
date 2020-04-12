Raspberry Pi configuration for multi-TICC

0.  This assumes all working files are installed in the user's home
directory (typically /home/pi).  Slight changes may be necessary if
you install them elsewhere.

1.  Install Raspbian packages you'll need or want:

    sudo apt-get install python3-serial python3-argparse python3-gpiozero
	    python3-serial avrdude dfu-programmer avrdude-doc

2.  Assign permanent names to USB ports by creating
    /etc/udev/rules.d/serial-symlinks.rules with the following lines:

    # top left USB port
    SUBSYSTEM=="tty", ENV{ID_PATH}=="platform-3f980000.usb-usb-0:1.2:1.0", \
        SYMLINK+="ttyTICC0"
    # bottom left USB port
    SUBSYSTEM=="tty", ENV{ID_PATH}=="platform-3f980000.usb-usb-0:1.3:1.0", \
        SYMLINK+="ttyTICC1"
    # top right USB port
        SUBSYSTEM=="tty", ENV{ID_PATH}=="platform-3f980000.usb-usb-0:1.4:1.0", \
        SYMLINK+="ttyTICC2"
    # bottom right USB port
    SUBSYSTEM=="tty", ENV{ID_PATH}=="platform-3f980000.usb-usb-0:1.5:1.0", \
        SYMLINK+="ttyTICC3"

4.  Using sudo, copy "multi-ticc_server.service" to /lib/systemd/system
and issue "sudo systemctl enable multi-ticc_server".  This will tell
the system to start /home/pi/multi-ticc_server.py automatically on boot.
To start the server manually, enter "sudo systemctl start multi-ticc_server".

5.  When executed, the "multi-ticc_reset.py" program will send a 100 ms reset 
pulse to the TICCs.  The multi-ticc_server.py program automatically does this,
but it might be necessary if you need to reset the TICCs during operation,
for example if the 10 MHz clock source has been removed and reconnected.

The python programs in this directory include some utilities to make it
easier to manage the system.  They are intended to be installed in the
user's home directory or in /usr/local/bin on the Raspberry Pi.  Here
is a brief description:


multi-ticc_term.sh -- opens a simple terminal program for the specified TICC
multi-ticc_reset.py -- reset all TICCs
multi-ticc_updater.py -- update firmware on all TICCs.  Requires avrdude.
multi-ticc_server.py -- the main server program; listens to TICCs on USB,
    outputs data on ethernet.
