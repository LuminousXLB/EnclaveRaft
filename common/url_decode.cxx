//
// Created by ncl on 14/11/19.
//

#include "url_decode.hxx"


string url_decode(const char *start, const char *end) {
    string output;
    output.reserve((end - start + 1) * sizeof(char));

    const char *ptr = start;
    while (*ptr && ptr < end) {
        if (*ptr == '%') {
            char buffer[3] = {ptr[1], ptr[2], 0};
            output.push_back((char) strtol(buffer, nullptr, 16));
            ptr += 3;
        } else {
            output.push_back(*ptr);
            ptr++;
        }
    }

    return output;
}
