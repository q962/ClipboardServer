#! /bin/bash

cd `dirname $0`

GETTEXT_PACKAGE="io.github.q962.ClipboardServer"

top_srcdir=../res/

for po_file in *.po ; do
	out_dir=$top_srcdir/locale/${po_file/%.po}/LC_MESSAGES/

	mkdir -p $out_dir
	msgfmt $po_file -o $out_dir/$GETTEXT_PACKAGE.mo
done