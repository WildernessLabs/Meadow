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
checkout_submodule_github "WildernessLabs/mbedtls" "mbedtls"

#
# Clone the dotnet/runtime fork (sibling directory, not a submodule).
# The build scripts expect it at ../runtime relative to this repo.
#
RUNTIME_REPO="WildernessLabs/runtime"
RUNTIME_DIR="$scriptdir/../runtime"
echo "Cloning or updating runtime repo into $RUNTIME_DIR"
git clone "https://${GITHUB_PERSONAL_ACCESS_TOKEN}@github.com/${RUNTIME_REPO}.git" "$RUNTIME_DIR" 2>/dev/null || git -C "$RUNTIME_DIR" fetch
# Check out the branch that matches this Meadow branch, or fall back to main
CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
if git -C "$RUNTIME_DIR" rev-parse --verify "origin/$CURRENT_BRANCH" &>/dev/null; then
  git -C "$RUNTIME_DIR" checkout "$CURRENT_BRANCH"
  git -C "$RUNTIME_DIR" pull origin "$CURRENT_BRANCH"
else
  git -C "$RUNTIME_DIR" checkout main
  git -C "$RUNTIME_DIR" pull origin main
fi

clean_submodule .

git submodule
