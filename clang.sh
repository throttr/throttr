#!/bin/bash

# Running clang over files
find ./src -regex '.*\.\(cpp\|hpp\|cc\|cxx\)' -exec clang-format -i {}  \;
find ./tests -regex '.*\.\(cpp\|hpp\|cc\|cxx\)' -exec clang-format -i {}  \;