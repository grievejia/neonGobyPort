#!/usr/bin/env python

import argparse
import ng_utils

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description = 'Add tracing code for pointers')
    parser.add_argument('prog', help = 'the program name (e.g. mysqld.bc)')
    parser.add_argument('--diagnose',
                        help = 'instrument for test case reduction and trace slicing (False by default)',
                        action = 'store_true',
                        default = False)
    args = parser.parse_args()

    if args.prog.endswith('.ll') or args.prog.endswith('.bc'):
        progName = args.prog[:-3]
    else:
        progName = args.prog
    instrumented_bc = progName + '.inst.bc'
    instrumented_exe = progName + '.inst'

    cmd = ng_utils.load_all_plugins('opt')
    # Preparer doesn't preserve IDAssigner, so we put it after
    # -instrument-memory.
    cmd = ' '.join((cmd, '-instrument-memory', '-prepare'))
    if args.diagnose:
        cmd = ' '.join((cmd, '-diagnose'))
    cmd = ' '.join((cmd, '-o', instrumented_bc))
    cmd = ' '.join((cmd, '<', args.prog))
    ng_utils.invoke(cmd)

    cmd = ' '.join(('clang++', instrumented_bc,
                    ng_utils.get_libdir() + '/libDynAAMemoryHooks.a',
                    '-o', instrumented_exe))
    # Memory hooks use pthread functions.
    linking_flags = ['-pthread']
    cmd = ' '.join((cmd, ' '.join(linking_flags)))
    ng_utils.invoke(cmd)
