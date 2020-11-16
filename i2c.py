#!/usr/bin/python3

import sys
import serial

def program(addr, subaddr, val=None):
    x = serial.Serial("/dev/ttyUSB0", baudrate=115200)

    if val is None:
        print("Reading")
        x.write(b'r')
    else:
        print("Writing")
        x.write(b'w')

    # Set starting address to 0
    addr = int(addr, 16).to_bytes(1, byteorder='big')
    x.write(addr)
    subaddr = int(subaddr, 16).to_bytes(1, byteorder='big')
    x.write(subaddr)

    if val:
        x.write(int(val, 16).to_bytes(1, byteorder='big'))

    result = x.read()
    print(result)

program(*sys.argv[1:])
