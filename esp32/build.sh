#!/bin/bash

#
#   This is really a convenience script allowing the build to be run from
#   the software source directory for the MeadowComms code.  It passes all
#   parameters on to the build script in the source of the repository.
#
#   Note that the build script in the root of the repository is the one
#   that should be maintained.  It needs to be there in order for the build
#   to work correctly in CI.
#
../build-esp32.sh "$@"
