#! /bin/sh
# $Id$
## @file
# Linux Additions kernel module init script ($Revision$)
#

#
# Copyright (C) 2006-2022 Oracle and/or its affiliates.
#
# This file is part of VirtualBox base platform packages, as
# available from https://www.virtualbox.org.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation, in version 3 of the
# License.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <https://www.gnu.org/licenses>.
#
# SPDX-License-Identifier: GPL-3.0-only
#

# X-Start-Before is a Debian Addition which we use when converting to
# a systemd unit.  X-Service-Type is our own invention, also for systemd.

# chkconfig: 345 10 90
# description: VirtualBox Linux Additions kernel modules
#
### BEGIN INIT INFO
# Provides:       vboxadd
# Required-Start:
# Required-Stop:
# Default-Start:  2 3 4 5
# Default-Stop:   0 1 6
# X-Start-Before: display-manager
# X-Service-Type: oneshot
# Description:    VirtualBox Linux Additions kernel modules
### END INIT INFO

## @todo This file duplicates a lot of script with vboxdrv.sh.  When making
# changes please try to reduce differences between the two wherever possible.

# Testing:
# * Should fail if the configuration file is missing or missing INSTALL_DIR or
#   INSTALL_VER entries.
# * vboxadd, vboxsf and vboxdrmipc user groups should be created if they do not exist - test
#   by removing them before installing.
# * Shared folders can be mounted and auto-mounts accessible to vboxsf group,
#   including on recent Fedoras with SELinux.
# * Setting INSTALL_NO_MODULE_BUILDS inhibits modules and module automatic
#   rebuild script creation; otherwise modules, user, group, rebuild script,
#   udev rule and shared folder mount helper should be created/set up.
# * Setting INSTALL_NO_MODULE_BUILDS inhibits module load and unload on start
#   and stop.
# * Uninstalling the Additions and re-installing them does not trigger warnings.

export LC_ALL=C
PATH=$PATH:/bin:/sbin:/usr/sbin
PACKAGE=VBoxGuestAdditions
MODPROBE=/sbin/modprobe
OLDMODULES="vboxguest vboxadd vboxsf vboxvfs vboxvideo"
SERVICE="VirtualBox Guest Additions"
## systemd logs information about service status, otherwise do that ourselves.
QUIET=
test -z "${TARGET_VER}" && TARGET_VER=`uname -r`

# Marker to ignore a particular kernel version which was already installed.
#
# This is needed in order to prevent modules rebuild on system start and do
# that on system shutdown instead. Modern Linux distributions might attempt
# to run Additions service in async mode. As a result, on system boot, modules
# not-by-us will be loaded while we will try to build our modules. This marker is:
#
#   created -- in scope of setup_modules() when actual modules are built.
#   checked -- in scope of stop() when system goes shutdown and if marker
#              for certain kernel version does not exist, modules rebuild
#              will be triggered for this kernel version.
#   removed -- in scope of cleanup_modules() when modules are removed from
#              system for all installed kernels.
SKIPFILE_BASE=/var/lib/VBoxGuestAdditions/skip

export VBOX_KBUILD_TYPE
export USERNAME

setup_log()
{
    test -z "${LOG}" || return 0
    # Rotate log files
    LOG="/var/log/vboxadd-setup.log"
    mv "${LOG}.3" "${LOG}.4" 2>/dev/null
    mv "${LOG}.2" "${LOG}.3" 2>/dev/null
    mv "${LOG}.1" "${LOG}.2" 2>/dev/null
    mv "${LOG}" "${LOG}.1" 2>/dev/null
}

if $MODPROBE -c 2>/dev/null | grep -q '^allow_unsupported_modules  *0'; then
  MODPROBE="$MODPROBE --allow-unsupported-modules"
fi

# Preamble for Gentoo
if [ "`which $0`" = "/sbin/rc" ]; then
    shift
fi

begin()
{
    test -n "${QUIET}" || echo "${SERVICE}: ${1}"
}

info()
{
    if test -z "${QUIET}"; then
        echo "${SERVICE}: $1" | fold -s
    else
        echo "$1" | fold -s
    fi
}

fail()
{
    log "${1}"
    echo "$1" >&2
    echo "The log file $LOG may contain further information." >&2
    exit 1
}

log()
{
    setup_log
    echo "${1}" >> "${LOG}"
}

module_build_log()
{
    log "Error building the module.  Build output follows."
    echo ""
    echo "${1}" >> "${LOG}"
}

