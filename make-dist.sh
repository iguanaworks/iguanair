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

usb_ir=software/usb_ir


# handle errors by just exiting
set -E
trap 'exit' ERR

# directory prep
cd $(dirname $(readlink -f $0))
rm -rf dist
mkdir dist

# default to tree "HEAD"
treeish=${1:-'HEAD'}

# test that treeish is valid before proceeding
git describe --always --tags ${treeish} > /dev/null

# read the release name from CMakeLists.txt and create tarname
release=$( git archive ${treeish} software/usb_ir/CMakeLists.txt |
               tar -x --to-stdout |
               sed -rne '/Set\(FULLVER/s/[^ ]+ ([^\)]+)\).*/\1/p' )
tarname=${TARNAME:-"iguanaIR-$release"}

# create a complete archive of the selected tree
git archive --prefix ${tarname}/ --output dist/${tarname}.tar ${treeish}

# complain if the git tag does not match the FULLVER line
git_version=$( git describe --always --tags ${treeish} )
git_release=$( git describe --always --abbrev=0 --tags ${treeish} )
if [ "${release}" != "${git_release}" ]; then
    echo "CMakeLists.txt FULLVER (${release}) differs from git tag (${git_release}) for ${treeish}"

    git_version="${release}!=${git_version}"
    git_release="${release}!=${git_release}"
fi

# generate a version.h similarly to how CMake does it
tar -xf dist/${tarname}.tar ${tarname}/${usb_ir}/version.h.in --to-stdout > /dev/null
tar -xf dist/${tarname}.tar ${tarname}/${usb_ir}/version.h.in --to-stdout |
    sed "s|\${GIT_VER_STR}|${git_version}|" |
    sed "s|\${GIT_REL_STR}|${git_release}|" > version.h
echo "Packaging ${git_version}"

# remove version.h.in and add version.h into the tarball
tar --delete --file dist/${tarname}.tar ${tarname}/${usb_ir}/version.h.in
tar --remove-files --append --transform "s|^|${tarname}/${usb_ir}/|" \
    -f dist/${tarname}.tar version.h

# unpack the ChangeLog we use as input
tar -xf dist/${tarname}.tar ${tarname}/ChangeLog --to-stdout > dist/ChangeLog

# generate the iguanair.spec with the proper %changelog content and put it in the tarball
tar -xf dist/${tarname}.tar ${tarname}/mkChangelog --to-stdout > dist/mkChangelog
tar -xf dist/${tarname}.tar ${tarname}/fedora/iguanair.spec.in --to-stdout > dist/iguanair.spec.in
python dist/mkChangelog --build 1 --append-to-spec dist/iguanair.spec.in > dist/iguanair.spec
tar --remove-files --append --transform "s|^dist|${tarname}/fedora|" \
    -f dist/${tarname}.tar dist/iguanair.spec
rm dist/iguanair.spec.in

# write a selection debian changelog files and put the first into the tarball
python dist/mkChangelog --build 1 --debian-release unstable --output dist/changelog
tar --delete --file dist/${tarname}.tar ${tarname}/debian/changelog.in
tar --remove-files --append --transform "s|^dist|${tarname}/debian|" \
    -f dist/${tarname}.tar dist/changelog

python dist/mkChangelog --build 1 --debian-release kinetic  --output dist/changelog.kinetic-1
python dist/mkChangelog --build 1 --debian-release jammy  --output dist/changelog.jammy-1
python dist/mkChangelog --build 1 --debian-release focal  --output dist/changelog.focal-1
#python dist/mkChangelog --build 1 --debian-release stable --output dist/changelog.stable-1
#python dist/mkChangelog --build 1 --debian-release cosmic  --output dist/changelog.cosmic-1
python dist/mkChangelog --build 1 --debian-release bionic  --output dist/changelog.bionic-1
python dist/mkChangelog --build 1 --debian-release xenial --output dist/changelog.xenial-1
python dist/mkChangelog --build 1 --debian-release trusty --output dist/changelog.trusty-1
#python dist/mkChangelog --build 1 --debian-release disco --output dist/changelog.disco-1
#python dist/mkChangelog --build 1 --debian-release eoan --output dist/changelog.eoan-1
#python dist/mkChangelog --build 1 --debian-release focal --output dist/changelog.focal-1
#python dist/mkChangelog --build 1 --debian-release groovy --output dist/changelog.groovy-1
#python dist/mkChangelog --build 1 --debian-release hirsute --output dist/changelog.hirsute-1

# cleanup
rm -f dist/ChangeLog dist/mkChangelog

# gzip, and sum it
gzip -f dist/${tarname}.tar
sha256sum dist/* > dist/${tarname}.sum

ls -l dist
