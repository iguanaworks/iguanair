#!/bin/sh
#
# What: create source distribution, requires a git tree.
#
# runCmake # should have been run, since it creates version.h which
# cannot be re-created outside the git tree.
#

if  test -n "$(git status -s)" ; then
    echo "Warning: the directory is not clean. git commit/stash/clean?" >&2
    git status -s
fi

release=$( sed -rne '/IGUANAIR_RELEASE/s/.*"([^"]+)".*/\1/p' < release.h )
tarname="IguanaIR-$release"
test -d dist || mkdir dist
tar czf dist/$tarname.tar.gz \
    --exclude='dist/*' --exclude='build/*' --exclude='.git*' \
    --transform "s|^\.|$tarname|" .
ls -l dist/$tarname.tar.gz
