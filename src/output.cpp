#include "output.hpp"

#include "generic.hpp"

std::ostream& operator<<(std::ostream& os, const StrPtr & s) {
	if (s.str == nullptr)
		os << "(nullptr)";
	else
		os << s.str;
	return os;
}

std::ostream& operator<<(std::ostream& os, const ObjectIdentity & o) {
	return os << o.name << " (" << o.path << ")";
}

std::ostream& operator<<(std::ostream& os, const Object & o) {
	return os << "Object " << o.file;
}

std::ostream& operator<<(std::ostream& os, const VersionedSymbol & s) {
	if (s.valid()) {
		os << s.name();
		if (s.version.valid) {
			if (s.version.name != nullptr)
				os << "@" << s.version.name;
		} else {
			os << " [invalid version]";
		}
	} else {
		os << "*invalid*";
	}
	os << " (" << s.object().file.name << ")";
	return os;
}

std::ostream& operator<<(std::ostream& os, const std::optional<VersionedSymbol> & s) {
	if (s)
		os << s.value();
	else
		os << "[no symbol]";
	return os;
}
