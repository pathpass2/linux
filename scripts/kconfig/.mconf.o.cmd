savedcmd_scripts/kconfig/mconf.o := gcc -Wp,-MMD,scripts/kconfig/.mconf.o.d -Wall -Wmissing-prototypes -Wstrict-prototypes -O2 -fomit-frame-pointer -std=gnu11   -I ./scripts/include  -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=600 -c -o scripts/kconfig/mconf.o scripts/kconfig/mconf.c

source_scripts/kconfig/mconf.o := scripts/kconfig/mconf.c

deps_scripts/kconfig/mconf.o := \
  scripts/include/list.h \
  scripts/include/list_types.h \
  scripts/include/xalloc.h \
  scripts/kconfig/lkc.h \
    $(wildcard include/config/prefix) \
  scripts/kconfig/expr.h \
  scripts/kconfig/lkc_proto.h \
  scripts/kconfig/lxdialog/dialog.h \
  scripts/kconfig/mnconf-common.h \

scripts/kconfig/mconf.o: $(deps_scripts/kconfig/mconf.o)

$(deps_scripts/kconfig/mconf.o):
