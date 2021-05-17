#pragma once

#include <optional>
#include <ostream>

#include "strptr.hpp"
#include "object_identity.hpp"
#include "object.hpp"
#include "versioned_symbol.hpp"

std::ostream& operator<<(std::ostream& os, const StrPtr & s);
std::ostream& operator<<(std::ostream& os, const ObjectIdentity & o);
std::ostream& operator<<(std::ostream& os, const Object & o);
std::ostream& operator<<(std::ostream& os, const VersionedSymbol & s);
std::ostream& operator<<(std::ostream& os, const std::optional<VersionedSymbol> & s);
