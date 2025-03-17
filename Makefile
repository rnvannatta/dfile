OBJ := dfile.o dprintf.o dragonbox.o dsanity.o
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

dfile.exe.o dfile.o : dfile.h
dprintf.exe.o dprintf.o : dfile.h dragonbox.h
dragonbox.exe.o dragonbox.o : dragonbox.h dragonbox.inl
dsanity.exe.o dsanity.o : dfile.h
