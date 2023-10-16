cc = gcc

use_clang := $(if $(findstring clang, $(cc)), 1,)
windows := $(if $(findstring Windows_NT, $(OS)), 1,)

comma =,

build_dir := build
ff_dir := $(build_dir)/ffmpeg

basename := pixie

bin := $(addsuffix $(strip $(if $(windows), .exe)), $(basename))

static_lib := lib$(basename).a
shared_lib := $(addsuffix $(strip $(if $(windows), .dll, .so)), lib$(basename))

app_src_dirs := app
lib_src_dirs := src src/util
incl_dirs := incl

lib_src_files := $(foreach dir, $(lib_src_dirs), $(wildcard $(dir)/*.c))
app_src_files := $(foreach dir, $(app_src_dirs), $(wildcard $(dir)/*.c))
lib_obj_files := $(lib_src_files:%=$(build_dir)/%.o)
app_obj_files := $(app_src_files:%=$(build_dir)/%.o)

dep_files := $(lib_obj_files:.o=.d) $(app_obj_files:.o=.d)

warns := $(strip -Wall -Wextra -Wpedantic -Wshadow -Wformat=2 \
	-Wno-gnu-zero-variadic-macro-arguments -Wno-dollar-in-identifier-extension \
	-Wno-for-loop-analysis -Wno-cast-function-type)

includes := $(strip $(incl_dirs:%=-I%) $(if $(windows), -isystem$(ff_dir)/include))

ff_libs := $(strip $(if $(windows), -L$(ff_dir)/lib -lavcodec -lavformat -lavutil, \
	$(shell sh scripts/get-ff-libs.sh)))
ifeq ($(ff_libs),)
	exit 1
endif

base_cflags := -std=c2x $(warns)
cflags := $(strip $(includes) $(base_cflags) $(extra_cflags) -fPIC \
	$(if $(debug), -ggdb3 -DDEBUG, -O3 -ffast-math -funroll-loops -DNDEBUG) \
	$(if $(windows), -D__USE_MINGW_ANSI_STDIO) \
	$(sanitize:%=-fsanitize=%))

ldflags := $(strip -lm $(ff_libs) $(if $(use_clang), -fuse-ld=lld) \
	$(sanitize:%=-fsanitize=%) $(extra_ldflags))

default: gen-compile_flags-txt $(build_dir)/$(bin)

lib := $(addprefix $(build_dir)/, $(strip $(if $(shared), $(shared_lib), $(static_lib))))
$(build_dir)/$(bin): $(lib) $(app_obj_files)
	@$(cc) $(app_obj_files) -L$(build_dir) -l$(basename) $(ldflags) -o $@

$(build_dir)/$(shared_lib): $(lib_obj_files)
	@$(cc) $(lib_obj_files) $(ldflags) -shared \
		$(if $(windows), -Wl$(comma)--out-implib$(comma)$(build_dir)/$(shared_lib).a) -o $@

$(build_dir)/$(static_lib): $(lib_obj_files)
	@ar rcs $@ $?

$(build_dir)/%.c.o: %.c Makefile
	@mkdir -p $(dir $@) &
	@$(cc) $(cflags) -MMD -MP -c $< -o $@

-include $(dep_files)

clean:
	@rm -rf $(build_dir)/$(bin) $(build_dir)/$(static_lib) $(build_dir)/$(shared_lib) \
		$(build_dir)/$(shared_lib).a $(app_src_dirs:%=$(build_dir)/%) $(lib_src_dirs:%=$(build_dir)/%) &

clean-all:
	rm -rf $(build_dir)/*

gen-compile_flags-txt:
	@sh ./scripts/gen-compile_flags-txt.sh "$(base_cflags) $(includes)"

.PHONY: default clean clean-all gen-compile_flags-txt
