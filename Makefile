.PHONY: all
all:

.PHONY: lint
lint:
	clang-format lpc/reform2_lpc.c | diff -u lpc/reform2_lpc.c -
	shfmt --posix --simplify --binary-next-line --case-indent --indent 2 --diff \
		bin sbin kernel/* initramfs-tools/*/* flash-kernel/*/*
	black --check --diff bin/reform-compstat libexec/reform-tools/reform-tray.py libexec/reform-tools/reform-wallpaper.py
	black --line-length 120 --check --diff sbin/reform-mcu-tool
	shellcheck bin/* sbin/* kernel/* initramfs-tools/*/* flash-kernel/*/* debian/reform-tools.postinst
