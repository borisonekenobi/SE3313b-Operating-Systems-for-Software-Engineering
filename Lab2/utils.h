//
// Created by boris on 2025-02-22.
//

#include <algorithm>
#include <cctype>
#include <locale>

using namespace std;

#ifndef UTILS_H
#define UTILS_H

inline void ltrim(string& s)
{
    s.erase(s.begin(), find_if(s.begin(), s.end(), [](unsigned char ch)
    {
        return !isspace(ch);
    }));
}

inline void rtrim(string& s)
{
    s.erase(find_if(s.rbegin(), s.rend(), [](unsigned char ch)
    {
        return !isspace(ch);
    }).base(), s.end());
}

inline void trim(std::string& s)
{
    rtrim(s);
    ltrim(s);
}

inline std::string ltrim_copy(std::string s)
{
    ltrim(s);
    return s;
}

inline std::string rtrim_copy(std::string s)
{
    rtrim(s);
    return s;
}

inline std::string trim_copy(std::string s)
{
    trim(s);
    return s;
}

#endif //UTILS_H
