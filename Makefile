#
#                       rt top level Makefile
#

# The default goal is...
.DEFAULT_GOAL = all

# Use V=1 argument for verbose builds
QUIET_  = @
QUIET   = $(QUIET_$(V))

###############
# Portability #
###############
# Program paths.
export CPPLINT   := scripts/cpplint.py
export CXX       := clang++
export RM        := rm -rf
export SHELL     := /bin/bash
# Make configuration:
export MAKEFLAGS := "-j $(SHELL nproc)"

#################
# Build options #
#################
# Infer whether we're using the clang++ compiler, else assume g++:
ifeq ($(CXX),clang++)
IsClang = 1
endif

# Whether to enable support for gprof profiling tool:
GPROF_ENABLED = 0
# The optimisation level to use:
OPTIMISATION_LEVEL = -O2
# The C++ standard to use:
CPP_STANDARD = c++14

# Compile-time flags.
CxxFlags =			\
	$(OPTIMISATION_LEVEL)	\
	-pedantic		\
	-Wall			\
	-Wextra			\
	-std=$(CPP_STANDARD)	\
	-Wno-unused-parameter	\
	$(NULL)

# Link-time flags.
LdFlags =			\
	-ltbb			\
	$(NULL)

# Compiler warnings.
CxxWarnings =			\
	cast-align		\
	cast-qual		\
	ctor-dtor-privacy	\
	disabled-optimization	\
	format=2		\
	frame-larger-than=1024	\
	init-self		\
	inline			\
	larger-than=2048	\
	missing-declarations	\
	missing-include-dirs	\
	no-div-by-zero		\
	no-main			\
	old-style-cast		\
	overloaded-virtual	\
	padded			\
	redundant-decls		\
	shadow			\
	sign-conversion		\
	sign-promo		\
	strict-overflow=5	\
	switch-default		\
	undef			\
	write-strings		\
	$(NULL)

ifeq ($(IsClang),1)
# Clang specific warnings.
CxxWarnings +=			\
	$(NULL)
else
# GCC specific warnings.
CxxWarnings +=			\
	logical-op		\
	noexcept		\
	stack-usage=1024	\
	strict-null-sentinel	\
	$(NULL)
endif

CxxFlags += $(addprefix -W,$(CxxWarnings))

# Compiler warnings to ignore.
CxxDisabledWarnings =		\
	missing-braces		\
	$(NULL)

CxxFlags += $(addprefix -Wno-,$(CxxDisabledWarnings))

# Enable GPROF flags if required.
ifeq ($(GPROF_ENABLED),1)
CxxFlags += -pg
endif


###########
# Targets #
###########
RayTracerSources =		\
	graphics.cc		\
	lights.cc		\
	objects.cc		\
	profiling.cc		\
	random.cc		\
	renderer.cc		\
	$(NULL)

RayTracerHeaders =		\
	camera.h		\
	graphics.h		\
	image.h			\
	lights.h		\
	math.h			\
	profiling.h		\
	random.h		\
	renderer.h		\
	rt.h			\
	scene.h			\
	$(NULL)

RayTracerSourceDir = src
RayTracerHeaderDir = include

CxxFlags += -I$(RayTracerHeaderDir)

Sources = $(addprefix $(RayTracerSourceDir)/,$(RayTracerSources))
Headers = $(addprefix $(RayTracerHeaderDir)/rt/,$(RayTracerHeaders))

Library = $(RayTracerSourceDir)/librt.so
MkScene = ./scripts/mkscene.py

Objects = $(patsubst %.cc,%.o,$(Sources))
CleanFiles = $(Binary) $(Objects) $(Scene)


##########
# Linter #
##########

CpplintExtension = .lint

# The cpplint script checks an input source file and enforces the
# style guidelines set out in:
#
#   http://google-styleguide.googlecode.com/svn/trunk/cppguide.xml
#
LintFiles = $(addsuffix $(CpplintExtension),$(Sources) $(Headers))
CleanFiles += $(LintFiles)

# Arguments to --filter flag for cpplint.
CpplintFilters = -legal,-build/c++11,-readability/streams,-readability/todo

# Explicit target for creating lint files:
$(LintFiles): %$(CpplintExtension): %
	@$(call cpplint,$<,$@)

# Function for generating lint files.
define cpplint
$(CPPLINT) --root=include --filter=$(CpplintFilters) $1 2>&1	\
	| grep -v '^Done processing\|^Total errors found: ' 	\
	| tee $2
endef


#########
# Rules #
#########

all: lib examples/example1 examples/example2

# Examples.
examples/example1: examples/example1.cc $(Library)
	@echo '  CXXLD    $(notdir $@)'
	$(QUIET)$(CXX) $(CxxFlags) $(LdFlags) -ldl $^ -o $@

examples/example2: examples/example2.cc $(Library)
	@echo '  CXXLD    $(notdir $@)'
	$(QUIET)$(CXX) $(CxxFlags) $(LdFlags) -ldl $^ -o $@

examples/example2.cc: examples/example2.rt $(MkScene)
	@echo '  MKSCENE  $(notdir $@)'
	$(QUIET)$(MkScene) $< $@

CleanFiles += examples/example1 examples/example2 examples/example2.cc

# Library target.
lib: $(Library) $(LintFiles)

$(Library): $(Objects)
	@echo '  LD       $(notdir $@)'
	$(QUIET)$(CXX) $(CxxFlags) $(LdFlags) -fPIC -shared $? -o $@

%.o: %.cc $(Headers)
	@echo '  CXX      $(notdir $@)'
	$(QUIET)$(CXX) $(CxxFlags) -fPIC -c $< -o $@
	$(QUIET)$(call cpplint,$<,$<$(CpplintExtension))

# Clean up.
.PHONY: clean
clean:
	$(RM) $(CleanFiles)
