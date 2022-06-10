set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_SYSROOT /home/ryder/workspace/rivian/QCS40X_WS/LE.UM.4.1.2/apps_proc/poky/build/tmp-glibc/work/aarch64-oe-linux/r1-modeflow/1.0-r0/recipe-sysroot)
set(CMAKE_STAGING_PREFIX <./>) 

set(tools /home/ryder/R1_perf/QCS40X_WS/LE.UM.4.1.2/apps_proc/poky/build/tmp-glibc/work/aarch64-oe-linux/r1-modeflow/1.0-r0/recipe-sysroot-native/usr/bin/aarch64-oe-linux)
set(CMAKE_C_COMPILER ${tools}/aarch64-oe-linux-gcc)
set(CMAKE_CXX_COMPILER ${tools}/aarch64-oe-linux-g++)


set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s") 
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}  -print-search-dirs -s")

# where is the target environment 
SET(CMAKE_FIND_ROOT_PATH /home/ryder/workspace/rivian/QCS40X_WS/LE.UM.4.1.2/apps_proc/poky/build/tmp-glibc/work/aarch64-oe-linux/r1-modeflow/1.0-r0/recipe-sysroot-native /home/ryder/workspace/rivian/QCS40X_WS/LE.UM.4.1.2/apps_proc/poky/build/tmp-glibc/work/aarch64-oe-linux/r1-modeflow/1.0-r0/recipe-sysroot)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

