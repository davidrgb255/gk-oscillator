# gk-oscillator — graphic oscillator (single binary, no app DLL/plugins)
CC       ?= gcc
CXX      ?= g++
CFLAGS   ?= -O2 -g -Wall -Wextra -std=c11 -D_DEFAULT_SOURCE
CXXFLAGS ?= -O2 -g -Wall -Wextra -std=c++17
CFLAGS   += -Ithird_party -Isrc
CXXFLAGS += -Ithird_party -Isrc

QT_CFLAGS := $(shell pkg-config --cflags Qt6Widgets Qt6Gui Qt6Core 2>/dev/null)
QT_LIBS   := $(shell pkg-config --libs   Qt6Widgets Qt6Gui Qt6Core 2>/dev/null)

PA_CFLAGS := $(shell pkg-config --cflags portaudio-2.0 2>/dev/null)
PA_LIBS   := $(shell pkg-config --libs   portaudio-2.0 2>/dev/null)
ifeq ($(PA_LIBS),)
  PA_LIBS := -l:libportaudio.so.2
endif

MOC ?= $(shell command -v moc-qt6 2>/dev/null || command -v moc 2>/dev/null || \
  ls /usr/lib/qt6/libexec/moc /usr/lib/x86_64-linux-gnu/qt6/libexec/moc 2>/dev/null | head -1)

ifeq ($(QT_CFLAGS),)
  $(warning Qt6 not found via pkg-config — GUI will fail to link)
endif

CFLAGS   += $(QT_CFLAGS) $(PA_CFLAGS)
CXXFLAGS += $(QT_CFLAGS) $(PA_CFLAGS)
# Single process binary: no app-specific shared libs. System Qt + PortAudio linked normally.
LIBS     := -lpthread -lm -ldl $(QT_LIBS) $(PA_LIBS)

PREFIX      ?= /usr/local
DESTDIR     ?=
USER_PREFIX ?= $(HOME)/.local

CSRC   := src/audio.c src/ringbuf.c src/config.c
CXXSRC := src/main.cpp src/main_window.cpp src/scope_widget.cpp
OBJ    := $(CSRC:src/%.c=build/%.o) $(CXXSRC:src/%.cpp=build/%.o)
MOC_SRC := build/moc_main_window.cpp build/moc_scope_widget.cpp
MOC_OBJ := $(MOC_SRC:%.cpp=%.o)

.PHONY: all clean run install-user uninstall-user desktop-local

all: gk-oscillator

gk-oscillator: $(OBJ) $(MOC_OBJ)
	$(CXX) $(LDFLAGS) -o $@ $(OBJ) $(MOC_OBJ) $(LIBS)

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c -o $@ $<

build/%.o: src/%.cpp | build
	$(CXX) $(CXXFLAGS) -fPIC -c -o $@ $<

build/moc_main_window.cpp: src/main_window.h | build
	$(MOC) -o $@ $<

build/moc_scope_widget.cpp: src/scope_widget.h | build
	$(MOC) -o $@ $<

build/moc_%.o: build/moc_%.cpp | build
	$(CXX) $(CXXFLAGS) -fPIC -c -o $@ $<

$(OBJ): src/audio.h src/ringbuf.h src/config.h src/main_window.h src/scope_widget.h

build:
	mkdir -p build

clean:
	rm -rf build gk-oscillator

run: gk-oscillator
	./gk-oscillator

install-user: gk-oscillator
	install -d $(USER_PREFIX)/bin
	install -m 755 gk-oscillator $(USER_PREFIX)/bin/gk-oscillator
	install -d $(USER_PREFIX)/share/applications
	sed 's|^Exec=.*|Exec=$(USER_PREFIX)/bin/gk-oscillator|' packaging/gk-oscillator.desktop \
	  > $(USER_PREFIX)/share/applications/gk-oscillator.desktop
	@echo "Installed to $(USER_PREFIX)/bin/gk-oscillator"

uninstall-user:
	rm -f $(USER_PREFIX)/bin/gk-oscillator
	rm -f $(USER_PREFIX)/share/applications/gk-oscillator.desktop

desktop-local: gk-oscillator
	sed "s|^Exec=.*|Exec=$(CURDIR)/gk-oscillator|; s|^Icon=.*|Icon=applications-multimedia|" \
	  packaging/gk-oscillator.desktop > gk-oscillator.desktop
	@echo "Wrote ./gk-oscillator.desktop"
