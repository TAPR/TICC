Raspberry Pi configuration for multi-TICC

1.  Install Raspbian packages you'll need or want:

sudo apt-get install python3-serial python3-argparse python3-gpiozero
	 python3-serial avrdude dfu-programmer avrdude-doc

2.  Assign permanent names to USB ports by creating
/etc/udev/rules.d/serial-symlinks.rules with the following lines:

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

4.  Using sudo, copy "multi-ticc_server.service" to /lib/systemd/system
and issue "sudo systemctl enable multi-ticc_server".  This will tell
the system to start /home/pi/multi-ticc_server.py automatically on boot.
To start the server manually, enter "sudo systemctl start multi-ticc_server".

5.  When executed, the "multi-ticc_reset.py" program will send a 100 ms reset 
pulse to the TICCs.  The multi-ticc_server.py program automatically does this,
but it might be necessary if you need to reset the TICCs during operation,
for example if the 10 MHz clock source has been removed and reconnected.

