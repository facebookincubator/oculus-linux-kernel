#!/usr/bin/env python3

# Python port of the Kconfig.kernel generation script in Makefile. This takes
# the kernel's .config file, filters it, and generates a config script with all
# default values set correctly.

import argparse
import sys

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-l', dest='local_symbols_path', type=str,
                        default='local-symbols',
                        help='Path to the local-symbols file')
    parser.add_argument('config_path', type=str,
                        help='Path to the kernel .config file')
    args = parser.parse_args()

    # local-symbols is normally a list of patterns to pass to grep, but it's
    # easier to just read it as a set of variable names.
    local_symbols = set()
    with open(args.local_symbols_path) as f:
        local_symbols.update(l.rstrip('=\n') for l in f)

    # Output a config statement for each variable which is not in
    # local-symbols.
    with open(args.config_path) as f:
        for line in f:
            if not line.startswith('CONFIG_'):
                continue
            equals = line.find('=')
            if equals < 0:
                continue
            name = line[7:equals]
            val = line[(equals + 1):].rstrip('\n')
            if name in local_symbols:
                continue
            kind = {'m': 'tristate', 'y': 'bool'}.get(val)
            if kind is None:
                continue
            print('config {name}'.format(name=name))
            print('    {kind}'.format(kind=kind))
            print('    default {val}'.format(val=val))
            print()

    return 0

if __name__ == '__main__':
    sys.exit(main())