dev=/dev/vboxguest
userdev=/dev/vboxuser
config=/var/lib/VBoxGuestAdditions/config
owner=vboxadd
group=1

if test -r $config; then
  . $config
else
  fail "Configuration file $config not found"
fi
test -n "$INSTALL_DIR" -a -n "$INSTALL_VER" ||
  fail "Configuration file $config not complete"
MODULE_SRC="$INSTALL_DIR/src/vboxguest-$INSTALL_VER"
BUILDINTMP="$MODULE_SRC/build_in_tmp"

# Attempt to detect VirtualBox Guest Additions version and revision information.
VBOXCLIENT="${INSTALL_DIR}/bin/VBoxClient"
VBOX_VERSION="`"$VBOXCLIENT" --version | cut -d r -f1`"
[ -n "$VBOX_VERSION" ] || VBOX_VERSION='unknown'
VBOX_REVISION="r`"$VBOXCLIENT" --version | cut -d r -f2`"
[ "$VBOX_REVISION" != "r" ] || VBOX_REVISION='unknown'

running_vboxguest()
{
    lsmod | grep -q "vboxguest[^_-]"
}

running_vboxadd()
{
    lsmod | grep -q "vboxadd[^_-]"
}

running_vboxsf()
{
    lsmod | grep -q "vboxsf[^_-]"
}

running_vboxvideo()
{
    lsmod | grep -q "vboxvideo[^_-]"
}

do_vboxguest_non_udev()
{
    if [ ! -c $dev ]; then
        maj=`sed -n 's;\([0-9]\+\) vboxguest;\1;p' /proc/devices`
        if [ ! -z "$maj" ]; then
            min=0
        else
            min=`sed -n 's;\([0-9]\+\) vboxguest;\1;p' /proc/misc`
            if [ ! -z "$min" ]; then
                maj=10
            fi
        fi
        test -n "$maj" || {
            rmmod vboxguest 2>/dev/null
            fail "Cannot locate the VirtualBox device"
        }

        mknod -m 0664 $dev c $maj $min || {
            rmmod vboxguest 2>/dev/null
            fail "Cannot create device $dev with major $maj and minor $min"
        }
    fi
    chown $owner:$group $dev 2>/dev/null || {
        rm -f $dev 2>/dev/null
        rm -f $userdev 2>/dev/null
        rmmod vboxguest 2>/dev/null
        fail "Cannot change owner $owner:$group for device $dev"
    }

    if [ ! -c $userdev ]; then
        maj=10
        min=`sed -n 's;\([0-9]\+\) vboxuser;\1;p' /proc/misc`
        if [ ! -z "$min" ]; then
            mknod -m 0666 $userdev c $maj $min || {
                rm -f $dev 2>/dev/null
                rmmod vboxguest 2>/dev/null
                fail "Cannot create device $userdev with major $maj and minor $min"
            }
            chown $owner:$group $userdev 2>/dev/null || {
                rm -f $dev 2>/dev/null
                rm -f $userdev 2>/dev/null
                rmmod vboxguest 2>/dev/null
                fail "Cannot change owner $owner:$group for device $userdev"
            }
        fi
    fi
}

restart()
{
    stop && start
    return 0
}

## Update the initramfs.  Debian and Ubuntu put the graphics driver in, and
# need the touch(1) command below.  Everyone else that I checked just need
# the right module alias file from depmod(1) and only use the initramfs to
# load the root filesystem, not the boot splash.  update-initramfs works
# for the first two and dracut for every one else I checked.  We are only
# interested in distributions recent enough to use the KMS vboxvideo driver.
update_initramfs()
{
    ## kernel version to update for.
    version="${1}"
    depmod "${version}"
    rm -f "/lib/modules/${version}/initrd/vboxvideo"
    test ! -d "/lib/modules/${version}/initrd" ||
        test ! -f "/lib/modules/${version}/misc/vboxvideo.ko" ||
        touch "/lib/modules/${version}/initrd/vboxvideo"

    # Systems without systemd-inhibit probably don't need their initramfs
    # rebuild here anyway.
    type systemd-inhibit >/dev/null 2>&1 || return
    if type dracut >/dev/null 2>&1; then
        systemd-inhibit --why="Installing VirtualBox Guest Additions" \
            dracut -f --kver "${version}"
    elif type update-initramfs >/dev/null 2>&1; then
        systemd-inhibit --why="Installing VirtualBox Guest Additions" \
            update-initramfs -u -k "${version}"
    fi
}

