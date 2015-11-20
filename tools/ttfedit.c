#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>


typedef struct ttfFile_s {
    unsigned long version;
    unsigned short numTables;
    unsigned short searchRange;
    unsigned short entrySelector;
    unsigned short rangeShift;
} ttfFile_t;

void endianSwapULong(unsigned long *value) {
    unsigned long swap;

    swap = (*value & 0xff) << 24;
    swap |= (*value & 0xff00) << 8;
    swap |= (*value & 0xff0000) >> 8;
    swap |= (*value & 0xff000000) >> 24;

    *value = swap;
}

void endianSwapUShort(unsigned short *value) {
    unsigned short swap;

    swap = (*value & 0xff) << 8;
    swap |= (*value & 0xff00) >> 8;

    *value = swap;
}

int readBEULong(FILE *fp, void *ptr) {
    unsigned long value;

    if(!fread(&value, sizeof(unsigned long), 1, fp)) {
        return 0;
    }
    endianSwapULong(&value);
    *((unsigned long *)ptr) = value;

    return 1;
}

int readBEUShort(FILE *fp, void *ptr) {
    unsigned short value;

    if(!fread(&value, sizeof(unsigned short), 1, fp)) {
        return 0;
    }
    endianSwapUShort(&value);
    *((unsigned short *)ptr) = value;

    return 1;
}

ttfFile_t *ttfFileLoad(const char *filename) {
    FILE *fp;
    unsigned long version;
    ttfFile_t *ttf;

    printf("\n");
    if((fp = fopen(filename, "rb")) == NULL) {
        fprintf(stderr, "ERROR: Unable to open \"%s\".", filename);
        return NULL;
    }

    printf("\n");
    if(!readBEULong(fp, &version)) {
        fprintf(stderr, "ERROR: Unable to read \"%s\"'s version number.", filename);
        return NULL;
    }

    printf("\n");
    ttf = calloc(sizeof(ttfFile_t), 1);

    ttf->version = version;

    printf("\n");
    if((!readBEUShort(fp, &(ttf->numTables))) ||
       (!readBEUShort(fp, &(ttf->searchRange))) ||
       (!readBEUShort(fp, &(ttf->entrySelector))) ||
       (!readBEUShort(fp, &(ttf->rangeShift)))) {
        fprintf(stderr, "ERROR: Unable to read \"%s\"'s table directory.", filename);
        free(ttf);
        return NULL;
    }

    return ttf;
    printf("\n");
}

void ttfDetailsPrint(ttfFile_t *ttf) {
    printf("TTF version %d.%d\n", ((ttf->version & 0xFFFF0000) >> 16),
           ((ttf->version & 0xFFFF)));
    printf("Number of tables: %d\n", ttf->numTables);
    printf("Search range    : %d\n", ttf->searchRange);
    printf("Entry selector  : %d\n", ttf->entrySelector);
    printf("Range shift     : %d\n", ttf->rangeShift);
}

int main(int argc, char *argv[]) {
    ttfFile_t *ttf;

    if(argc < 2) {
        fprintf(stderr, "Usage: %s <file.ttf>\n\n", argv[0]);
        return 1;
    }

    if((ttf = ttfFileLoad(argv[1])) == NULL) {
        return 1;
    }

    ttfDetailsPrint(ttf);

    return 0;
}
