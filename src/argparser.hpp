#pragma once

#include <functional>
#include <variant>
#include <vector>

#include "utils.hpp"
#include "generic.hpp"

template <class Opts>
struct ArgParser : Opts {
	typedef std::variant<const char * Opts::*, int Opts::*, unsigned Opts::*, long long Opts::*, unsigned long long Opts::*, bool Opts::*, std::vector<const char *> Opts::*, std::vector<int> Opts::*, std::vector<unsigned> Opts::*, std::vector<long long> Opts::*, std::vector<unsigned long long> Opts::*> element_t;

	using Parameter = struct {
		const char name_short;
		const char * name_long;
		const char * name_arg;
		element_t element;
		bool required;
		const char * help_text;
		std::function<bool(const char *)> validate;
		bool present;

		bool matches(const char * name) {
			return name[0] == '-'
			   && (   (name_short != '\0' && name[1] == name_short)
			       || (name_long != nullptr && name[1] == '-' && strcmp(name + 2, name_long) == 0));
		}

		template<typename T>
		void help(BufferStream & out, T* def = nullptr, size_t max_len = 70) {
			size_t pos = 0;
			auto text = [this, &out, max_len, &pos, &def](size_t len) -> bool {
				size_t l = 0;
				bool e = false;
				while (help_text[pos] == ' ')
					pos++;
				for (size_t i = 0; i < max_len; i++)
					if (help_text[pos + i] == '\0') {
						e = def == nullptr;
						l = i;
						break;
					} else if (help_text[pos + i] == ' ') {
						l = i;
					}
				size_t spacer = 25;
				if (l > 0) {
					if (len < spacer)
						out.write(' ', spacer - len);
					out.write(help_text + pos, l);
					pos += l;
				} else if (def != nullptr) {
					if (len < spacer)
						out.write(' ', spacer - len);
					out << "(default: " << *def << ")" << endl;
					def = nullptr;
				}
				if (l > 0 || len > 0)
					out << endl;
				return e;
			};

			if (name_short != '\0') {
				out << "    -" << name_short;
				if (name_arg == nullptr) {
					text(7);
				} else {
					out << ' ' << name_arg;
					text(8 + strlen(name_arg));
				}
			}
			if (name_long != nullptr) {
				out << "    --" << name_long;
				if (name_arg == nullptr)
					text(7 + strlen(name_long));
				else {
					out << ' ' << name_arg;
					text(8 + strlen(name_long) + strlen(name_arg));
				}
			}
			while (!text(0)) {}

			out << endl;
		}
	};

 private:
	std::vector<Parameter> args;
	std::vector<const char *> positional, terminal;
	std::function<bool(const char *)> validate_positional, validate_terminal;

	ArgParser() = default;
	ArgParser(const ArgParser&) = delete;
	ArgParser(ArgParser&&) = delete;
	ArgParser& operator=(const ArgParser&) = delete;
	ArgParser& operator=(ArgParser&&) = delete;

	bool set(const Parameter & arg, const char * value) {
		return visit([this, arg, value](auto&& element) -> bool {
			return arg.validate && !arg.validate(value) ? false : Utils::parse(this->*element, value);
		}, arg.element);
	}

 public:
	ArgParser(const std::initializer_list<Parameter> & list, std::function<bool(const char *)> validate_positional, std::function<bool(const char *)> validate_terminal) : validate_positional(validate_positional), validate_terminal(validate_terminal) {
		for (auto arg : list) {
			// Check if parameter is unqiue
			bool skip = false;
			if (arg.name_short == '\0' && arg.name_long == nullptr) {
				LOG_FATAL << "Parameter has neither short nor long name!" << endl;
				skip = true;
			} else {
				for (const auto & other : args)
					if (arg.name_short != '\0' && arg.name_short == other.name_short) {
						LOG_FATAL << "Parameter '-" << arg.name_short << "' is not unqiue!" << endl;
						skip = true;
						break;
					} else if (arg.name_long != nullptr && other.name_long != nullptr && strcmp(arg.name_long, other.name_long) == 0) {
						LOG_FATAL << "Parameter '--" << arg.name_long << "' is not unqiue!" << endl;
						skip = true;
						break;
					} else if (arg.element == other.element) {
						if (arg.name_long != nullptr) {
							LOG_FATAL << "Element of parameter '--" << arg.name_long << "' is not unqiue!" << endl;
						} else {
							LOG_FATAL << "Element of parameter '-" << arg.name_short << "' is not unqiue!" << endl;
						}
						skip = true;
						break;
					}
			}

			// valid parameter.
			if (!skip)
				args.push_back(arg);
		}
	}

	ArgParser(const std::initializer_list<Parameter> & list) : ArgParser(list, [](const char * str) -> bool { if (str != nullptr && *str == '-') { LOG_ERROR << "Positional parameter '" << str << "' looks like its not meant to be positional!" << endl; return false; } else { return true; } }, [](const char *) -> bool { return true; }) {}

	~ArgParser() = default;