# Remove any existing VirtualBox guest kernel modules from the disk, but not
# from the kernel as they may still be in use
cleanup_modules()
{
    # Needed for Ubuntu and Debian, see update_initramfs
    rm -f /lib/modules/*/initrd/vboxvideo
    for i in /lib/modules/*/misc; do
        KERN_VER="${i%/misc}"
        KERN_VER="${KERN_VER#/lib/modules/}"
        unset do_update
        for j in ${OLDMODULES}; do
            for mod_ext in ko ko.gz ko.xz ko.zst; do
                test -f "${i}/${j}.${mod_ext}" && do_update=1 && rm -f "${i}/${j}.${mod_ext}"
            done
        done
        test -z "$do_update" || update_initramfs "$KERN_VER"
        # Remove empty /lib/modules folders which may have been kept around
        rmdir -p "${i}" 2>/dev/null || true
        unset keep
        for j in /lib/modules/"${KERN_VER}"/*; do
            name="${j##*/}"
            test -d "${name}" || test "${name%%.*}" != modules && keep=1
        done
        if test -z "${keep}"; then
            rm -rf /lib/modules/"${KERN_VER}"
            rm -f /boot/initrd.img-"${KERN_VER}"
        fi
    done
    for i in ${OLDMODULES}; do
        # We no longer support DKMS, remove any leftovers.
        rm -rf "/var/lib/dkms/${i}"*
    done
    rm -f /etc/depmod.d/vboxvideo-upstream.conf
    rm -f "$SKIPFILE_BASE"-*
}

# Secure boot state.
case "`mokutil --sb-state 2>/dev/null`" in
    *"disabled in shim"*) unset HAVE_SEC_BOOT;;
    *"SecureBoot enabled"*) HAVE_SEC_BOOT=true;;
    *) unset HAVE_SEC_BOOT;;
esac
# So far we can only sign modules on Ubuntu and on Debian 10 and later.
DEB_PUB_KEY=/var/lib/shim-signed/mok/MOK.der
DEB_PRIV_KEY=/var/lib/shim-signed/mok/MOK.priv
# Check if key already enrolled.
unset HAVE_DEB_KEY
case "`mokutil --test-key "$DEB_PUB_KEY" 2>/dev/null`" in
    *"is already"*) DEB_KEY_ENROLLED=true;;
    *) unset DEB_KEY_ENROLLED;;
esac

# Check if update-secureboot-policy tool supports required commandline options.
update_secureboot_policy_supports()
{
    opt_name="$1"
    [ -n "$opt_name" ] || return

    [ -z "$(update-secureboot-policy --help 2>&1 | grep "$opt_name")" ] && return
    echo "1"
}

HAVE_UPDATE_SECUREBOOT_POLICY_TOOL=
if type update-secureboot-policy >/dev/null 2>&1; then
    [ "$(update_secureboot_policy_supports new-key)" = "1" -a "$(update_secureboot_policy_supports enroll-key)" = "1" ] && \
        HAVE_UPDATE_SECUREBOOT_POLICY_TOOL=true
fi

# Reads kernel configuration option.
kernel_get_config_opt()
{
    opt_name="$1"
    [ -n "$opt_name" ] || return

    # Check if there is a kernel tool which can extract config option.
    if test -x /lib/modules/"$KERN_VER"/build/scripts/config; then
        /lib/modules/"$KERN_VER"/build/scripts/config \
            --file /lib/modules/"$KERN_VER"/build/.config \
            --state "$opt_name" 2>/dev/null
    elif test -f /lib/modules/"$KERN_VER"/build/.config; then
        # Extract config option manually.
        grep "$opt_name" /lib/modules/"$KERN_VER"/build/.config | sed -e "s/^$opt_name=//" -e "s/\"//g"
    fi
}

# Reads CONFIG_MODULE_SIG_HASH from kernel config.
kernel_module_sig_hash()
{
    kernel_get_config_opt "CONFIG_MODULE_SIG_HASH"
}

# Returns "1" if kernel module signature hash algorithm
# is supported by us. Or empty string otherwise.
module_sig_hash_supported()
{
    sig_hashalgo="$1"
    [ -n "$sig_hashalgo" ] || return

    # Go through supported list.
    [    "$sig_hashalgo" = "sha1"   \
      -o "$sig_hashalgo" = "sha224" \
      -o "$sig_hashalgo" = "sha256" \
      -o "$sig_hashalgo" = "sha384" \
      -o "$sig_hashalgo" = "sha512" ] || return

    echo "1"
}

