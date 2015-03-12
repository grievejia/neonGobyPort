import os
import sys
import string

def invoke(cmd, exit_on_failure = True):
    sys.stderr.write('\n\033[0;34m')
    print >> sys.stderr, cmd
    sys.stderr.write('\033[m')
    ret = os.WEXITSTATUS(os.system(cmd))
    if exit_on_failure and ret != 0:
        sys.exit(ret)
    return ret

def get_libdir():
    installPath = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
    return os.path.join(installPath, 'lib')

def load_plugin(cmd, plugin):
    return string.join((cmd, '-load', get_libdir() + '/' + plugin + '.so'))

def load_all_plugins(cmd):
    cmd = load_plugin(cmd, 'libRCSID')
    cmd = load_plugin(cmd, 'libDynAAUtils')
    cmd = load_plugin(cmd, 'libDynAAInstrumenters')
    cmd = load_plugin(cmd, 'libDynAAAnalyses')
    return cmd

def load_aa(cmd, *aas):
    for aa in aas:
        cmd = string.join((cmd, '-' + aa))
    return cmd
