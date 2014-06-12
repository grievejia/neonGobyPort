import os
import sys
import string
import rcs_utils

def load_all_plugins(cmd):
    cmd = rcs_utils.load_all_plugins(cmd)
    cmd = rcs_utils.load_plugin(cmd, 'libDynAAUtils')
    cmd = rcs_utils.load_plugin(cmd, 'libDynAAAnalyses')
    cmd = rcs_utils.load_plugin(cmd, 'libDynAACheckers')
    cmd = rcs_utils.load_plugin(cmd, 'libDynAAInstrumenters')
    cmd = rcs_utils.load_plugin(cmd, 'libDynAATransforms')
    #cmd = rcs_utils.load_plugin(cmd, 'libTunableCFS')
    #return cmd
    return string.join((cmd, '-load', '~/Research/tcfs/debugBuild/Debug+Asserts/lib/libAnders.so', '-load', '~/Research/tcfs/debugBuild/Debug+Asserts/lib/libTPA.so', '-load', '~/Research/tcfs/debugBuild/Debug+Asserts/lib/libSemiSparseTPA.so', '-load', '~/Research/tcfs/debugBuild/Debug+Asserts/lib/libSparseTPA.so'))

def load_aa(cmd, *aas):
    for aa in aas:
        
        cmd = string.join((cmd, '-' + aa))
    return cmd

def supports_intra_proc_queries_only(aa):
    return aa == 'basicaa' or aa == 'ds-aa'

def get_aa_choices():
    return ['tbaa', 'basicaa', 'no-aa', 'ds-aa', 'anders-aa', 'bc2bdd-aa',
            'su-aa', 'scev-aa', 'taa', 'staa', 'sstaa']
