OBJ := $(addprefix build/, dfile.o dprintf.o dragonbox.o fast_float.o dsanity.o dscanf.o)
WIN_OBJ := $(OBJ:.o=.exe.o)

DEP := $(OBJ:.o=.d) $(WIN_OBJ:.o=.d)

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
	-\rm -f $(OBJ) libdfile.a $(WIN_OBJ) dfile.lib dfile.dll libdfile.so a.out a.exe $(DEP)

-include $(DEP)
