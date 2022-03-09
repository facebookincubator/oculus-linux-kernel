#!/usr/bin/env python3

# Python port of the Kconfig.versions generation script in Makefile. This takes
# a kernel version and generates a config script which enables the backports
# files for all newer kernel versions. e.g. for 4.9, this will output
# config variables KERNEL_4_10 through KERNEL_4_99.

import argparse
import sys

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('version', type=str, help='Kernel version number')
    args = parser.parse_args()

    version = tuple(map(int, args.version.split('.')))
    if len(version) < 2:
        print('Version must include major and minor numbers', file=sys.stderr)
        return 1

    major = version[0]
    for minor in range(version[1] + 1, 100):
        print('config KERNEL_{major}_{minor}'.format(major=major, minor=minor))
        print('    def_bool y')

    return 0

if __name__ == '__main__':
    sys.exit(main())
