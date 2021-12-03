#pragma once

extern const char * libname();

typedef int (*greek_t)(char*,size_t);

extern int _alpha(char * str, size_t size) __attribute__((weak));
extern int _beta(char * str, size_t size) __attribute__((weak));
extern int _gamma(char * str, size_t size) __attribute__((weak));
extern int _delta(char * str, size_t size) __attribute__((weak));
extern int _epsilon(char * str, size_t size) __attribute__((weak));
extern int _zeta(char * str, size_t size) __attribute__((weak));
extern int _eta(char * str, size_t size) __attribute__((weak));
extern int _theta(char * str, size_t size) __attribute__((weak));
extern int _iota(char * str, size_t size) __attribute__((weak));
extern int _kappa(char * str, size_t size) __attribute__((weak));
extern int _lambda(char * str, size_t size) __attribute__((weak));
