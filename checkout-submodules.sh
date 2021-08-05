#!/bin/bash
set -ex

scriptdir="$( cd "$(dirname "$0")" ; pwd -P )"

if [ "$TF_BUILD" != True ]; then
  echo "This script is only meant to run on CI, exiting."
  exit 1
fi

# GITHUB_PERSONAL_ACCESS_TOKEN=<removed>
# need to set GITHUB_PERSONAL_ACCESS_TOKEN as env var with PAT

clean_submodule() {
    LOCALREPO=$1
    pushd .
    (cd $LOCALREPO && git clean -xffd)
    popd
}

reset_submodule() {
    LOCALREPO=$1
    HASH=$2
    pushd .
    (cd $LOCALREPO && git reset --hard $HASH)
    popd
}

clone_or_fetch_submodule_github() {
    REPO=$1
    LOCALREPO=$2
    echo "Cloning or update submodule $REPO into $LOCALREPO"
    git clone "https://${GITHUB_PERSONAL_ACCESS_TOKEN}@github.com/${REPO}.git" $LOCALREPO 2> /dev/null || git -C "$LOCALREPO" fetch
    clean_submodule $LOCALREPO
}

clone_or_fetch_submodule() {
    REPO=$1
    LOCALREPO=$2
    echo "Cloning or update submodule $REPO into $LOCALREPO"
    git clone $REPO $LOCALREPO 2> /dev/null || git -C "$LOCALREPO" fetch
    clean_submodule $LOCALREPO
}

checkout_submodule_github() {
    REPO=$1
    LOCALREPO=$2
    HASH=`git submodule status $LOCALREPO | awk '{print $1;}'`
    if [[ $HASH = +* ]]; then
        echo "Found unexpected Git submodule state"
        exit 1
    fi
    clone_or_fetch_submodule_github $REPO $LOCALREPO
    reset_submodule $LOCALREPO $HASH
    clean_submodule $LOCALREPO
}

checkout_submodule() {
    REPO=$1
    LOCALREPO=$2
    HASH=`git submodule status $LOCALREPO | awk '{print $1;}'`
    if [[ $HASH = +* ]]; then
        echo "Found unexpected Git submodule state"
        exit 1
    fi
    clone_or_fetch_submodule $REPO $LOCALREPO
    reset_submodule $LOCALREPO $HASH
    clean_submodule $LOCALREPO
}

git submodule init
git submodule update
git submodule

checkout_submodule "https://bitbucket.org/nuttx/tools.git" "tools"

checkout_submodule_github "WildernessLabs/mono" "mono"
checkout_submodule_github "WildernessLabs/mbedtls" "mbedtls"
clone_or_fetch_submodule_github "WildernessLabs/corefx" "mono/external/corefx"

clean_submodule .

git submodule
