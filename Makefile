# Azravibe Recorder — build system.
#
# Two modes:
#   1) Dev sandbox build (this machine): uses the local sysroot produced by
#      extracting .deb packages — no root required.
#         make            (auto-detects /tmp/opencode/sysroot)
#   2) Normal Linux machine with -dev packages installed system-wide:
#         make SYSROOT=   (pkg-config based)
#
# Targets:
#   make            build ./build/azravibe-recorder
#   make run        build + run
#   make smoke      headless 3 s record smoke test (offscreen)
#   make shot       render window to /tmp/opencode/azravibe-shot.png (offscreen)
#   make install    install to ~/.local (binary, launcher, icon, .desktop)
#   make uninstall  remove installed files
#   make clean

SYSROOT ?= $(HOME)/.local/opt/azravibe-recorder/sysroot
BIN     := build/azravibe-recorder

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter
LDFLAGS  ?= -Wl,--no-undefined

ifneq ($(wildcard $(SYSROOT)/usr/include),)
  # ---------- sysroot mode ----------
  QTROOT  := $(SYSROOT)/usr/include/x86_64-linux-gnu/qt6
  INC     := -I$(QTROOT) -I$(QTROOT)/QtCore -I$(QTROOT)/QtGui -I$(QTROOT)/QtWidgets \
             -I$(SYSROOT)/usr/include/x86_64-linux-gnu \
             -I$(SYSROOT)/usr/include \
             -I$(SYSROOT)/usr/include/X11
  LIBDIR  := $(SYSROOT)/usr/lib/x86_64-linux-gnu
  DEFS    := -DQT_CORE_LIB -DQT_GUI_LIB -DQT_WIDGETS_LIB -DNDEBUG
  LIBS    := -L$(LIBDIR) -Wl,-rpath,$(LIBDIR) \
             -lQt6Widgets -lQt6Gui -lQt6Core \
             -lavformat -lavcodec -lswscale -lswresample -lavutil \
             -lpulse -lpulse-simple -lX11 -lXext -lXrandr -lXfixes \
             -ldl -lm -lpthread
  CXXFLAGS += -fPIC
else
  # ---------- system mode ----------
  PKG_DEPS := Qt6Widgets Qt6Gui Qt6Core \
              x11 xext xrandr xfixes libpulse libpulse-simple \
              libavformat libavcodec libswscale libswresample libavutil
  INC     := $(shell pkg-config --cflags-only-I $(PKG_DEPS)) -D_GLIBCXX_ASSERTIONS
  DEFS    := $(shell pkg-config --cflags-only-other $(PKG_DEPS))
  LIBS    := $(shell pkg-config --libs $(PKG_DEPS)) -ldl -lpthread
endif

SRC := $(wildcard src/*.cpp src/*/*.cpp)
OBJ := $(patsubst src/%.cpp,build/obj/%.o,$(SRC))

.PHONY: all clean run smoke shot install uninstall
all: $(BIN)

build/obj/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(DEFS) $(INC) -MMD -MP -c $< -o $@

$(BIN): $(OBJ)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(OBJ) -o $@ $(LDFLAGS) $(LIBS)

-include $(OBJ:.o=.d)

run: all
	./azravibe-recorder

# Headless end-to-end smoke test: record ~3 s of the screen with the default
# target/source, then quit. Requires an X11 session for capture; the GUI runs offscreen.
smoke: all
	QT_QPA_PLATFORM=offscreen AZRAVIBE_SMOKE_SECONDS=3 ./azravibe-recorder

shot: all
	QT_QPA_PLATFORM=offscreen AZRAVIBE_SHOT=/tmp/opencode/azravibe-shot.png ./azravibe-recorder

PREFIX ?= $(HOME)/.local

install: all
	install -d $(PREFIX)/bin $(PREFIX)/share/applications $(PREFIX)/share/icons/hicolor/scalable/apps
	install -m 755 $(BIN) $(PREFIX)/bin/azravibe-recorder.bin
	install -m 755 azravibe-recorder $(PREFIX)/bin/azravibe-recorder
	install -m 644 assets/azravibe-recorder.svg $(PREFIX)/share/icons/hicolor/scalable/apps/azravibe-recorder.svg
	sed 's|@BIN@|azravibe-recorder|g' assets/azravibe-recorder.desktop > $(PREFIX)/share/applications/azravibe-recorder.desktop
	-update-desktop-database $(PREFIX)/share/applications 2>/dev/null || true
	@echo "Installed to $(PREFIX). Ensure $(PREFIX)/bin is on your PATH."

uninstall:
	rm -f $(PREFIX)/bin/azravibe-recorder $(PREFIX)/bin/azravibe-recorder.bin
	rm -f $(PREFIX)/share/applications/azravibe-recorder.desktop
	rm -f $(PREFIX)/share/icons/hicolor/scalable/apps/azravibe-recorder.svg

clean:
	rm -rf build
