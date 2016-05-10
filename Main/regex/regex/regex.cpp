// regex.cpp : Defines the entry point for the application.
//

#include "regex.h"
#include <regex>
#include <string>

int win32_regex_match(const char* pattern, const char* value)
{
    std::tr1::regex regpattern(pattern,std::tr1::regex_constants::extended | std::tr1::regex_constants::icase);
    std::tr1::match_results<std::string::const_iterator> result;
    std::string stdvalue(value);
    return std::tr1::regex_match(stdvalue, result,regpattern);
}