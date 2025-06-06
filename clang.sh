#!/bin/bash

# Running clang over files
find ./src -regex '.*\.\(cpp\|hpp\|ipp\|cc\|cxx\)' -exec clang-format -i {}  \;
find ./tests -regex '.*\.\(cpp\|hpp\|ipp\|cc\|cxx\)' -exec clang-format -i {}  \;
clang-format -i main.cpp