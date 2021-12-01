BIN := ../$(notdir $(lastword $(abspath .))).so

EXT_H    := h
EXT_HPP  := h hh hpp hxx h++
EXT_C    := c
EXT_CXX  := C cc cpp cxx c++

INCLUDE_DIR := ../include
SOURCE_DIR  := .

WILD_EXT  = $(strip $(foreach EXT,$($(1)),$(wildcard $(2)/*.$(EXT))))

HDRS_C   := $(call WILD_EXT,EXT_H,$(INCLUDE_DIR))
HDRS_CXX := $(call WILD_EXT,EXT_HPP,$(INCLUDE_DIR))
SRCS_C   := $(call WILD_EXT,EXT_C,$(SOURCE_DIR))
SRCS_CXX := $(call WILD_EXT,EXT_CXX,$(SOURCE_DIR))
OBJS     := $(SRCS_C:%=%.o) $(SRCS_CXX:%=%.o)

CC       := $(CC)
CCFLAGS  := -g -Wall -Wextra -Wfatal-errors -O2 -std=c11 -fPIC -I$(INCLUDE_DIR)
CXX      := $(CXX)
CXXFLAGS := -g -Wall -Wextra -Wfatal-errors -O2 -std=c++17 -fPIC -I$(INCLUDE_DIR)
LD       := $(if $(SRCS_CXX),$(CXX),$(CC))
LDFLAGS  := -g -shared
LDLIBS   :=

.PHONY: build clean

build: $(BIN)
clean:
	$(RM) $(OBJS) $(BIN)
	rm -f tests

define BUILD_C
%.$(1).o: %.$(1) $$(HDRS_C) Makefile
	$$(CC) $$(CCFLAGS) -c -o $$@ $$<
endef
$(foreach EXT,$(EXT_C),$(eval $(call BUILD_C,$(EXT))))

define BUILD_CXX
%.$(1).o: %.$(1) $$(HDRS_CXX) Makefile
	$$(CXX) $$(CXXFLAGS) -c -o $$@ $$<
endef
$(foreach EXT,$(EXT_CXX),$(eval $(call BUILD_CXX,$(EXT))))

$(BIN): $(OBJS) Makefile
	$(LD) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

lint:
	./lint.sh

tests: $(OBJS) Makefile
	$(CC) $(CCFLAGS) -pthread -o $@ $(OBJS) $(LDLIBS)

run_tests: clean lint tests
	./tests

grade:
	cd .. && ./grade.sh run

perf:
	cd .. && ./grade.sh perf

submit:
	cd .. && ./submit.sh