sign_modules()
{
    KERN_VER="$1"
    test -n "$KERN_VER" || return 1

    # Make list of mudules to sign.
    MODULE_LIST="vboxguest vboxsf"
    # vboxvideo might not present on for older kernels.
    [ -f "/lib/modules/"$KERN_VER"/misc/vboxvideo.ko" ] && MODULE_LIST="$MODULE_LIST vboxvideo"

    # Secure boot on Ubuntu, Debian and Oracle Linux.
    if test -n "$HAVE_SEC_BOOT"; then
        begin "Signing VirtualBox Guest Additions kernel modules"

        # Generate new signing key if needed.
        [ -n "$HAVE_UPDATE_SECUREBOOT_POLICY_TOOL" ] && SHIM_NOTRIGGER=y update-secureboot-policy --new-key

        # Check if signing keys are in place.
        if test ! -f "$DEB_PUB_KEY" || ! test -f "$DEB_PRIV_KEY"; then
            # update-secureboot-policy tool present in the system, but keys were not generated.
            [ -n "$HAVE_UPDATE_SECUREBOOT_POLICY_TOOL" ] && info "

update-secureboot-policy tool does not generate signing keys
in your distribution, see below on how to generate them manually."
            # update-secureboot-policy not present in the system, recommend generate keys manually.
            fail "

System is running in Secure Boot mode, however your distribution
does not provide tools for automatic generation of keys needed for
modules signing. Please consider to generate and enroll them manually:

    sudo mkdir -p /var/lib/shim-signed/mok
    sudo openssl req -nodes -new -x509 -newkey rsa:2048 -outform DER -addext \"extendedKeyUsage=codeSigning\" -keyout $DEB_PRIV_KEY -out $DEB_PUB_KEY
    sudo mokutil --import $DEB_PUB_KEY
    sudo reboot

Restart \"rcvboxadd setup\" after system is rebooted.
"
        fi

        # Get kernel signature hash algorithm from kernel config and validate it.
        sig_hashalgo=$(kernel_module_sig_hash)
        [ "$(module_sig_hash_supported $sig_hashalgo)" = "1" ] \
            || fail "Unsupported kernel signature hash algorithm $sig_hashalgo"

        # Sign modules.
        for i in $MODULE_LIST; do

            # Try to find a tool for modules signing.
            SIGN_TOOL=$(which kmodsign 2>/dev/null)
            # Attempt to use in-kernel signing tool if kmodsign not found.
            if test -z "$SIGN_TOOL"; then
                if test -x "/lib/modules/$KERN_VER/build/scripts/sign-file"; then
                    SIGN_TOOL="/lib/modules/$KERN_VER/build/scripts/sign-file"
                fi
            fi

            # Check if signing tool is available.
            [ -n "$SIGN_TOOL" ] || fail "Unable to find signing tool"

            "$SIGN_TOOL" "$sig_hashalgo" "$DEB_PRIV_KEY" "$DEB_PUB_KEY" \
                /lib/modules/"$KERN_VER"/misc/"$i".ko || fail "Unable to sign $i.ko"
        done
        # Enroll signing key if needed.
        if test -n "$HAVE_UPDATE_SECUREBOOT_POLICY_TOOL"; then
            # update-secureboot-policy "expects" DKMS modules.
            # Work around this and talk to the authors as soon
            # as possible to fix it.
            mkdir -p /var/lib/dkms/vbox-temp
            update-secureboot-policy --enroll-key 2>/dev/null ||
                fail "Failed to enroll secure boot key."
            rmdir -p /var/lib/dkms/vbox-temp 2>/dev/null

            # Indicate that key has been enrolled and reboot is needed.
            HAVE_DEB_KEY=true
        fi
    fi
}

