#!/bin/sh
export DEBEMAIL="support@iguanaworks.net"
export DEBFULLNAME="IguanaWorks"
LIRCTMP=/tmp/lirc-temp
mkdir $LIRCTMP
pushd $LIRCTMP
apt-get source lirc
cd lirc-*
dch -l -iguana "Compiled with support for IguanaWorks USB IR Transceiver (driver: iguanaIR)"
fakeroot debian/rules binary
popd
cp $LIRCTMP/lirc_*.deb .
rm -rf $LIRCTMP
