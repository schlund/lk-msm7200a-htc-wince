export TOOLCHAIN_PREFIX=arm-iwmmxt-linux-gnueabi-
export PATH=$PATH:/home/darkenangel/CodeSourcery/iwmmxt/bin
_XPROJ=msm7200a_htc_wince
colormake $_XPROJ clean
colormake $_XPROJ
