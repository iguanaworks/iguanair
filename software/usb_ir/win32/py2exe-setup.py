import sys

from distutils.core import setup
import py2exe

# first ensure that py2exe can find the iguanaIR.py
sys.path.append('Release')

# create the various executables (just iguanaIR-reflasher for now)
setup(console = ['../reflasher/iguanaIR-reflasher'],
      options = { 'py2exe' : { 'dist_dir' : 'Release' }})
