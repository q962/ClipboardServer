#! /bin/bash

cd `dirname $0`

GETTEXT_PACKAGE="io.github.q962.ClipboardServer"

top_srcdir=../src

xgettext \
    --directory="$top_srcdir" \
    --package-name=$GETTEXT_PACKAGE \
    --add-comments \
    --keyword=_ \
    --keyword=N_ \
    --keyword=C_:1c,2 \
    --keyword=NC_:1c,2 \
    --from-code=utf-8 \
    --no-wrap \
    --files-from="POTFILES.in" \
    -o $GETTEXT_PACKAGE.pot