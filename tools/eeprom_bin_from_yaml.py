#!/usr/bin/env python3

# This program converts an input file representing the layout of an eeprom into
# a binary output suitable for writing into the eeprom. The input is a YAML
# (http://yaml.org) file with a number of extra restrictions.
#
# Elements which can appear in the input file:
# * integers: These represent a byte value which should appear in the output.
#   Since they are representing a byte, only values from 0-255 are accepted.
#   Both hexadecimal (eg. 0x1F) and decimal (eg. 31) formatted values are
#   acceptable.
# * strings: A string which appears in the input will be put as a utf-8 encoded
#   value in the output. A null terminator will be appended to the string.
# * blocks: blocks do not appear in the output directly. Instead they are
#   containers for other values. A block is a YAML dictionary that has a size
#   key with an integer value, a content key with a list value, and optionally a
#   fill key with an integer value in the byte range. Size is the number of
#   bytes in the block. Content is the list of items that will be encoded into
#   this block starting at the first byte. Fill is the value that will be put
#   into any unused space at the end of the block. It is illegal for the encoded
#   content to occupy more space than the size of the block.
#
# This program is written in Python 3 and depends on the PyYAML library
# (http://pyyaml.org)

import sys
import yaml
import struct

def write_block(size, content, fill):
    used = write_bin_from_yaml(content)
    if used > size:
        print("Block content is too large for block", file=sys.stderr)
        sys.exit(1)
    sys.stdout.buffer.write(struct.pack('B', fill) * (size - used))
    return size

def write_bin_from_yaml(yaml):
    bytes_written = 0
    if type(yaml) is str:
        encoded = yaml.encode('utf-8')
        sys.stdout.buffer.write(encoded)
        sys.stdout.buffer.write(b'\0')
        bytes_written = len(encoded) + 1
    elif type(yaml) is int:
        if yaml > 255 or yaml < 0:
            print("integer value ({}) is outside of byte range".format(yaml), file=sys.stderr)
            sys.exit(1)
        else:
            sys.stdout.buffer.write(struct.pack('B', yaml))
            bytes_written = 1
    elif type(yaml) is list:
        for e in yaml:
            bytes_written += write_bin_from_yaml(e)
    elif type(yaml) is dict:
        size = None
        content = None
        fill = 0xFF
        for (k, v) in yaml.items():
            if k == 'size':
                if type(v) is not int:
                    print("Block size parameter is not an integer", file=sys.stderr)
                    sys.exit(1)
                size = v
            elif k == 'content':
                content = v
            elif k == 'fill':
                if type(v) is not int:
                    print("Block fill parameter is not an integer", file=sys.stderr)
                    sys.exit(1)
                if v < 0 or v > 255:
                    print("Fill value is not a byte", file=sys.stderr)
                    sys.exit(1)
                fill = v
            else:
                print("Dict has unexpected key {}".format(k), file=sys.stderr)
                exit(1)
        bytes_written = write_block(size, content, fill)
    else:
        print("Element is not an expected type: {}".format(type(yaml)), file=sys.stderr)
        sys.exit(1)

    return bytes_written

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Expected a filename parameter")
        sys.exit(1)

    with open(sys.argv[1], 'r') as yaml_fh:
        yaml_obj = yaml.safe_load(yaml_fh)
        total_bytes_written = write_bin_from_yaml(yaml_obj)
        print("Success - {} bytes written".format(total_bytes_written), file=sys.stderr)
