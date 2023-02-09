import serial

# This script reads the serial number and status from the board
# Also used to check how reliable the communication is, without glitching

s = 0
f = 0
statusmapping = {0xb6: "locked", 0xb2: "unlocked"}
with serial.Serial('/dev/ttyACM0', 115200) as ser:
    while True:
        ser.write(b'b')
        response = ser.read(4)
        if response[:2] == b'\x81\x04':
            s += 1
            print(statusmapping.get(response[2], f"unknown status! {hex(response[2])}"))
        else:
            print(response)
            f += 1
        print(s/(s+f))
