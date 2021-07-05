#pragma once

#include "object/identity.hpp"

extern "C" void gdb_initialize(ObjectIdentity *main);
extern "C" void gdb_notify();
