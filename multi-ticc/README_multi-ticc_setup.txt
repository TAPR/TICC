Raspberry Pi configuration for multi-TICC

1.  Assign permanent names to USB ports by creating
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

2.  install the Python3 version of gpiozero:
	sudo apt-get install pyton3-gpiozero

3.  When executed, the "multi-ticc_reset.py" program will send a 100 ms reset 
pulse to the TICCs.

