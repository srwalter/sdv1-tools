#!/usr/bin/python3

import sys
import serial

def dump(num, output):
    output = open(output, 'wb')
    x = serial.Serial("/dev/ttyUSB0", baudrate=115200)
    # Set starting address to 0
    x.write(b'2\x00\x00')
    result = x.read()
    assert(result == b'K')

    for i in range(0, int(num)):
        x.write(b'1')
        output.write(x.read())
        sys.stderr.write('.')
        sys.stderr.flush()

dump(*sys.argv[1:])
