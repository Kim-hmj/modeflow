rm -rf ../buildaarch64
mkdir ../buildaarch64
cd  ../buildaarch64
cmake ../src  -DCMAKE_TOOLCHAIN_FILE=./toolchain.cmake
