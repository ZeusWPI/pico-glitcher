import serial
import time
import signal
import sys
from collections import Counter
import datetime

# glitch calculated at 130 and 690 us

# bound = 33520
bound = 178003
run_distance = 75

start_pulse_width = 6
end_pulse_width = 7 # not inclusive
trials = 10000

class BitArray:
    def __init__(self, size):
        self.size = size
        self.bytearray = bytearray(size)

    def get_bit(self, bit_ix):
        if not isinstance(bit_ix, int):
            raise IndexError("bit array index not an int")

        if bit_ix < 0 or bit_ix >= self.size*8:
            raise IndexError("bit array index out of range")

        byte_ix = bit_ix >> 3
        bit_ix = 7 - (bit_ix & 7)
        return (self.bytearray[byte_ix] >> bit_ix) & 1

    def set_bit(self, bit_ix, val):
        if not isinstance(bit_ix, int):
            raise IndexError("bit array index not an int")

        if bit_ix < 0 or bit_ix >= self.size*8:
            raise IndexError("bit array index out of range")

        if not isinstance(val, int):
            raise ValueError("bit array value not an int")

        if val not in (0, 1):
            raise ValueError("bit array value must be 0 or 1")

        byte_ix = bit_ix >> 3
        bit_ix = 7 - (bit_ix & 7)
        bit_val = 1 << bit_ix

        if val:
            self.bytearray[byte_ix] |= bit_val
        else:
            self.bytearray[byte_ix] &= ~bit_val

    def __getitem__(self, key):
        return self.get_bit(key)

    def __setitem__(self, key, value):
        self.set_bit(key, value)

def add_pulse(bitarray: BitArray, offset: int, pulse_width: int):
    for idx in range(offset, offset+pulse_width):
        bitarray[idx] = 1
    return bitarray

def generate_waveform(array_size: int, *args):
    bitarray = BitArray(array_size)
    for offset, pulse_width in args:
        add_pulse(bitarray, offset, pulse_width)
    # print("WARNING: 0xff bytearray")
    # return bytearray([0xff]*array_size)
    return bitarray.bytearray

start = time.time()
successes = []
success_ctr = Counter()
error_ctr = Counter()

start_datetime = datetime.datetime.now()


def middle_out(middle, amount):
    total = 2*amount - 1
    yield (0, middle)
    for offset in range(1, amount):
        yield ((2*offset + 1) / total, middle + offset)
        yield ((2*offset + 2) / total, middle - offset)

def signal_handler(sig, frame):
    print(f'{successes=}')
    print(f'{success_ctr=}')
    sys.exit(0)

signal.signal(signal.SIGINT, signal_handler)

with serial.Serial('/dev/ttyACM0', 115200) as ser:
    ser.write(b's')
    glitch_size_bytes = int(ser.read_until(b'.')[:-1]) * 4
    print(f'Glitch buffer size = {glitch_size_bytes}')

    ser.write(b'a')
    ser.write([0x63, 0xE0, 0x42, 0x04]) # XOR 0x42 in acc, then increment it
    # if we have a succesful first glitch, we expect acc to be 0x42
    # if we have a succesful second glitch, we expect acc to be 0x01
    assert ser.read(2) == b'a.'

    for pulse_width in range(start_pulse_width, end_pulse_width):
        pulse_work = end_pulse_width - start_pulse_width
        pulse_progress = (pulse_width - start_pulse_width) / pulse_work
        middle_out_work = run_distance*2+1
        for middle_out_progress, offset in middle_out(bound, run_distance):
            ser.write(b'd')
            pulse = generate_waveform(glitch_size_bytes, (offset, pulse_width))
            ser.write(pulse)
            response = ser.read_until(b'.')
            assert response == b'd.', response
            for i in range(trials):
                ser.write(b'g')
                response = ser.read(5)
                if response[1] == 0xb2 or response[3] == 0xb2:
                    successes.append(f'{offset} {pulse_width}: {response}')
                    success_ctr[(offset, pulse_width)] += 1
                    print(f'SUCCESS: {response.hex()}')
                global_progress = (((0.5+i)/trials) / middle_out_work + middle_out_progress) / pulse_work + pulse_progress
                time_spent = (datetime.datetime.now() - start_datetime)
                total_time_estimated = time_spent * 1/global_progress
                print(response.hex(' '), f"success={success_ctr.total()} progress={global_progress*100:>6.2f}%, pw={pulse_width} [{' X'[response[1] == 0xb2]}] [{' X'[response[3] == 0xb2]}] elapsed={time_spent} eta={start_datetime + total_time_estimated}")
            print(f'\npw = {pulse_width} offset = {offset}; {time.time() - start}')
            print(successes)
            print(success_ctr)
