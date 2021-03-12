#pragma once

#include <algorithm>
#include <functional>
#include <typeinfo>
#include <cctype>
#include <set>
#include <string>
#include <sstream>
#include <variant>
#include <vector>

#include "generic.hpp"

template <class Opts>
struct ArgParser : Opts {
	static const std::string REQUIRED;
	static const std::string TERMINATOR;
	static const std::string SPACE;
	static const std::string TAB;

	using Parameter = struct {
		std::string name;
		std::variant<std::string Opts::*, int Opts::*, bool Opts::*> element;
		std::string help;
		std::string def;
		std::function<bool(const std::string &)> validate;
		bool present;

		std::string get_name() const {
			return name.substr(0, name.find(" "));
		}
	};

 private:
	std::vector<Parameter> args;
	std::vector<std::string> positional, terminal;
	std::function<bool(const std::string &)> validate_positional, validate_terminal;

	ArgParser() = default;
	ArgParser(const ArgParser&) = delete;
	ArgParser(ArgParser&&) = delete;
	ArgParser& operator=(const ArgParser&) = delete;
	ArgParser& operator=(ArgParser&&) = delete;

	bool set(const Parameter & arg, const std::string value) {
		return visit([this, arg, value](auto&& element) -> bool {
			if (typeid(element) == typeid(bool Opts::*)) {
				this->*element = false;
				return value.empty() || value == "0";
			} else if (arg.validate && !arg.validate(value)) {
				return false;
			} else {
				std::stringstream s;
				s << value;
				s >> this->*element;
				return !s.eof();
			}
		}, arg.element);
	}

	std::string get(const Parameter & arg) {
		return visit([this, arg](auto&& element) -> std::string {
			std::stringstream s;
			s << this->*element;
			return s.str();
		}, arg.element);
	}

	std::vector<std::string> help_parameter(const Parameter & arg, std::size_t len = 70){
		size_t l = arg.name.size();
		std::vector<std::string> result({ l < SPACE.size() ? arg.name + SPACE.substr(l) : arg.name });

		std::stringstream s;
		s << arg.help;

		std::string word;
		while(s >> word) {
			if ((result.back().size() + word.size()) <= len) {
				result.back() += word + ' ';
			} else {
				result.back().pop_back();
				result.push_back(SPACE + word + ' ');
			}
		}
		result.back().pop_back();
		return result;
	}

 public:
	ArgParser(const std::initializer_list<Parameter> & list, std::function<bool(const std::string &)> validate_positional, std::function<bool(const std::string &)> validate_terminal) : validate_positional(validate_positional), validate_terminal(validate_terminal) {
		std::set<std::string> opts;
		for (auto arg : list) {
			std::string name = arg.get_name();

			// Check if parameter is unqiue
			if (!opts.insert(name).second) {
				LOG_FATAL << "Parameter '" << name << "' is not unqiue!";
				continue;
			}

			// check default value
			if (arg.def.empty())
				arg.def = get(arg);
			if (!set(arg, arg.def)) {
				LOG_FATAL << "Invalid default value '" << arg.def << "' for parameter " << name << "!";
				continue;
			}

			// valid parameter.
			arg.present = false;
			args.push_back(arg);
		}
	}

	ArgParser(const std::initializer_list<Parameter> & list) : ArgParser(list, [](const std::string & str) -> bool { if (str.rfind("-", 0) == 0) { LOG_ERROR << "Positional parameter '" << str << "' looks like its not meant to be positional!" ; return false; } else { return true; } }, [](const std::string &) -> bool { return true; }) {}

	~ArgParser() = default;

