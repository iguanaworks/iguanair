Please see the more generic README.txt for more details, but OS X
specific information should go here.

Additional files for OS X:
com.iguana.igdaemon.plist
    A launched configuration file used to start igdaemon on MacOS X.
    Use the following command to quit igdaemon: 
    sudo launchctl unload -w /Library/LaunchDaemons/com.iguana.igdaemon.plist

daemonosx.c
    Support for USB hot plug events in OS X.
