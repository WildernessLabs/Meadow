#!/bin/bash
set -ex

scriptdir="$( cd "$(dirname "$0")" ; pwd -P )"

if [ "$TF_BUILD" != True ]; then
  echo "This script is only meant to run on CI, exiting."
  exit 1
fi

GITHUB_PERSONAL_ACCESS_TOKEN=56034ef7c8d98122587ae55a86348722aaa4f73f

clean_submodule() {
    LOCALREPO=$1
    (cd $LOCALREPO && git clean -xfd)
}

clone_or_fetch_submodule_github() {
    REPO=$1
    LOCALREPO=$2
    echo "Cloning or update submodule $REPO into $LOCALREPO"
    git clone https://$GITHUB_PERSONAL_ACCESS_TOKEN@github.com/$REPO.git $LOCALREPO 2> /dev/null || git -C "$LOCALREPO" fetch
    clean_submodule $LOCALREPO
}

checkout_submodule_github() {
    REPO=$1
    LOCALREPO=$2
    clone_or_fetch_submodule_github $REPO $LOCALREPO
    HASH=`git submodule status $LOCALREPO | awk '{print $1;}'`
    (cd $LOCALREPO && git reset --hard $HASH)
    clean_submodule $LOCALREPO
}

checkout_submodule() {
    REPO=$1
    LOCALREPO=$2
    echo "Cloning or update submodule $REPO into $LOCALREPO"
    git clone $REPO $LOCALREPO 2> /dev/null || git -C "$LOCALREPO" fetch
    HASH=`git submodule status $LOCALREPO | awk '{print $1;}'`
    (cd $LOCALREPO && git reset --hard $HASH)
    clean_submodule $LOCALREPO
}

git submodule init
checkout_submodule_github "WildernessLabs/apps" "apps"
checkout_submodule_github "WildernessLabs/nuttx" "nuttx"
checkout_submodule "https://bitbucket.org/nuttx/tools.git" "tools"
checkout_submodule_github "WildernessLabs/toolchain" "toolchain"

checkout_submodule_github "WildernessLabs/mono" "mono"
clone_or_fetch_submodule_github "WildernessLabs/corefx" "mono/external/corefx"

cd $scriptdir/..
rm -rf Meadow.CLI
checkout_submodule_github "WildernessLabs/Meadow.CLI" "Meadow.CLI"