	std::string help(const std::string &header, const std::string &file, const std::string &footer, const std::string & positional = {}, const std::string & terminal = {}) {
		bool hasRequired = false;
		bool hasOptional = false;
		std::stringstream out;

		// Usage line
		if (!header.empty())
			out << std::endl << header << std::endl;
		out << std::endl << " Usage: " << std::endl << TAB << file;

		for (auto arg : args) {
			if (arg.def == REQUIRED) {
				out << " " << arg.name;
				hasRequired = true;
			} else {
				out << " [" << arg.name << "]";
				hasOptional = true;
			}
		}
		if (!positional.empty())
			out << " " << positional;
		if (!terminal.empty())
			out << " " << TERMINATOR << " " << terminal;
		out << std::endl;

		// Parameter details
		if (hasRequired) {
			out << std::endl << " Required parameters:" << std::endl;
			for (auto arg : args)
				if (arg.def == REQUIRED)
					for (auto & line: help_parameter(arg))
						out << TAB << line << std::endl;
		}

		if (hasOptional) {
			out << std::endl << " Optional parameters:" << std::endl;
			for (auto arg : args)
				if (arg.def != REQUIRED) {
					for (auto & line: help_parameter(arg))
						out << TAB << line << std::endl;
					if (!arg.def.empty())
						out << TAB << SPACE << "(Default: " << arg.def << ")" << std::endl;
				}
		}

		if (!footer.empty())
			out << std::endl << footer << std::endl;

		out << std::endl;

		return out.str();
	}

	bool parse(int argc, const char* argv[]) {
		bool terminator = false;
		bool error = false;

		for (int idx = 1; idx < argc; ++idx) {
			std::string current = argv[idx];

			if (terminator) {
				// Terminal argument
				if (validate_terminal(current)) {
					terminal.push_back(current);
				} else {
					LOG_ERROR << "Terminal argument '" << current << "' is not valid!";
					error = true;
				}
			} else if (current == TERMINATOR) {
				terminator = true;
				continue;
			} else {
				bool found = false;
				// Check all parameter names
				for (auto & arg : args) {
					if (current == arg.get_name()) {
						found = true;

						const bool hasNext = idx < argc - 1;
						const std::string next = hasNext ? argv[idx + 1] : "";

						bool valid = visit([this, arg, &idx, current, hasNext, next](auto&& element) -> int {
							if (typeid(element) == typeid(bool Opts::*)) {
								this->*element = true;
								return true;
							} else if (hasNext) {
								idx += 1;
								if (set(arg, next)) {
									return true;
								} else {
									LOG_ERROR << "Invalid value '" << next << "' for parameter " << arg.get_name() << "!";
									return false;
								}
							} else {
								LOG_ERROR << "Missing value for parameter " << current << "!";
								return false;
							}
						}, arg.element);

						if (!valid) {
							error = true;
						} else if (arg.present) {
							LOG_ERROR << "Parameter " << current << " used multiple times (= reassigned)!";
							error = true;
						} else {
							arg.present = true;
						}
					}
				}

				// Positional argument
				if (!found) {
					if (validate_positional(current)) {
						positional.push_back(current);
					} else {
						LOG_ERROR << "Positional argument '" << current << "' is not valid!";
						error = true;
					}
				}
			}
		}
		for (auto arg : args)
			if (arg.def == REQUIRED && !arg.present) {
				LOG_ERROR << "Parameter " << arg.get_name() << " is required!";
				error = true;
			}

		return !error;
	}

	bool has(const std::string &parameter) {
		for (auto arg : args)
			if (parameter == arg.get_name())
				return arg.present;
		return false;
	}

	bool has_positional() {
		return positional.size() > 0;
	}

	std::vector<std::string> get_positional() {
		return positional;
	}

	bool has_terminal() {
		return terminal.size() > 0;
	}

	std::vector<std::string> get_terminal() {
		return terminal;
	}

	bool set(const std::string name, const std::string value) {
		for (auto & arg : args)
			if (name == arg.get_name())
				return set(arg, value);
		return false;
	}

	std::string get(const std::string & name) {
		for (auto & arg : args)
			if (name == arg.get_name())
				return get(arg);
		return "";
	}

	Opts get_all() {
		return static_cast<Opts>(*this);
	}
};


template <class Opts>
const std::string ArgParser<Opts>::REQUIRED({ '\0' });

template <class Opts>
const std::string ArgParser<Opts>::TERMINATOR("--");

template <class Opts>
const std::string ArgParser<Opts>::SPACE("                  ");

template <class Opts>
const std::string ArgParser<Opts>::TAB("    ");
