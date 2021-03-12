#include "argparser.hpp"

template <class T>
const std::string ArgParser<T>::REQUIRED ({ '\0' });

template <class T>
const std::string ArgParser<T>::TERMINATOR ("--");

template <class T>
const std::string ArgParser<T>::SPACE ("                  ");

template <class T>
const std::string ArgParser<T>::TAB ("    ");
