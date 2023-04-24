asm(".symver func_v0,foo@VERS_0");
asm(".symver func_v1,foo@@VERS_1");
asm(".symver func_v2,foo@VERS_2");

int func_v0() {
	return 23;
}

int func_v1() {
	return 42;
}

int func_v2() {
	return 1337;
}