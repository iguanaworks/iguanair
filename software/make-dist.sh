#!/bin/bash
#
# What: create source distribution, requires a git tree.
#
# Usage: make_dist [tree-ish]
#
# runCmake should have been run, since it creates version.h which
# cannot be re-created outside the git tree. The script build also uses
# release.h created by ./runCmake
#
# Contrary to the cmake code, this script only includes sources as
# defined by git (+ version.h, version.h) which makes it less error
# prone.

set -e
cd $(dirname $(readlink -f $0))

treeish=${1:-'HEAD'}
release=$(
    sed -rne '/IGUANAIR_RELEASE/s/.*"([^"]+)".*/\1/p' < usb_ir/release.h )
tarname="IguanaIR-$release"

test -d dist || mkdir dist
git archive --prefix $tarname/ --output dist/$tarname.tar $treeish
tar --append --transform "s|^\.|$tarname|"  -f dist/$tarname.tar \
    ./usb_ir/version.h ./usb_ir/release.h
gzip -f dist/$tarname.tar
sha256sum dist/$tarname.tar.gz > dist/$tarname.sum

ls -l dist/$tarname.tar.gz
