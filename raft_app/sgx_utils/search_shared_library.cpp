#include <vector>
#include <string>
#include <cstring>
#include <sys/stat.h>

using namespace std;


bool check_file_path(const string &file_path) {
    struct stat file_stat{};
    return stat(file_path.c_str(), &file_stat) == 0;
}


string file_in_search_path(const string &filename, const string &path) {
    if (path.empty()) {
        return "";
    }

    char *str = strdup(path.c_str());
    for (char *p = strtok(str, ":"); p != nullptr; p = strtok(nullptr, ":")) {
        string full_path = p;
        full_path.append("/").append(filename);

        if (check_file_path(full_path)) {
            free(str);
            return full_path;
        }
    }

    free(str);
    return "";
}


string search_shared_library(const string &filename, const string &path) {
    string result;

    if (!path.empty()) {
        result = file_in_search_path(filename, path);
        if (!result.empty()) {
            return result;
        }
    }

    if (check_file_path(filename)) {
        return filename;
    }

#ifdef __x86_64__
#define DEF_LIB_SEARCHPATH "/lib:/lib64:/usr/lib:/usr/lib64"
#else
#define DEF_LIB_SEARCHPATH "/lib:/usr/lib"
#endif

    vector<string> paths = {
            getenv("LD_LIBRARY_PATH"),
            getenv("DT_RUNPATH"),
            DEF_LIB_SEARCHPATH
    };

    for (const auto &p : paths) {
        result = file_in_search_path(filename, getenv("LD_LIBRARY_PATH"));
        if (!result.empty()) {
            return result;
        }
    }

    return filename;
}
