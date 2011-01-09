import os
import sys
import glob
import shutil

from distutils.core import setup
import py2exe

# first ensure that py2exe can find the iguanaIR.py
sys.path.append('Release')

# create the various executables (just iguanaIR-reflasher for now)
setup(console = ['../reflasher/iguanaIR-reflasher'],
      options = { 'py2exe' : { 'dist_dir' : 'Release' }})

# copy the hex files we need (but not he devel ones)
shutil.rmtree('Release/hex', True)
shutil.copytree('../reflasher/hex', 'Release/hex')
shutil.rmtree('Release/hex/.svn')
for devel in glob.glob('Release/hex/*-0.hex'):
    os.unlink(devel)
