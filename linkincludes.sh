#! /bin/sh
# A simple script to replicate the include structure of os161
# Place this script in the os161/src folder.
# Once run, this script is not use anymore
# Usage: ./linkincludes.sh

# Change for different kernel configurations, make sure to rerun
KERNEL=DUMBVM

# Protect against people not following the instructions
if [ ! -d userland ] || [ ! -d kern ]; then
    echo "Run this in the src directory only"
    exit 1
fi

# Clean up, incase this is run multiple times
if [ -d .ycm_inc ]; then
    # Got to be very sure symlinks don't get dereferenced
    # otherwise we end up deleting all the headers...
    echo "Cleaning up"
    rm -v $(find -P .ycm_inc -type l)
    rm -r -v .ycm_inc
fi

mkdir -v .ycm_inc

cd .ycm_inc

# Use relative paths for links for better portability

# Kernel
# (Yes there is a includelinks folder in kern/compile/$KERNEL,
# but this works without a configured kernel)
mkdir -v kernel2
mkdir -v kernel3
mkdir -v kernel3/kern
# <*>
ln -v -s ../kern/include kernel
# <machine/*>
ln -v -s ../../kern/arch/mips/include kernel2/machine
# <lamebus/*>
ln -v -s ../../kern/dev/lamebus kernel2/lamebus
# <sys161/*>
ln -v -s ../../kern/arch/sys161/include kernel2/sys161
# <platform/*>
ln -v -s ../../kern/arch/sys161/include kernel2/platform
# <kern/machine/*>
ln -v -s ../../../kern/arch/mips/include/kern kernel3/kern/machine
# <kern/mips/*>
ln -v -s ../../../kern/arch/mips/include/kern kernel3/kern/mips
# <*> Add auto generated files from the current kernel
ln -v -s "../kern/compile/$KERNEL" kernel4
if [ ! -d "../kern/compile/$KERNEL" ]; then
    echo "WARNING: kernel $KERNEL not found, some includes will be missing"
    echo "Compile the kernel to fix this"
fi

# kern/dev
mkdir -v kerndev
# <generic/*>
ln -v -s ../../kern/dev/generic kerndev/generic

# Mips
mkdir -v mips
mkdir -v mips/kern
# <mips/*>
ln -v -s ../../kern/arch/mips/include mips/mips
# <kern/mips/*>
ln -v -s ../../../kern/arch/mips/include/kern mips/kern/mips

# Userland
mkdir -v user2
mkdir -v user3
mkdir -v user3/kern
# <*>
ln -v -s ../userland/include user
# <kern/*>
ln -v -s ../../kern/include/kern user2/kern
# <machine/*>
ln -v -s ../../kern/arch/mips/include user2/machine
# <kern/machine/*>
ln -v -s ../../../kern/arch/mips/include/kern user3/kern/machine

cd ..

echo "Done!"
echo ""

if [ ! -f .ycm_extra_conf.py ]; then
    echo "Make sure to download the .ycm_extra_conf.py"
    echo "Place in the current folder ($PWD)"
    echo ""
fi

echo "Make sure to add the following to your .vimrc:"
echo "let g:ycm_extra_conf_globlist = ['$PWD/*']"
