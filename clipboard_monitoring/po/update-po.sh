#! /bin/bash

cd `dirname $0`

GETTEXT_PACKAGE="io.github.q962.ClipboardServer"

for po_file in *.po ; do
    msgmerge --update $po_file --backup=none $GETTEXT_PACKAGE.pot
    msgattrib --no-obsolete $po_file > $po_file.bak
    mv $po_file.bak $po_file
done