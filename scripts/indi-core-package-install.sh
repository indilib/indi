#!/bin/bash

set -e

command -v bsdtar >/dev/null 2>&1 && \
    $(command -v sudo) bsdtar -xvf packages/indi-core-package.tar --strip-components=1 -C / || \
    $(command -v sudo)   tar -hxvf packages/indi-core-package.tar --strip-components=1 -C /
