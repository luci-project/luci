#pragma once

typedef void (*func_t)(int);

void foo(int i);
func_t get_bar();
void baz(int i);
void oneshot_bar(int i);
void indirect_foo(int i);
extern func_t baz_ptr;
