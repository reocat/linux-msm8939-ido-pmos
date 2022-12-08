#! /bin/sh
set -e o pipefail

KERNEL_DIR=$PWD

##------------------------------------------------------##

#Now Its time for other stuffs like cloning, exporting, etc

clone() {
	echo " "
	echo "★★Cloning GCC Toolchain from GitHub .."
	git clone --progress -j32 --depth 1 --no-single-branch https://github.com/archie9211/linaro -b master linaro
	echo "★★Cloning Kinda Done..!!!"
}

##------------------------------------------------------##

function exports {
	export KBUILD_BUILD_USER="maple"
	export KBUILD_BUILD_HOST="mimi"
	export ARCH=arm64
	export SUBARCH=arm64
	export CROSS_COMPILE=$KERNEL_DIR/linaro/bin/aarch64-linux-gnu-
	export PROCS=$(nproc --all)
}

##---------------------------------------------------------##



##----------------------------------------------------------##

function build_kernel {
	make O=out defconfig_msm8916
	BUILD_START=$(date +"%s")
	make -j$PROCS O=out CROSS_COMPILE=$CROSS_COMPILE
	BUILD_END=$(date +"%s")
	DIFF=$((BUILD_END - BUILD_START))
}


##--------------------------------------------------------------##

clone
exports
build_kernel
tar -czvf $KERNEL_DIR `date`.tar.gz

##----------------*****-----------
