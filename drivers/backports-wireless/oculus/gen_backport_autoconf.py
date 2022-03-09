#!/usr/bin/env python3

# Python port of the autoconf.h generation script in Makefile.real. This takes
# the final .config file, filters it, and generates the autoconf.h header file
# which C code can use to check the configuration.

import argparse
import sys

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-l', dest='local_symbols_path', type=str,
                        default='local-symbols',
                        help='Path to the local-symbols file')
    parser.add_argument('config_path', type=str,
                        help='Path to the backports .config file')
    args = parser.parse_args()

    print('#ifndef COMPAT_AUTOCONF_INCLUDED')
    print('#define COMPAT_AUTOCONF_INCLUDED')
    print('/*')
    print(' * Automatically generated file, don\'t edit!')
    print(' * Changes will be overwritten')
    print(' */')
    print()

    # local-symbols is normally a list of patterns to pass to grep, but it's
    # easier to just read it as a set of variable names.
    local_symbols = set()
    with open(args.local_symbols_path) as f:
        local_symbols.update(l.rstrip('=\n') for l in f)

    # Output a #define for each variable in our .config which is part of
    # local-symbols.
    with open(args.config_path) as f:
        for line in f:
            if not line.startswith('CPTCFG_'):
                continue
            equals = line.find('=')
            if equals < 0:
                continue
            name = line[7:equals]
            val = line[(equals + 1):].rstrip('\n')
            # Some variables have BACKPORTED_ versions that we also need.
            symbol_name = name[11:] if name.startswith('BACKPORTED_') else name
            if symbol_name not in local_symbols:
                continue
            if val == 'y':
                print('#define CPTCFG_{name} 1'.format(name=name))
            elif val == 'm':
                print('#define CPTCFG_{name}_MODULE 1'.format(name=name))

    print('#endif /* COMPAT_AUTOCONF_INCLUDED */')
    return 0

if __name__ == '__main__':
    sys.exit(main())