# Build and install the VirtualBox guest kernel modules
setup_modules()
{
    KERN_VER="$1"
    test -n "$KERN_VER" || return 1
    # Match (at least): vboxguest.o; vboxguest.ko; vboxguest.ko.xz
    set /lib/modules/"$KERN_VER"/misc/vboxguest.*o*
    #test ! -f "$1" || return 0
    test -d /lib/modules/"$KERN_VER"/build || return 0
    export KERN_VER
    info "Building the modules for kernel $KERN_VER."

    # Detect if kernel was built with clang.
    unset LLVM
    vbox_cc_is_clang=$(kernel_get_config_opt "CONFIG_CC_IS_CLANG")
    if test "${vbox_cc_is_clang}" = "y"; then
        info "Using clang compiler."
        export LLVM=1
    fi

    log "Building the main Guest Additions $INSTALL_VER module for kernel $KERN_VER."
    if ! myerr=`$BUILDINTMP \
        --save-module-symvers /tmp/vboxguest-Module.symvers \
        --module-source $MODULE_SRC/vboxguest \
        --no-print-directory install 2>&1`; then
        # If check_module_dependencies.sh fails it prints a message itself.
        module_build_log "$myerr"
        "${INSTALL_DIR}"/other/check_module_dependencies.sh 2>&1 &&
            info "Look at $LOG to find out what went wrong"
        return 0
    fi
    log "Building the shared folder support module."
    if ! myerr=`$BUILDINTMP \
        --use-module-symvers /tmp/vboxguest-Module.symvers \
        --module-source $MODULE_SRC/vboxsf \
        --no-print-directory install 2>&1`; then
        module_build_log "$myerr"
        info  "Look at $LOG to find out what went wrong"
        return 0
    fi
    log "Building the graphics driver module."
    if ! myerr=`$BUILDINTMP \
        --use-module-symvers /tmp/vboxguest-Module.symvers \
        --module-source $MODULE_SRC/vboxvideo \
        --no-print-directory install 2>&1`; then
        module_build_log "$myerr"
        info "Look at $LOG to find out what went wrong"
    fi
    [ -d /etc/depmod.d ] || mkdir /etc/depmod.d
    echo "override vboxguest * misc" > /etc/depmod.d/vboxvideo-upstream.conf
    echo "override vboxsf * misc" >> /etc/depmod.d/vboxvideo-upstream.conf
    echo "override vboxvideo * misc" >> /etc/depmod.d/vboxvideo-upstream.conf

    sign_modules "${KERN_VER}"

    update_initramfs "${KERN_VER}"

    # We have just built modules for KERN_VER kernel. Create a marker to indicate
    # that modules for this kernel version should not be rebuilt on system shutdown.
    touch "$SKIPFILE_BASE"-"$KERN_VER"

    return 0
}

create_vbox_user()
{
    # This is the LSB version of useradd and should work on recent
    # distributions
    useradd -d /var/run/vboxadd -g 1 -r -s /bin/false vboxadd >/dev/null 2>&1 || true
    # And for the others, we choose a UID ourselves
    useradd -d /var/run/vboxadd -g 1 -u 501 -o -s /bin/false vboxadd >/dev/null 2>&1 || true

}

create_udev_rule()
{
    # Create udev description file
    if [ -d /etc/udev/rules.d ]; then
        udev_call=""
        udev_app=`which udevadm 2> /dev/null`
        if [ $? -eq 0 ]; then
            udev_call="${udev_app} version 2> /dev/null"
        else
            udev_app=`which udevinfo 2> /dev/null`
            if [ $? -eq 0 ]; then
                udev_call="${udev_app} -V 2> /dev/null"
            fi
        fi
        udev_fix="="
        if [ "${udev_call}" != "" ]; then
            udev_out=`${udev_call}`
            udev_ver=`expr "$udev_out" : '[^0-9]*\([0-9]*\)'`
            if [ "$udev_ver" = "" -o "$udev_ver" -lt 55 ]; then
               udev_fix=""
            fi
        fi
        ## @todo 60-vboxadd.rules -> 60-vboxguest.rules ?
        echo "KERNEL=${udev_fix}\"vboxguest\", NAME=\"vboxguest\", OWNER=\"vboxadd\", MODE=\"0660\"" > /etc/udev/rules.d/60-vboxadd.rules
        echo "KERNEL=${udev_fix}\"vboxuser\", NAME=\"vboxuser\", OWNER=\"vboxadd\", MODE=\"0666\"" >> /etc/udev/rules.d/60-vboxadd.rules
        # Make sure the new rule is noticed.
        udevadm control --reload >/dev/null 2>&1 || true
        udevcontrol reload_rules >/dev/null 2>&1 || true
    fi
}

