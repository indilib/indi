#!/bin/bash

set -e

$(command -v sudo) tar -hxvf packages/indi-core-package.tar --strip-components=1 -C /
