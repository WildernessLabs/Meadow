#!/bin/bash
git clone git@github.com:WildernessLabs/Meadow.git nuttx
pushd .
cd nuttx
git checkout gpio
popd
git clone https://bitbucket.org/nuttx/tools.git
git clone git@github.com:WildernessLabs/apps.git
pushd .
cd apps
git checkout gpio
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
git clone git@github.com:WildernessLabs/mono.git
cd mono
git checkout wip-rebase
git clone git@github.com:WildernessLabs/corefx.git external/corefx
