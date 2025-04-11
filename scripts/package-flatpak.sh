#!/usr/bin/bash

cd `dirname $0`

git ls-files --recurse-submodules .. | tar caf archive.tar -T-
flatpak-builder --state-dir=../build/.flatpak-build ../build/flatpak-build-dir flatpak.yml  --force-clean --user --install --disable-updates --repo=../build/.flatpak-repo
flatpak build-bundle ../build/.flatpak-repo io.github.q962.ClipboardServer.flatpak  io.github.q962.ClipboardServer stable