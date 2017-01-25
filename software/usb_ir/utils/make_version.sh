#!/bin/bash
# What: Create version.h with info from release.h and git
#
HERE=$(dirname $(readlink -f $0))

version=$(git log -1 --format=format:%h)
uncommitted_files=$(expr $(git status --porcelain 2>/dev/null  \
                            | egrep "^(M| M)" | wc -l))
if [ $uncommitted_files -gt 0 ]; then
    version=$version:M
fi
when=$(git log -1 --format=format:%ci | cut -d: -f1,2)
version=$version-$when

cd $HERE/..
make release.h >/dev/null
release=$(sed -rne '/IGUANAIR_RELEASE/s/.*"([0-9\.]+)"/\1/p'  < release.h)

sed -e "s/IGUANAIR_VERSION/$version/" -e "s/IGUANAIR_RELEASE/$release/" \
    < version.h.in > version.h





