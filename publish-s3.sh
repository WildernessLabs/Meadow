#!/bin/bash

#
# 1. Install AWS CLI through Homebrew
#

if [[ $(command -v brew) == "" ]]; then
    echo "Installing Homebrew"
    /usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
else
    echo "Updating Homebrew"
    brew update
fi

PACKAGES=(
    awscli
)

echo "Installing packages..."
for pkg in ${PACKAGES[@]}
do
    if brew ls --versions $pkg > /dev/null; then
        if brew outdated | grep -q $pkg; then
            echo "Upgrading package $pkg..."
            brew upgrade $pkg
        else
            echo "Package $pkg already installed."
        fi
    else
        echo "Installing package $pkg..."
        brew install $pkg
    fi
done

#
# 2. Setup AWS access credentials
#

if [ "$TF_BUILD" == True ]; then
    export AWS_ACCESS_KEY_ID=$AWS_ACCESS_KEY_ID
    export AWS_SECRET_ACCESS_KEY=$AWS_SECRET_ACCESS_KEY
fi

#
# 3. Upload package to AWS S3 bucket
#

echo $AWS_ACCESS_KEY_ID
echo $AWS_SECRET_ACCESS_KEY