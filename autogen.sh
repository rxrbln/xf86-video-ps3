#! /bin/sh

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

ORIGDIR=`pwd`
cd $srcdir

autoreconf -v --install || exit 1
cd $ORIGDIR || exit $?

git config --local --get format.subjectPrefix >/dev/null 2>&1 ||
    git config --local format.subjectPrefix "PATCH xf86-video-ps3"

$srcdir/configure --enable-maintainer-mode "$@"
