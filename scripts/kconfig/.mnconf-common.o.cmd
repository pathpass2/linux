savedcmd_scripts/kconfig/mnconf-common.o := gcc -Wp,-MMD,scripts/kconfig/.mnconf-common.o.d -Wall -Wmissing-prototypes -Wstrict-prototypes -O2 -fomit-frame-pointer -std=gnu11   -I ./scripts/include   -c -o scripts/kconfig/mnconf-common.o scripts/kconfig/mnconf-common.c

source_scripts/kconfig/mnconf-common.o := scripts/kconfig/mnconf-common.c

deps_scripts/kconfig/mnconf-common.o := \
  scripts/include/list.h \
  scripts/include/list_types.h \
  scripts/kconfig/expr.h \
  scripts/kconfig/mnconf-common.h \

scripts/kconfig/mnconf-common.o: $(deps_scripts/kconfig/mnconf-common.o)

$(deps_scripts/kconfig/mnconf-common.o):
