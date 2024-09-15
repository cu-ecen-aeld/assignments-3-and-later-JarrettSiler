#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

USER=${USER:-default_user}
OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

if [ ! -d "${OUTDIR}" ]; then
    mkdir -p $OUTDIR
fi
sudo chown $USER:$USER ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # DONE: Add your kernel build steps here
    #clean step
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    #default configuration
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    #build vm linux target
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    #build modules and device tree
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
fi

#make image
cd linux-stable
echo "Adding the Image in outdir"
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}/Image

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# DONE: Create necessary base directories
mkdir -p ${OUTDIR}/rootfs/{bin,dev,etc,lib,lib64,proc,sys,sbin,tmp,usr,usr/lib,usr/bin,usr/sbin,var,var/log,home}

cd ${OUTDIR}
if [ ! -d "${OUTDIR}/busybox" ]
then
    git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # DONE:  Configure busybox
    make distclean
    make defconfig
else
    cd busybox
fi

# DONE: Make and install busybox
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

cd ${OUTDIR}/rootfs
echo "Library dependencies"
${CROSS_COMPILE}readelf -a $OUTDIR/bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a $OUTDIR/bin/busybox | grep "Shared library"

# DONE: Add library dependencies to rootfs
REQD_LIBRARIES=$(${CROSS_COMPILE}readelf -a $OUTDIR/bin/busybox | grep "Shared library" | awk -F'[][]' '{print $2}' )
for LIB in ${REQD_LIBRARIES}; do
    LIB_PATH=$(find $(aarch64-none-linux-gnu-gcc --print-sysroot) -name "$LIB")

    if [ -n "$LIB_PATH" ]; then
        echo "Copying $LIB_PATH to $OUTDIR/rootfs/lib64 and $OUTDIR/rootfs/lib"
        cp $LIB_PATH ${OUTDIR}/rootfs/lib64
        cp $LIB_PATH ${OUTDIR}/rootfs/lib
    else
        echo "Library $LIB not found!"
    fi
done
cp $(aarch64-none-linux-gnu-gcc --print-sysroot)/lib/ld-linux-aarch64.so.1 ${OUTDIR}/rootfs/lib/

# DONE: Make device nodes
create_device_node() {
    local node="$1"
    local type="$2"
    local major="$3"
    local minor="$4"

    #if [ ! -d "$(dirname "$node")" ]; then
    if [ ! -e "$node" ]; then
        sudo mknod -m 666 "$node" "$type" "$major" "$minor"
        echo "Created $node"
    else
        echo "$node already exists"
    fi
}
create_device_node "${OUTDIR}/rootfs/dev/null" c 1 3
create_device_node "${OUTDIR}/rootfs/dev/zero" c 1 5
create_device_node "${OUTDIR}/rootfs/dev/random" c 1 8
create_device_node "${OUTDIR}/rootfs/dev/console" c 5 1

# DONE: Clean and build the writer utility
cd ${FINDER_APP_DIR}
make clean
${CROSS_COMPILE}gcc -o writer writer.c
cp writer ${OUTDIR}/rootfs/home/

# DONE: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
PARENT_DIR=$(dirname "$FINDER_APP_DIR")
cd ${OUTDIR}
mkdir -p rootfs/home/conf

cp ${PARENT_DIR}/conf/username.txt rootfs/home/conf/
cp ${PARENT_DIR}/conf/assignment.txt rootfs/home/conf/
cp ${FINDER_APP_DIR}/finder.sh rootfs/home/
cp ${FINDER_APP_DIR}/finder-test.sh rootfs/home/
cp ${FINDER_APP_DIR}/autorun-qemu.sh rootfs/home/

# DONE: Chown the root directory
sudo chown -R root:root ${OUTDIR}/rootfs

#for  earlier issues:
#sudo rm -f ${OUTDIR}/rootfs/initramfs.cpio

# DONE: Create initramfs.cpio.gz
cd ${OUTDIR}/rootfs
sudo chown -R root:root *
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
cd ${OUTDIR}
gzip -f initramfs.cpio

#did it get created?
echo -n "location of initramfs.cpio.gz: "
ls -l ${OUTDIR}/initramfs.cpio.gz