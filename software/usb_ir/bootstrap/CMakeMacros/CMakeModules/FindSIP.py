# FindSIP.py
#
# Copyright (c) 2007, Simon Edwards <simon@simonzone.com>
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

import sys
import posixpath
import os

try:
    import sipconfig
except:
    sys.exit(-1)

def dos2posix(original):
    if os.name != 'nt':
        return original
    drive, path = os.path.splitdrive(original)
    splits = [drive]
    pathsplits = list(os.path.split(path))
    if pathsplits[0][0] == '\\':
        pathsplits[0] = pathsplits[0][1:]
    splits.extend(pathsplits)
    return posixpath.join(*splits)

sipcfg = sipconfig.Configuration()
print("sip_version:%06.0x" % sipcfg.sip_version)
print("sip_version_str:%s" % sipcfg.sip_version_str)
print("sip_bin:%s" % dos2posix(sipcfg.sip_bin))
print("default_sip_dir:%s" % dos2posix(sipcfg.default_sip_dir))
print("sip_inc_dir:%s" % dos2posix(sipcfg.sip_inc_dir))
