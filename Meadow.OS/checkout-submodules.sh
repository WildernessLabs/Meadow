 #!/usr/bin/env sh

if [ "$TF_BUILD" != True ]; then
  echo "This script is only meant to run on CI, exiting."
  exit 1
fi

GITHUB_PERSONAL_ACCESS_TOKEN=56034ef7c8d98122587ae55a86348722aaa4f73f

checkout_submodule_github() {
    REPO=$1
    LOCALREPO=$2
    echo "Cloning or update submodule $REPO into $LOCALREPO"
    git clone https://$GITHUB_PERSONAL_ACCESS_TOKEN@github.com/$REPO.git $LOCALREPO 2> /dev/null || git -C "$LOCALREPO" fetch
    HASH=`git submodule status $LOCALREPO | awk '{print $1;}'`
    (cd $LOCALREPO && git reset --hard $HASH)
}

checkout_submodule() {
    REPO=$1
    LOCALREPO=$2
    echo "Cloning or update submodule $REPO into $LOCALREPO"
    git clone $REPO $LOCALREPO 2> /dev/null || git -C "$LOCALREPO" fetch
    HASH=`git submodule status $LOCALREPO | awk '{print $1;}'`
    (cd $LOCALREPO && git reset --hard $HASH)
}

git submodule init
checkout_submodule_github "WildernessLabs/apps" "apps"
checkout_submodule_github "WildernessLabs/nuttx" "nuttx"
checkout_submodule_github "WildernessLabs/mono" "mono"
checkout_submodule_github "WildernessLabs/nuttx" "nuttx"
checkout_submodule "https://bitbucket.org/nuttx/tools.git" "tools"
