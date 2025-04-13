OBJ := $(addprefix build/, dfile.o dprintf.o dragonbox.o fast_float.o dsanity.o dscanf.o)
WIN_OBJ := $(OBJ:.o=.exe.o)
EM_OBJ := $(OBJ:.o=.em.o)

DEP := $(OBJ:.o=.d) $(WIN_OBJ:.o=.d) $(EM_OBJ:.o=.d)

.PHONY: clean all

CFLAGS += -Iinclude -g

libdfile.so : $(OBJ)
	gcc -shared -o $@ $^

build/%.o : src/%.c
	@mkdir -p $(dir $@)
	gcc -c -o $@ $< -fPIC -MMD -MP $(CFLAGS)

build/%.o : src/%.cpp
	@mkdir -p $(dir $@)
	gcc -c -o $@ $< -fPIC -MMD -MP -fvisibility=hidden -Os $(CFLAGS)

# Remember: DFILE is LGPL, so if you statically link, your software has to be GPL or you have to linkable object files
#libemdfile.a : $(EM_OBJ)
#	emar rcs $@ $^

build/%.em.o : src/%.c
	@mkdir -p $(dir $@)
	emcc -Wno-gnu -c -o $@ $< -MMD -MP $(CFLAGS)

build/%.em.o : src/%.cpp
	@mkdir -p $(dir $@)
	emcc -c -o $@ $< -MMD -MP -Os $(CFLAGS)

dfile.dll : $(WIN_OBJ)
	/usr/bin/x86_64-w64-mingw32-gcc -shared -o $@ $^ -lgcc_eh

build/%.exe.o : src/%.c
	@mkdir -p $(dir $@)
	/usr/bin/x86_64-w64-mingw32-gcc -c -o $@ $< -MMD -MP $(CFLAGS)

build/%.exe.o : src/%.cpp
	@mkdir -p $(dir $@)
	/usr/bin/x86_64-w64-mingw32-gcc -c -o $@ $< -fno-exceptions -MMD -MP -fvisibility=hidden -Os $(CFLAGS)

all : libdfile.so dfile.dll

clean :
	-\rm -f $(OBJ) $(WIN_OBJ) $(EM_OBJ) dfile.dll libdfile.so libemdfile.a a.out a.exe $(DEP)

-include $(DEP)
