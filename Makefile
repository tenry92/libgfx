prefix := /usr/local
exec_prefix := $(prefix)
includedir := $(prefix)/include
libdir := $(prefix)/lib
datarootdir := /usr/local/share
docdir := $(datarootdir)/doc/libgfx
pkgconfigdir = $(libdir)/pkgconfig

SHELL := /bin/sh

CXXFLAGS := -std=c++0x `pkg-config --cflags libpng` -Iinclude


.PHONY: all install doc clean uninstall


all: lib/libgfx.a doc

clean:
	@rm -rf obj

install: all
	@echo Copying libgfx.a to $(libdir)/libgfx.a...
	@cp lib/libgfx.a $(libdir)/libgfx.a
	@echo Copying libgfx.h to $(includedir)/libgfx.h...
	@cp include/libgfx.h $(includedir)/libgfx.h
	@echo Copying documentation to $(docdir)...
	@mkdir -p $(docdir)
	@cp -r -t $(docdir) doc/*
	@echo Generating $(pkgconfigdir)/libgfx.pc...
	@echo 'libdir=$(libdir)\nincludedir=$(includedir)\n' | cat - libgfx.pc > $(pkgconfigdir)/libgfx.pc

uninstall:
	@echo Removing $(pkgconfigdir)/libgfx.pc...
	@rm -f $(pkgconfigdir)/libgfx.pc
	@echo Removing $(docdir)...
	@rm -rf $(docdir)
	@echo Removing $(includedir)/libgfx.h
	@rm -f $(includedir)/libgfx.h
	@echo Removing $(libdir)/libgfx.a...
	@rm -f $(libdir)/libgfx.a...

doc:
	@echo Generating documentation...
	@cd doc && doxygen > /dev/null 2>&1

lib/libgfx.a: obj/libgfx.o
	@echo Generating archive...
	@mkdir -p lib
	@ar rvs lib/libgfx.a obj/libgfx.o

obj/libgfx.o: src/libgfx.cpp
	@echo Compiling...
	@mkdir -p obj
	@$(CXX) $(CXXFLAGS) -o $@ -c $^