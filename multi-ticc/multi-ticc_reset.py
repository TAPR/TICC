#!/usr/bin/env python3

from gpiozero import OutputDevice
from time import sleep

pin21 = OutputDevice(21)

print("Sending reset pulse...")
pin21.on()
sleep(0.1)
pin21.off()

pin21.close

