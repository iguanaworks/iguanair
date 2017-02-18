#!/bin/bash
#
# What: create source distribution, requires a git tree.
#
# Usage: [TARNAME=...] make_dist [tree-ish]
#
# runCmake should have been run, since it creates version.h which
# cannot be re-created outside the git tree.
#
# Contrary to the cmake code, this script only includes sources as
# defined by git (+ version.h) which makes it less error prone.

set -e
cd $(dirname $(readlink -f $0))

treeish=${1:-'HEAD'}
release=$( sed -rne '/Set\(FULLVER/s/[^ ]+ ([^\)]+)\).*/\1/p' \
               < software/usb_ir/CMakeLists.txt )
tarname=${TARNAME:-"IguanaIR-$release"}

test -d dist || mkdir dist
git archive --prefix $tarname/ --output dist/$tarname.tar $treeish
tar --append --transform "s|^\.|$tarname|"  -f dist/$tarname.tar \
    ./software/usb_ir/version.h
gzip -f dist/$tarname.tar
sha256sum dist/$tarname.tar.gz > dist/$tarname.sum

ls -l dist/$tarname.tar.gz
