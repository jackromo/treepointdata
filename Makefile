# gcc main.c treepoints.c -o ./build/treepoints  && mv build/treepoints . && ./treepoints

MINGCC32=i686-w64-mingw32-gcc
MINGCC64=x86_64-w64-mingw32-gcc

DATADIR=data
BUILDDIR=build

CFLAGS=-Wno-implicit-int -lm -g -O0
SOURCES=main.c treepoints.c
EXEC=treepoints

linux:
	@$(CC) $(SOURCES) -o $(BUILDDIR)/$(EXEC) $(CFLAGS)

mingw32:
	@$(MINGCC64) $(SOURCES) -o $(BUILDDIR)/$(EXEC) $(CFLAGS)

mingw64:
	@$(MINGCC64) $(SOURCES) -o $(BUILDDIR)/$(EXEC) $(CFLAGS)

clean:
	@rm $(BUILDDIR)/*
	@touch $(BUILDDIR)/.keep
