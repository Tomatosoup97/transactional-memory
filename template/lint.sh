#!/bin/zsh
set -e

cd ..
fd ".*\.(c|h)" include template | xargs -I % sh -c "echo formatting %; clang-format -i %"
