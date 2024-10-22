To compile the kernel image as used by the Quest headset, follow the
instructions below.

* Obtain the necessary toolchain from AOSP:
  * git clone --depth=1 -b android12L-release https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86
  * git clone --depth=1 -b android12L-release https://android.googlesource.com/platform/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9
* Use the following script to build the kernel image:
```
LLVM_PATH=$1
GCC_PATH=$2
KERNEL_SRC_PATH=$3
OUT_PATH=$4

MAKE_ENV="O=$OUT_PATH \
  ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- \
  CROSS_COMPILE_COMPAT=arm-linux-gnueabi- \
  CROSS_COMPILE_ARM32=arm-linux-androideabi- \
  CLANG_TRIPLE=aarch64-linux-gnu- \
  REAL_CC=$LLVM_PATH/bin/clang \
  GCC_TOOLCHAIN=$GCC_PATH \
  GCC_TOOLCHAIN_DIR=$GCC_PATH/bin \
  LLVM=1 \
  LLVM_IAS=1 \
  LD=$LLVM_PATH/bin/ld.lld \
  AR=$LLVM_PATH//bin/llvm-ar \
  LLVM_AR=$LLVM_PATH/bin/llvm-ar \
  NM=$LLVM_PATH/bin/llvm-nm \
  LLVM_NM=$LLVM_PATH/bin/llvm-nm \
  OBJCOPY=$LLVM_PATH/bin/llvm-objcopy \
  OBJDUMP=$LLVM_PATH/bin/llvm-objdump \
  READELF=$LLVM_PATH/bin/llvm-readelf \
  OBJSIZE=$LLVM_PATH/bin/llvm-size \
  STRIP=$LLVM_PATH/bin/llvm-strip
  HOSTCC=$LLVM_PATH/bin/clang \
  HOSTAR=$LLVM_PATH/bin/llvm-ar \
  HOSTLD=$LLVM_PATH/bin/ld.lld \
  HOSTLDFLAGS=-fuse-ld=lld"

$KERNEL_SRC_PATH/scripts/kconfig/merge_config.sh \
  -O $OUT_PATH \
  -m $KERNEL_SRC_DIR/arch/arm64/configs/vendor/oculus_anorak_defconfig $KERNEL_SRC_DIR/arch/arm64/configs/vendor/oculus_eureka_defconfig

cp $OUT_PATH/.config $KERNEL_SRC_PATH/arch/arm64/configs/eureka_defconfig

make -C $KERNEL_SRC_PATH ${MAKE_ENV} eureka_defconfig
make -C $KERNEL_SRC_PATH ${MAKE_ENV} -j12
```
* Execute the script by providing the path to the LLVM and GCC compilers
  downloaded in the preceding step.
