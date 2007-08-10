#!/bin/sh

cd autoconf
aclocal
autoconf -o ../configure configure.ac
