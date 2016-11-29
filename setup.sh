#!/bin/sh

set -e

git submodule sync --recursive
git submodule update --init --recursive
