#!/bin/bash

get_git_commit_hash() {
  REPO_PATH=$1
  echo `git -C $REPO_PATH rev-parse HEAD`
}

get_git_branch_or_tag() {
  REPO_PATH=$1
  echo `git -C $REPO_PATH describe --tags --exact-match 2> /dev/null || git -C $REPO_PATH symbolic-ref -q --short HEAD`
}

get_version_change_distance() {
  REPO_PATH=$1
  origin=$(git log -n 1 --oneline $scriptdir/version.txt  | cut -f 1 -d " ")
  distance=$(git log --oneline ${origin}..HEAD | wc -l)
  echo $distance
}

sedFriendly() {
  result=$(echo $1 | sed -r 's/([\$\.\*\/\[\\^])/\\\1/g'|sed 's/[]]/\[]]/g')
  echo $result
}

inject_value() {
  KEY=$1
  VALUE=$(eval echo '${'$KEY'}')
  # REPLACEMENT=$(sedFriendly $VALUE)
  FILE=$2
  sed -i.bak 's/###'${KEY}'###/'${VALUE}'/g' $FILE
}

generate_build_info() {
  printf "Generating build info..."

  MEADOW_GIT_HASH=$(get_git_commit_hash $scriptdir)
  MEADOW_GIT_REF=$(get_git_branch_or_tag $scriptdir)

  git checkout HEAD $scriptdir/version.txt

  read -r MEADOW_VERSION_STRING<$scriptdir/version.txt || true
  IFS='.' read -ra MEADOW_VERSION <<< "$MEADOW_VERSION_STRING"
  VERSION_MAJOR=${MEADOW_VERSION[0]}
  VERSION_MINOR=${MEADOW_VERSION[1]}
  VERSION_REVISION=${MEADOW_VERSION[2]}

  VERSION_BUILD=$(get_version_change_distance $1)

  echo "$VERSION_MAJOR.$VERSION_MINOR.$VERSION_REVISION.$VERSION_BUILD" > $scriptdir/esp32/version.txt

  echo Calculated version: ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_REVISION}.${VERSION_BUILD} '('${MEADOW_GIT_HASH:0-8}:${MEADOW_GIT_REF}')'

  git checkout HEAD $scriptdir/nuttx/configs/stm32f777zit6-meadow/scripts/user-space.ld
  git checkout HEAD $scriptdir/nuttx/include/meadow/hcom_nuttx_shared.h
  git checkout HEAD $scriptdir/esp32/main/version.h

  #
  # Get the date / time components in UTC format.
  #
  # These macros may look odd but the date foramtting can return date componets in the form
  # 00, 01, 02 etc and these when compiled are taken as octal numbers.  This means 09 is an
  # invalid number for the compiler so it it is necessary to remove the leading 0 and put it
  # back when formatting the date/time output for the user.
  #
  BUILD_DAY=$((10#`date -u +"%d"`))
  BUILD_TWO_DIGIT_DAY=`date -u +"%d"`
  BUILD_MONTH=$((10#`date -u +"%m"`))
  BUILD_TWO_DIGIT_MONTH=`date -u +"%m"`
  BUILD_MONTH_NAME=`date -u +"%b"`
  BUILD_YEAR=$((10#`date -u +"%y"`))
  BUILD_HOUR=$((10#`date -u +"%H"`))
  BUILD_TWO_DIGIT_HOUR=`date -u +"%H"`
  BUILD_MINUTE=$((10#`date -u +"%M"`))
  BUILD_TWO_DIGIT_MINUTE=`date -u +"%M"`
  BUILD_SECOND=$((10#`date -u +"%S"`))
  BUILD_TWO_DIGIT_SECOND=`date -u +"%S"`
  BUILD_HASH="0x${MEADOW_GIT_HASH:0-8}"
  BUILD_HASH_STRING="${MEADOW_GIT_HASH:0-8}"
  BUILD_EPOCH_TIME=`date -u +"%s"`

  for s in $(echo VERSION_MAJOR VERSION_MINOR VERSION_REVISION VERSION_BUILD BUILD_DAY BUILD_TWO_DIGIT_DAY BUILD_MONTH BUILD_TWO_DIGIT_MONTH BUILD_MONTH_NAME BUILD_YEAR BUILD_HOUR BUILD_TWO_DIGIT_HOUR HOUR BUILD_MINUTE BUILD_TWO_DIGIT_MINUTE BUILD_SECOND BUILD_TWO_DIGIT_SECOND BUILD_HASH MEADOW_GIT_REF BUILD_EPOCH_TIME BUILD_HASH_STRING)
  do
    inject_value $s $scriptdir/nuttx/configs/stm32f777zit6-meadow/scripts/user-space.ld
    inject_value $s $scriptdir/nuttx/include/meadow/hcom_nuttx_shared.h
    inject_value $s $scriptdir/esp32/main/build_info.template
  done
  cp $scriptdir/esp32/main/build_info.template $scriptdir/esp32/main/build_info.h

  #
  # If there are no arguments then we assume that we are building for the ESP32
  # and there is no need to generate the momo GIT referencce.
  #
  # This is ignored as the ESP32 docker image does not contain the xdd command.
  #
  if [ $# -eq 0 ]; then
    MONO_GIT_REF=''
    BYTE_COUNT=0
    for b in `xxd -p -c 1 <<<$MEADOW_GIT_REF`
    do
      if [ $BYTE_COUNT -lt 32 ]; then
        if [ "$b" != "0a" ]; then
          MONO_GIT_REF+="BYTE(0x$b)"
          BYTE_COUNT=$((BYTE_COUNT+1))
        fi
      fi
    done
    MONO_GIT_REF+="BYTE(00)"
    sed -i.bak 's/###MONO_GIT_REF###/'$MONO_GIT_REF'/g' $scriptdir/nuttx/configs/stm32f777zit6-meadow/scripts/user-space.ld
  fi

# Generate build-info.json file
BUILD_DATE="`date +"%F %T"`"
BUILD_HASH="`echo "$BUILD_DATE" | shasum -a 256 | awk '{print $1}'`"

JSON=$(cat <<-END
{
  "git": {
    "meadow": [ "$MEADOW_GIT_HASH", "$MEADOW_GIT_REF" ]
  },
  "build-date": "$BUILD_DATE",
  "build-hash": "$BUILD_HASH"
}
END
)
  echo "$JSON" > $scriptdir/nuttx/build-info.json

  printf " ${green}success${reset}\n"
}

#
#   Restore auto-versioned files
#
restore_versioned_files() {
  git checkout HEAD $scriptdir/nuttx/configs/stm32f777zit6-meadow/scripts/user-space.ld
  git checkout HEAD $scriptdir/nuttx/include/meadow/hcom_nuttx_shared.h
  git checkout HEAD $scriptdir/esp32/version.txt
  git checkout HEAD $scriptdir/esp32/main/build_info.template
  rm $scriptdir/nuttx/configs/stm32f777zit6-meadow/scripts/user-space.ld.bak
  rm $scriptdir/nuttx/include/meadow/hcom_nuttx_shared.h.bak
  rm $scriptdir/esp32/main/build_info.template.bak
  rm $scriptdir/esp32/main/build_info.h
}