#!/usr/bin/python3

import sys
import serial

def program(inp):
    inp = open(inp, 'rb')
    x = serial.Serial("/dev/ttyUSB0", baudrate=115200)

    # Set starting address to 0
    x.write(b'2\x00\x00')
    result = x.read()
    assert(result == b'K')

    # Erase flash
    print("Erasing...")
    x.write(b'3')
    result = x.read()
    assert(result == b'K')

    while True:
        val = inp.read(1)
        if val == b'':
            break

        x.write(b'4')
        x.write(val)
        result = x.read()
        assert(result == b'K')

        sys.stderr.write('.')
        sys.stderr.flush()

    print("Verifying...")
    # Set starting address to 0
    x.write(b'2\x00\x00')
    result = x.read()
    assert(result == b'K')

    inp.seek(0)
    i = 0
    while True:
        expected = inp.read(1)
        if expected == b'':
            break
        x.write(b'1')
        actual = x.read();

        if expected != actual:
            print("Error at {}, expected {} got {}".format(i, expected, actual))
            break

        sys.stderr.write('.')
        sys.stderr.flush()
        i += 1
    sys.stderr.write('\n')

program(*sys.argv[1:])
