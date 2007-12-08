#!/bin/sh
rm -r debinary
rm -r debinary64
rm -r pdebinary
rm -r pdebinary64

#./configure
make clean
make
##Set Revision Number, default is 0
if [ "$1" == "" ]; then
REV=0

else
REV="$1"
fi

ARCH=`uname -m | sed 's/i\([456]\)86/i386/'`


#directory for making the .deb
BASE=debinary
PBASE=pdebinary
BASE64=debinary64
PBASE64=pdebinary64
version=`grep '^Version:' iguanaIR.spec | sed 's/^Version:\s*//'`
NAME="iguanaIR-$version-$REV.$ARCH.deb"
PNAME="iguanaIR-python-$version-$REV.$ARCH.deb"
NAME64="iguanaIR-$version-$REV.amd64.deb"
PNAME64="iguanaIR-python-$version-$REV.amd64.deb"
mkdir $BASE
mkdir $PBASE
mkdir $BASE/DEBIAN
mkdir $PBASE/DEBIAN

##make the DEBIAN/control file
echo "Package: iguanaIR" > $BASE/DEBIAN/control
echo "Version: $version-$REV" >> $BASE/DEBIAN/control
echo "Section: utils
Priority: optional
Architecture: $ARCH
Essential: no
Depends: udev (>= 0.99), libusb-0.1-4 (>= 0.1.11)
Recommends: lirc
Installed-Size: 500
Maintainer: IguanaWorks [support@iguanaworks.net]
Provides: iguanaIR
Description: This is the IguanaWorks IR USB client. Software allows you
             to control your IguanaWorks IR USB device. Includes software for use
             with lirc.
 .
 More info.
">> $BASE/DEBIAN/control


##make the Python  DEBIAN/control file
echo "Package: iguanaIR-python" > $PBASE/DEBIAN/control
echo "Version: $version-$REV" >> $PBASE/DEBIAN/control
echo "Section: utils
Priority: optional
Architecture: $ARCH
Essential: no
Depends: iguanaIR (>= $version), iguanaIR (<= $version-9999999), python (>= 2.4), python (<= 2.5)
Installed-Size: 100
Maintainer: IguanaWorks [support@iguanaworks.net]
Provides: iguanaIR-python
Description: This package provides the swig-generated Python module for interfacing
	with the Iguanaworks USB IR transceiver.
 .
 More info.
">> $PBASE/DEBIAN/control




#Make the DEBIAN/postrm file
echo "#!/bin/sh
set -e
update-rc.d iguanaIR remove ||true
rmdir /dev/iguanaIR 2>/dev/null||true
userdel iguanair||true" > $BASE/DEBIAN/postrm
chmod a+x $BASE/DEBIAN/postrm



#Make the DEBIAN/prerm file
echo "#!/bin/sh
set -e
/etc/init.d/iguanaIR stop" > $BASE/DEBIAN/prerm
chmod a+x $BASE/DEBIAN/prerm



#Make the DEBIAN/postinst file
echo "#!/bin/sh
set -e
/usr/sbin/groupadd -f -K GID_MIN=100 -K GID_MAX=500 iguanair
USERS=\`getent passwd|grep iguanair|sed -e 's/\([a-zA-Z]*:\)\(.*\)/\1/g'\`
update-rc.d iguanaIR defaults

if [ \"\$USERS\" != \"iguanair:\" ] ; then
echo \"Adding user iguananir\"
useradd -d /tmp -g iguanair -K UID_MIN=100 -K UID_MAX=500 -K PASS_MAX_DAYS=-1 -s /bin/false iguanair
fi

chown iguanair:iguanair /lib/udev/devices/iguanaIR
mkdir /dev/iguanaIR||true
touch /etc/udev/rules.d/iguanaIR.rules
chown iguanair:iguanair /dev/iguanaIR" >> $BASE/DEBIAN/postinst
chmod a+x $BASE/DEBIAN/postinst


mkdir $BASE/etc
mkdir $BASE/etc/default
cp iguanaIR.options $BASE/etc/default/
mkdir $BASE/etc/init.d
cp iguanaIR.init $BASE/etc/init.d/iguanaIR
mkdir $BASE/etc/udev
mkdir $BASE/etc/udev/rules.d
cp plug-trigger/udev/iguanaIR.rules $BASE/etc/udev/rules.d/

mkdir $BASE/lib
mkdir $BASE/lib/udev
mkdir $BASE/lib/udev/devices/
mkdir $BASE/lib/udev/devices/iguanaIR

mkdir $BASE/usr
mkdir $BASE/usr/bin

## Python stuff
mkdir $PBASE/usr
mkdir $PBASE/usr/lib
mkdir $PBASE/usr/lib/python2.4
mkdir $PBASE/usr/lib/python2.4/site-packages

cp iguanaIR.py $PBASE/usr/lib/python2.4/site-packages/
cp _iguanaIR.so $PBASE/usr/lib/python2.4/site-packages/
## End Python Stuff


cp igclient igdaemon $BASE/usr/bin/
mkdir $BASE/usr/include
cp iguanaIR.h  $BASE/usr/include/
mkdir $BASE/usr/lib
cp libiguanaIR.so $BASE/usr/lib/


mkdir $BASE/usr/share
mkdir $BASE/usr/share/doc
mkdir $BASE/usr/share/doc/iguanaIR
cp AUTHORS LICENSE WHY notes.txt protocols.txt $BASE/usr/share/doc/iguanaIR/





dpkg -b $BASE $NAME
dpkg -b $PBASE $PNAME


if [ -a  "$2" ];then

versioncheck=`echo "$2"|grep x86_64|grep iguanaIR-$version`

if [ -z "$versioncheck" ];then
echo "x86_64 rpm package wrong version"
else


echo "amd64"
cp -a $BASE $BASE64
echo "Architecture: amd64">>$BASE64/DEBIAN/control2
grep -v "Architecture:" $BASE64/DEBIAN/control >> $BASE64/DEBIAN/control2
mv $BASE64/DEBIAN/control2 $BASE64/DEBIAN/control


mkdir temp64
cp $2 temp64
cd temp64
rpm2cpio $2 |cpio -idv
cp usr/bin/igclient ../$BASE64/usr/bin/
cp usr/bin/igdaemon ../$BASE64/usr/bin/
cp usr/lib64/libiguanaIR.so ../$BASE64/usr/lib/
cd ..
rm -r temp64
dpkg -b $BASE64 $NAME64

fi
fi


if [ -a  "$3" ];then

versioncheck=`echo "$3"|grep x86_64|grep iguanaIR-python-$version`

if [ -z "$versioncheck" ];then
echo "x86_64 rpm package wrong version"
else


echo "Python: amd64"
cp -a $PBASE $PBASE64
echo "Architecture: amd64">>$PBASE64/DEBIAN/control2
grep -v "Architecture:" $PBASE64/DEBIAN/control >> $PBASE64/DEBIAN/control2
mv $PBASE64/DEBIAN/control2 $PBASE64/DEBIAN/control


mkdir ptemp64
cp $3 ptemp64
cd ptemp64
rpm2cpio $3 |cpio -idv
cp usr/lib/python2.5/site-packages/_iguanaIR.so ../$PBASE64/usr/lib/python2.4/site-packages/
cp usr/lib/python2.5/site-packages/iguanaIR.py ../$PBASE64/usr/lib/python2.4/site-packages/
#cp usr/lib/python2.5/site-packages/iguanaIR.pyc ../$PBASE64/usr/lib/python2.4/site-packages/
cd ..
rm -r ptemp64

dpkg -b $PBASE64 $PNAME64

fi
fi



