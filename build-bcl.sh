#!/bin/bash -ex

#set -eo
scriptdir="$( cd "$(dirname "$0")" ; pwd -P )"

if [[ $("uname -p") -eq "arm" ]]; then
  arch -x86_64 $scriptdir/build-mono-bcl.sh $@
else
    $scriptdir/build-mono-bcl.sh $@
fi
