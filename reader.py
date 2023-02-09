import serial
import struct

# This scripts reads an (unprotected) chip

with open('readout', 'wb') as outfile:
    with serial.Serial('/dev/ttyACM0', 115200) as ser:
        ser.write(b'r')
        for i in range(32768):
            response = ser.read(1)
            outfile.write(response)
    print("done")