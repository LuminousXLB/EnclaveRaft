//
// Created by ncl on 12/11/19.
//

#ifndef ENCLAVERAFT_UNQUOTE_HXX
#define ENCLAVERAFT_UNQUOTE_HXX

#include <string>

using std::string;

static string url_decode(const string &str) {
    string decoded;
    size_t len = str.length();

    for (size_t i = 0; i < len; ++i) {
        switch (str[i]) {
            case '+':
                decoded += ' ';
                break;
            case '%': {
                char *e = nullptr;

                // Have a % but run out of characters in the string

                if (i + 3 > len) {
                    throw std::length_error("premature end of string");
                }

                unsigned long int v = strtoul(str.substr(i + 1, 2).c_str(), &e, 16);

                // Have %hh but hh is not a valid hex code.
                if (*e) {
                    throw std::out_of_range("invalid encoding");
                }

                i += 2;
                decoded += static_cast<char>(v);
                break;
            }
            default:
                decoded += str[i];
        }
    }

    return decoded;
}

#endif //ENCLAVERAFT_UNQUOTE_HXX
