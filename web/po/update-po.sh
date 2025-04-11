#!/usr/bin/bash

set -e

cd `dirname $0`/..

npx vue-gettext-extract --config po/gettext.config.mjs
npx vue-gettext-compile --config po/gettext.config.mjs
