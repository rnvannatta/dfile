OBJ := dfile.o dprintf.o dragonbox.o fast_float.o dsanity.o dscanf.o
WIN_OBJ := $(OBJ:.o=.exe.o)

.PHONY: clean all

libdfile.so : $(OBJ)
	gcc -shared -o $@ $^

%.o : %.c
	gcc -c -o $@ $< -fPIC -g

%.o : %.cpp
	gcc -c -o $@ $< -fPIC -fvisibility=hidden -O2

dfile.dll : $(WIN_OBJ)
	/usr/bin/x86_64-w64-mingw32-gcc -shared -o $@ $^

%.exe.o : %.c
	/usr/bin/x86_64-w64-mingw32-gcc -c -o $@ $<

%.exe.o : %.cpp
	/usr/bin/x86_64-w64-mingw32-gcc -c -o $@ $< -fno-exceptions -fvisibility=hidden -O2

all : dfile.a dfile.lib libdfile.so

clean :
	-\rm -f $(OBJ) dfile.a $(WIN_OBJ) dfile.lib

dfile.exe.o dfile.o : dfile.h
dprintf.exe.o dprintf.o : dfile.h dragonbox.h dprintf.h
dscanf.exe.o dscanf.o : dfile.h fast_float.h dprintf.h
dragonbox.exe.o dragonbox.o : dragonbox.h dragonbox.inl
fast_float.exe.o fast_float.o : fast_float.h
dsanity.exe.o dsanity.o : dfile.h