	void help(BufferStream & out, const char * header, const char * file, const char * footer, const char * positional = nullptr, const char * terminal = nullptr) {
		// Usage line
		if (header != nullptr)
			out << endl << header << endl;
		out << endl << " Usage: " << endl << "    " << file;

		bool hasRequired = false;
		bool hasOptional = false;
		size_t flen = 4 + strlen(file);
		int i = 0;
		for (auto arg : args) {
			if (arg.required) {
				if (arg.name_long != nullptr)
					out << " --" << arg.name_long;
				else
					out << " -" << arg.name_short;
				if (arg.name_arg != nullptr)
					out << " " << arg.name_arg;
				hasRequired = true;
			} else {
				if (arg.name_long != nullptr)
					out << " [--" << arg.name_long;
				else
					out << " [-" << arg.name_short;
				if (arg.name_arg != nullptr)
					out << " " << arg.name_arg;
				out << "]";
				hasOptional = true;
			}
			if (++i == 4) {
				out << endl;
				out.write(' ', flen);
			}
		}
		if (positional != nullptr)
			out << " " << positional;
		if (terminal != nullptr)
			out << " -- " << terminal;
		out << endl;

		// Parameter details
		if (hasRequired) {
			out << endl << " Required parameters:" << endl;
			for (auto arg : args)
				if (arg.required)
					arg.help(out, (void**)nullptr);
		}

		if (hasOptional) {
			out << endl << " Optional parameters:" << endl;
			for (auto arg : args)
				if (!arg.required)
					visit([&arg, &out, this](auto&& element) { arg.help(out, Utils::is_default(this->*element) ? nullptr : &(this->*element)); }, arg.element);
		}

		if (footer != nullptr)
			out << endl << footer << endl;

		out << endl;
	}

	bool parse(int argc, char* argv[]) {
		bool terminator = false;
		bool error = false;

		for (int idx = 1; idx < argc; ++idx) {
			char * current = argv[idx];

			if (terminator) {
				// Terminal argument
				if (validate_terminal(current)) {
					terminal.push_back(current);
				} else {
					LOG_ERROR << "Terminal argument '" << current << "' is not valid!" << endl;
					error = true;
				}
			} else if (current[0] == '-' && current[1] == '-' && current[2] == '\0') {
				terminator = true;
				continue;
			} else {
				bool found = false;
				// Check all parameter names
				for (auto & arg : args) {
					if (arg.matches(current)) {
						found = true;
						bool valid;
						if (std::holds_alternative<bool Opts::*>(arg.element)) {
							valid = visit([this](auto&& element) -> bool {
								return Utils::parse(this->*element, "1");
							}, arg.element);
						} else {
							const char * next = idx < argc - 1 ? argv[idx + 1] : nullptr;
							valid = visit([this, arg, &idx, current, next](auto&& element) -> bool {
								if (next != nullptr) {
									idx += 1;
									if (set(arg, next)) {
										return true;
									} else {
										LOG_ERROR << "Invalid value '" << next << "' for parameter " << current << "!" << endl;
										return false;
									}
								} else {
									LOG_ERROR << "Missing value for parameter " << current << "!" << endl;
									return false;
								}
							}, arg.element);
						}
						if (!valid) {
							error = true;
						} else if (arg.present && !visit([this](auto&& element) -> bool { return Utils::is_vector(this->*element); }, arg.element)) {
							LOG_ERROR << "Parameter " << current << " occured multiple times (= reassigned)!" << endl;
							error = true;
						} else {
							arg.present = true;
						}
						break;
					}
				}

				// Positional argument
				if (!found) {
					if (validate_positional(current)) {
						positional.push_back(current);
					} else {
						LOG_ERROR << "Positional argument '" << current << "' is not valid!" << endl;
						error = true;
					}
				}
			}
		}

		for (auto arg : args)
			if (arg.required && !arg.present) {
				if (arg.name_long != nullptr) {
					LOG_ERROR << "Parameter '--" << arg.name_long << "' is required!" << endl;
				} else {
					LOG_ERROR << "Parameter '-" << arg.name_short << "' is required!" << endl;
				}
				error = true;
			}

		return !error;
	}

	bool has(const char * name) {
		for (auto arg : args)
			if (arg.matches(name))
				return arg.present;
		return false;
	}

	bool has(element_t element) {
		for (auto arg : args)
			if (element == arg.element)
				return arg.present;
		return false;
	}

	bool has_positional() {
		return positional.size() > 0;
	}

	std::vector<const char *> get_positional() {
		return positional;
	}

	bool has_terminal() {
		return terminal.size() > 0;
	}

	std::vector<const char *> get_terminal() {
		return terminal;
	}

	bool set(const char * name, const char * value) {
		for (auto & arg : args)
			if (arg.matches(name))
				return set(arg, value);
	}

	bool set(element_t element, const char * value) {
		for (auto & arg : args)
			if (element == arg.element)
				return set(arg, value);
		return false;
	}

	Opts get_all() {
		return static_cast<Opts>(*this);
	}
};
