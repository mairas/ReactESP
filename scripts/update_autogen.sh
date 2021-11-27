#!/usr/bin/env bash

# fail fast
set -euo pipefail

VERSION=$(cat VERSION)

# check that the repo is clean

if ! git diff-index --quiet HEAD --; then
    echo "Repo is not clean, aborting"
    exit 1
fi

# udpate the doxygen docs

git rm -rf docs/generated
mkdir -p docs/generated
doxygen
git add docs/generated
git commit -m "Update the Doxygen docs for version ${VERSION}"

