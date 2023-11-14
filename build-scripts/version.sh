#!/bin/bash -e
# SPDX-License-Identifier: LGPL-2.1-or-later

VERSION=0.7.0
IS_RELEASE=false

function short(){
    echo ${VERSION}
}

function long(){
    echo "$(short)-$(release)"
}

function release(){
    # Package release

    if [ $IS_RELEASE = true ]; then
        # Used for official releases. Increment if necessary
        RELEASE="1"
    else
        # Used for nightly builds
        RELEASE="0.$(date +%04Y%02m%02d%02H%02M).git$(git rev-parse --short ${GITHUB_SHA:-HEAD})"
    fi
    echo $RELEASE
}

[ -z $1 ] && short || $1