#!/bin/bash
# Setup git-subrepo for RHEL

git clone https://github.com/ingydotnet/git-subrepo ~/.local/git-subrepo

echo 'source ~/.local/git-subrepo/.rc' >> ~/.bashrc
source ~/.bashrc

git subrepo --version
