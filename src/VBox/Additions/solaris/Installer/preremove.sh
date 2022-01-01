#!/bin/sh
# $Id$
## @file
# VirtualBox preremove script for Solaris Guest Additions.
#

#
# Copyright (C) 2008-2022 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#
# The contents of this file may alternatively be used under the terms
# of the Common Development and Distribution License Version 1.0
# (CDDL) only, as it comes in the "COPYING.CDDL" file of the
# VirtualBox OSE distribution, in which case the provisions of the
# CDDL are applicable instead of those of the GPL.
#
# You may elect to license modified versions of this file under the
# terms and conditions of either the GPL or the CDDL or both.
#

LC_ALL=C
export LC_ALL

LANG=C
export LANG

echo "Removing VirtualBox service..."

# stop and unregister VBoxService
/usr/sbin/svcadm disable -s virtualbox/vboxservice
# Don't need to delete, taken care of by the manifest action
# /usr/sbin/svccfg delete svc:/application/virtualbox/vboxservice:default
/usr/sbin/svcadm restart svc:/system/manifest-import:default

# stop VBoxClient
pkill -INT VBoxClient

echo "Removing VirtualBox kernel modules..."

# vboxguest.sh would've been installed, we just need to call it.
/opt/VirtualBoxAdditions/vboxguest.sh stopall silentunload

# Figure out group to use for /etc/devlink.tab (before Solaris 11 SRU6
# it was always using group sys)
group=sys
if [ -f /etc/dev/reserved_devnames ]; then
    # Solaris 11 SRU6 and later use group root (check a file which isn't
    # tainted by VirtualBox install scripts and allow no other group)
    refgroup=$(LC_ALL=C /usr/bin/ls -lL /etc/dev/reserved_devnames | awk '{ print $4 }' 2>/dev/null)
    if [ $? -eq 0 -a "x$refgroup" = "xroot" ]; then
        group=root
    fi
    unset refgroup
fi

# remove devlink.tab entry for vboxguest
sed -e '/name=vboxguest/d' /etc/devlink.tab > /etc/devlink.vbox
chmod 0644 /etc/devlink.vbox
chown root:$group /etc/devlink.vbox
mv -f /etc/devlink.vbox /etc/devlink.tab

# remove the link
if test -h "/dev/vboxguest" || test -f "/dev/vboxguest"; then
    rm -f /dev/vboxdrv
fi
if test -h "/dev/vboxms" || test -f "/dev/vboxms"; then
    rm -f /dev/vboxms
fi

# Try and restore xorg.conf!
echo "Restoring X.Org..."
/opt/VirtualBoxAdditions/x11restore.pl


echo "Done."

