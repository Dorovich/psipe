KBLUE := "\e[1;36m"
KNORM := "\e[0m"

build_dir := build/
include_dir := $(abspath ../../../include/)
includes := $(addprefix -I, $(include_dir))
cflags := -static -static-libgcc -Wall -Werror -O2 $(includes)
targets := master0 chiplet0 master1 chiplet1 master-mm chiplet-mm
util := $(build_dir)pnvl_util.o

CC := riscv64-linux-gnu-gcc

.PHONY : all
all: $(targets) $(util)

$(build_dir)%.o : %.c Makefile | $(build_dir)
	@printf $(KBLUE)"---- building $@ ----\n"$(KNORM)
	$(CC) $(cflags) -c -MMD -MP $< -o $@

$(targets): %: $(build_dir)%.o $(util)
	@printf $(KBLUE)"---- linking $@ ----\n"$(KNORM)
	$(CC) $(cflags) -o $@ $(util) $<

$(build_dir):
	@printf $(KBLUE)"---- create $@ dir ----\n"$(KNORM)
	mkdir -p $(build_dir)

.PHONY : clean
clean:
	@printf $(KBLUE)"---- cleaning ----\n"$(KNORM)
	rm -rf $(targets)
	rm -rf $(build_dir)
