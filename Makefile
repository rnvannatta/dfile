OBJ := dfile.o dprintf.o dragonbox.o
WIN_OBJ := $(OBJ:.o=.exe.o)

.PHONY: clean all

dfile.a : $(OBJ)
	ar rcs $@ $^

%.o : %.c
	gcc -c -o $@ $< -g

%.o : %.cpp
	gcc -c -o $@ $<

dfile.lib : $(WIN_OBJ)
	/usr/bin/x86_64-w64-mingw32-ar rcs $@ $^

%.exe.o : %.c
	/usr/bin/x86_64-w64-mingw32-gcc -c -o $@ $<

%.exe.o : %.cpp
	/usr/bin/x86_64-w64-mingw32-gcc -c -o $@ $< -fno-exceptions

all : dfile.a dfile.lib

clean :
	-\rm -f $(OBJ) dfile.a $(WIN_OBJ) dfile.lib

dfile.o : dfile.h
dfile_printf.o : dfile.h dragonbox.h
dragonbox.o : dragonbox.h dragonbox.inl
