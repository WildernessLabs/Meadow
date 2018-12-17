#!/bin/bash
git clone https://github.com/WildernessLabs/Meadow.git nuttx
pushd .
cd nuttx
git checkout feature/semihosting-wip
popd
git clone https://bitbucket.org/nuttx/tools.git
git clone https://github.com/WildernessLabs/apps
pushd .
cd apps
git checkout wip
popd
#
#   Configure the build.
#
pushd .
cd tools/kconfig-frontends
./configure --enable-mconf --disable-nconf --disable-gconf --disable-qconf
make
make install
popd
./nuttx/tools/configure.sh stm32f777zit6-meadow/mono
pushd .
cd nuttx
#
#   Note that the following step is expected to fail.
#
make
echo
echo The above command is expected to fail.
echo
popd
git clone https://github.com/WildernessLabs/mono
cd mono
git checkout wip-rebase
git clone https://github.com/WildernessLabs/corefx external/corefx