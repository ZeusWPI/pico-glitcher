import serial
import time
import signal
import sys
import struct

# This script will wipe the microcontroller, re-enabling the debug interface

with serial.Serial('/dev/ttyACM0', 115200) as ser:
    ser.write(b'b')
    response = ser.read(4)
    print(response)
    print(f"debug locked: {response[3] & (1<<2)}")
    
    ser.write(b'p')
    ser.write(struct.pack('>Q', 1000))
    response = ser.read(1)
    print(response)
    print(f"debug locked: {response[0] & 0x04}")