create_module_rebuild_script()
{
    # And a post-installation script for rebuilding modules when a new kernel
    # is installed.
    mkdir -p /etc/kernel/postinst.d /etc/kernel/prerm.d
    cat << EOF > /etc/kernel/postinst.d/vboxadd
#!/bin/sh
# This only works correctly on Debian derivatives - Red Hat calls it before
# installing the right header files.
/sbin/rcvboxadd quicksetup "\${1}"
exit 0
EOF
    cat << EOF > /etc/kernel/prerm.d/vboxadd
#!/bin/sh
for i in ${OLDMODULES}; do rm -f /lib/modules/"\${1}"/misc/"\${i}".ko; done
rmdir -p /lib/modules/"\$1"/misc 2>/dev/null || true
exit 0
EOF
    chmod 0755 /etc/kernel/postinst.d/vboxadd /etc/kernel/prerm.d/vboxadd
}

shared_folder_setup()
{
    # Add a group "vboxsf" for Shared Folders access
    # All users which want to access the auto-mounted Shared Folders have to
    # be added to this group.
    groupadd -r -f vboxsf >/dev/null 2>&1

    # Put the mount.vboxsf mount helper in the right place.
    ## @todo It would be nicer if the kernel module just parsed parameters
    # itself instead of needing a separate binary to do that.
    ln -sf "${INSTALL_DIR}/other/mount.vboxsf" /sbin
    # SELinux security context for the mount helper.
    if test -e /etc/selinux/config; then
        # This is correct.  semanage maps this to the real path, and it aborts
        # with an error, telling you what you should have typed, if you specify
        # the real path.  The "chcon" is there as a back-up for old guests.
        command -v semanage > /dev/null &&
            semanage fcontext -a -t mount_exec_t "${INSTALL_DIR}/other/mount.vboxsf"
        chcon -t mount_exec_t "${INSTALL_DIR}/other/mount.vboxsf" 2>/dev/null
    fi
}

# Returns path to module file as seen by modinfo(8) or empty string.
module_path()
{
    mod="$1"
    [ -n "$mod" ] || return

    modinfo "$mod" 2>/dev/null | grep -e "^filename:" | tr -s ' ' | cut -d " " -f2
}

# Returns module version if module is available or empty string.
module_version()
{
    mod="$1"
    [ -n "$mod" ] || return

    modinfo "$mod" 2>/dev/null | grep -e "^version:" | tr -s ' ' | cut -d " " -f2
}

# Returns module revision if module is available in the system or empty string.
module_revision()
{
    mod="$1"
    [ -n "$mod" ] || return

    modinfo "$mod" 2>/dev/null | grep -e "^version:" | tr -s ' ' | cut -d " " -f3
}


# Returns "1" if module is signed and signature can be verified
# with public key provided in DEB_PUB_KEY. Or empty string otherwise.
module_signed()
{
    mod="$1"
    [ -n "$mod" ] || return

    extraction_tool=/lib/modules/"$(uname -r)"/build/scripts/extract-module-sig.pl
    mod_path=$(module_path "$mod" 2>/dev/null)
    openssl_tool=$(which openssl 2>/dev/null)
    # Do not use built-in printf!
    printf_tool=$(which printf 2>/dev/null)

    # Make sure all the tools required for signature validation are available.
    [ -x "$extraction_tool" ] || return
    [ -n "$mod_path"        ] || return
    [ -n "$openssl_tool"    ] || return
    [ -n "$printf_tool"     ] || return

    # Make sure openssl can handle hash algorithm.
    sig_hashalgo=$(modinfo -F sig_hashalgo "$mod" 2>/dev/null)
    [ "$(module_sig_hash_supported $sig_hashalgo)" = "1" ] || return

    # Generate file names for temporary stuff.
    mod_pub_key=$(mktemp -u)
    mod_signature=$(mktemp -u)
    mod_unsigned=$(mktemp -u)

    # Convert public key in DER format into X509 certificate form.
    "$openssl_tool" x509 -pubkey -inform DER -in "$DEB_PUB_KEY" -out "$mod_pub_key" 2>/dev/null
    # Extract raw module signature and convert it into binary format.
    "$printf_tool" \\x$(modinfo -F signature "$mod" | sed -z 's/[ \t\n]//g' | sed -e "s/:/\\\x/g") 2>/dev/null > "$mod_signature"
    # Extract unsigned module for further digest calculation.
    "$extraction_tool" -0 "$mod_path" 2>/dev/null > "$mod_unsigned"

    # Verify signature.
    rc=""
    "$openssl_tool" dgst "-$sig_hashalgo" -binary -verify "$mod_pub_key" -signature "$mod_signature" "$mod_unsigned" 2>&1 >/dev/null && rc="1"
    # Clean up.
    rm -f $mod_pub_key $mod_signature $mod_unsigned

    # Check result.
    [ "$rc" = "1" ] || return

    echo "1"
}

