#include <unistd.h>
#include <stdio.h>
#include <flub/util/file.h>


int utilFexists(const char *fname) {
    if(access(fname, F_OK) != -1) {
        return 1;
    }
    return 0;
}

char *utilGenFilename(const char *prefix, const char *suffix,
                      int max_attempts, char *buffer, size_t len) {
    int k;

    for(k = 1; k <= max_attempts; k++) {
        snprintf(buffer, len - 1, "%s_%d.%s", prefix, k, suffix);
        buffer[len - 1] = '\0';
        if(!utilFexists(buffer)) {
            return buffer;
        }
    }
    return NULL;
}