# Returns "1" if externally built module is available in the system and its
# version and revision number do match to current VirtualBox installation.
# Or empty string otherwise.
module_available()
{
    mod="$1"
    [ -n "$mod" ] || return

    [ "$VBOX_VERSION" = "$(module_version "$mod")" ] || return
    [ "$VBOX_REVISION" = "$(module_revision "$mod")" ] || return

    # Check if module belongs to VirtualBox installation.
    #
    # We have a convention that only modules from /lib/modules/*/misc
    # belong to us. Modules from other locations are treated as
    # externally built.
    mod_path="$(module_path "$mod")"

    # If module path points to a symbolic link, resolve actual file location.
    [ -L "$mod_path" ] && mod_path="$(readlink -e -- "$mod_path")"

    # File exists?
    [ -f "$mod_path" ] || return

    # Extract last component of module path and check whether it is located
    # outside of /lib/modules/*/misc.
    mod_dir="$(dirname "$mod_path" | sed 's;^.*/;;')"
    [ "$mod_dir" = "misc" ] || return

    # In case if system is running in Secure Boot mode, check if module is signed.
    if test -n "$HAVE_SEC_BOOT"; then
        [ "$(module_signed "$mod")" = "1" ] || return
    fi

    echo "1"
}

# Check if required modules are installed in the system and versions match.
setup_complete()
{
    [ "$(module_available vboxguest)"   = "1" ] || return
    [ "$(module_available vboxsf)"      = "1" ] || return

    # All modules are in place.
    echo "1"
}

# setup_script
setup()
{
    info "Setting up modules"

    # chcon is needed on old Fedora/Redhat systems.  No one remembers which.
    test ! -e /etc/selinux/config ||
        chcon -t bin_t "$BUILDINTMP" 2>/dev/null

    if test -z "$INSTALL_NO_MODULE_BUILDS"; then
        # Check whether modules setup is already complete for currently running kernel.
        # Prevent unnecessary rebuilding in order to speed up booting process.
        if test "$(setup_complete)" = "1"; then
            info "VirtualBox Guest Additions kernel modules $VBOX_VERSION $VBOX_REVISION are \
already available for kernel $TARGET_VER and do not require to be rebuilt."
        else
            info "Building the VirtualBox Guest Additions kernel modules.  This may take a while."
            info "To build modules for other installed kernels, run"
            info "  /sbin/rcvboxadd quicksetup <version>"
            info "or"
            info "  /sbin/rcvboxadd quicksetup all"
            if test -d /lib/modules/"$TARGET_VER"/build; then
                setup_modules "$TARGET_VER"
                depmod
            else
                info "Kernel headers not found for target kernel $TARGET_VER. \
Please install them and execute
  /sbin/rcvboxadd setup"
            fi
        fi
    fi
    create_vbox_user
    create_udev_rule
    test -n "${INSTALL_NO_MODULE_BUILDS}" || create_module_rebuild_script
    shared_folder_setup
    # Create user group which will have permissive access to DRP IPC server socket.
    groupadd -r -f vboxdrmipc >/dev/null 2>&1

    if  running_vboxguest || running_vboxadd; then
        info "Running kernel modules will not be replaced until the system is restarted"
    fi

    # Put the X.Org driver in place.  This is harmless if it is not needed.
    # Also set up the OpenGL library.
    myerr=`"${INSTALL_DIR}/init/vboxadd-x11" setup 2>&1`
    test -z "${myerr}" || log "${myerr}"

    return 0
}

# cleanup_script
cleanup()
{
    if test -z "${INSTALL_NO_MODULE_BUILDS}"; then
        # Delete old versions of VBox modules.
        cleanup_modules
        depmod

        # Remove old module sources
        for i in $OLDMODULES; do
          rm -rf /usr/src/$i-*
        done
    fi

    # Clean-up X11-related bits
    "${INSTALL_DIR}/init/vboxadd-x11" cleanup

    # Remove other files
    if test -z "${INSTALL_NO_MODULE_BUILDS}"; then
        rm -f /etc/kernel/postinst.d/vboxadd /etc/kernel/prerm.d/vboxadd
        rmdir -p /etc/kernel/postinst.d /etc/kernel/prerm.d 2>/dev/null || true
    fi
    rm -f /sbin/mount.vboxsf 2>/dev/null
    rm -f /etc/udev/rules.d/60-vboxadd.rules 2>/dev/null
    udevadm control --reload >/dev/null 2>&1 || true
    udevcontrol reload_rules >/dev/null 2>&1 || true
}

start()
{
    begin "Starting."

    # Check if kernel modules for currently running kernel are ready
    # and rebuild them if needed.
    setup

    # Warn if Secure Boot setup not yet complete.
    if test -n "$HAVE_SEC_BOOT" && test -z "$DEB_KEY_ENROLLED"; then
        if test -n "$HAVE_DEB_KEY"; then
            info "You must re-start your system to finish secure boot set-up."
        else
            info "You must sign vboxguest, vboxsf and
vboxvideo (if present) kernel modules before using
VirtualBox Guest Additions. See the documentation
for your Linux distribution."
        fi
    fi

    if test -z "${INSTALL_NO_MODULE_BUILDS}"; then
        test -d /sys &&
            ps -A -o comm | grep -q '/*udevd$' 2>/dev/null ||
            no_udev=1
        running_vboxguest || {
            rm -f $dev || {
                fail "Cannot remove $dev"
            }
            rm -f $userdev || {
                fail "Cannot remove $userdev"
            }
            $MODPROBE vboxguest >/dev/null 2>&1 ||
                fail "modprobe vboxguest failed"
            case "$no_udev" in 1)
                sleep .5;;
            esac
            $MODPROBE vboxsf > /dev/null 2>&1 ||
                info "modprobe vboxsf failed"
        }
        case "$no_udev" in 1)
            do_vboxguest_non_udev;;
        esac
    fi  # INSTALL_NO_MODULE_BUILDS

    return 0
}

stop()
{
    begin "Stopping."
    if test -z "${INSTALL_NO_MODULE_BUILDS}"; then
        # We want to build modules for newly installed kernels on shutdown, so
        # check which we marked at start-up.
        for setupi in /lib/modules/*; do
            KERN_VER="${setupi##*/}"
            # For a full setup, mark kernels we do not want to build.
            test -f "$SKIPFILE_BASE"-"$KERN_VER" || setup_modules "$KERN_VER"
        done
    fi
    if test -r /etc/ld.so.conf.d/00vboxvideo.conf; then
        rm /etc/ld.so.conf.d/00vboxvideo.conf
        ldconfig
    fi
    if ! umount -a -t vboxsf 2>/dev/null; then
        # Make sure we only fail, if there are truly no more vboxsf
        # mounts in the system.
        [ -n "$(findmnt -t vboxsf)" ] && fail "Cannot unmount vboxsf folders"
    fi
    test -n "${INSTALL_NO_MODULE_BUILDS}" ||
        info "You may need to restart your guest system to finish removing guest drivers."
    return 0
}

dmnstatus()
{
    if running_vboxguest; then
        echo "The VirtualBox Additions are currently running."
    else
        echo "The VirtualBox Additions are not currently running."
    fi
}

for i; do
    case "$i" in quiet) QUIET=yes;; esac
done
case "$1" in
# Does setup without clean-up first and marks all kernels currently found on the
# system so that we can see later if any were added.
start)
    start
    ;;
# Tries to build kernel modules for kernels added since start.  Tries to unmount
# shared folders.  Uninstalls our Chromium 3D libraries since we can't always do
# this fast enough at start time if we discover we do not want to use them.
stop)
    stop
    ;;
restart)
    restart
    ;;
# Setup does a clean-up (see below) and re-does all Additions-specific
# configuration of the guest system, including building kernel modules for the
# current kernel.
setup)
    cleanup && start
    ;;
# Builds kernel modules for the specified kernels if they are not already built.
quicksetup)
    if test x"$2" = xall; then
       for topi in /lib/modules/*; do
           KERN_VER="${topi%/misc}"
           KERN_VER="${KERN_VER#/lib/modules/}"
           setup_modules "$KERN_VER"
        done
    elif test -n "$2"; then
        setup_modules "$2"
    else
        setup_modules "$TARGET_VER"
    fi
    ;;
# Clean-up removes all Additions-specific configuration of the guest system,
# including all kernel modules.
cleanup)
    cleanup
    ;;
status)
    dmnstatus
    ;;
*)
    echo "Usage: $0 {start|stop|restart|status|setup|quicksetup|cleanup} [quiet]"
    exit 1
esac

exit
