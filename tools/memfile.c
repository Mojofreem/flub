#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <unistd.h>


#define MAX_STRUCT_NAME_LEN 1024
#define MAX_FILE_NAME_LEN 1024
#define MEMFILE_BYTES_PER_ROW 16

#ifdef WIN32
#	define PLATFORM_PATH_SEP	'\\'
#else // WIN32
#	define PLATFORM_PATH_SEP	'/'
#endif // WIN32


const char *g_progName = NULL;


typedef struct fileInfo_s {
	const char *path;
	const char *asPath;
	int index;
	struct fileInfo_s *next;
} fileInfo_t;


int exists(const char *path) {
	if(access(path, F_OK) != -1) {
		return 1;
	}
	return 0;
}

int fileAdd(fileInfo_t **info, const char *workingPath, const char *name, const char *as) {
	fileInfo_t *walk, *entry;
	int idx;

    if((as == NULL) && (workingPath != NULL)) {
        if(!strncmp(name, workingPath, strlen(workingPath))) {
            if ((as = strdup(name + strlen(workingPath))) == NULL) {
                printf("ERROR: Out of memory.\n");
                return 0;
            }
            printf("INFO: Trimming base path to \"%s\"\n", as);
        } else {
            printf("INFO: Base path [%s] not in [%s]\n", workingPath, name);
        }
    }

	if(!exists(name)) {
		printf("ERROR: File \"%s\" could not be found.\n", name);
		return 0;
	}

	if((entry = calloc(1, sizeof(fileInfo_t))) == NULL) {
		printf("ERROR: Out of memory.\n");
		return 0;
	}

	if((entry->path = strdup(name)) == NULL) {
		printf("ERROR: Out of memory.\n");
		return 0;
	}

    if(as != NULL) {
		if((entry->asPath = strdup(as)) == NULL) {
			printf("ERROR: Out of memory.\n");
			return 0;			
		}
	}

	printf("INFO: Adding file \"%s\"%s%s\n", name, ((as != NULL) ? " as " : ""), ((as != NULL) ? as : ""));

	if(*info == NULL) {
		*info = entry;
		entry->index = 1;
	} else {
		for(walk = *info; walk->next != NULL; walk = walk->next);
		walk->next = entry;
		entry->index = walk->index + 1;
	}
	return 1;
}

int numLenAsStr(int num) {
	int k = 1;

	while(num >= 10) {
		num /= 10;
		k++;
	}
	return k;
}

const char *structNameGen(const char *symbol, int caps, int index) {
	static char buf[MAX_STRUCT_NAME_LEN];
	int k;
	const char *ptr;

	memset(buf, 0, sizeof(buf));
	for(k = 0, ptr = symbol; *ptr != '\0'; ptr++) {
		if(!isalnum(*ptr)) {
			if((k == 0) || (buf[k - 1] != '_')) {
				buf[k] = '_';
				k++;
			}
		} else {
			if(caps) {
				buf[k] = toupper(*ptr);
			} else {
				buf[k] = *ptr;
			}
			k++;
		}
		if(k >= (sizeof(buf) - 1)) {
			break;
		}
	}
	buf[k] = '\0';
	if(index > 0) {
		if((numLenAsStr(index) + strlen(buf) + 1) > sizeof(buf)) {
			printf("ERROR: Out of room for generated symbol name. WTF are you doing?\n");
			return buf;
		}
		sprintf(buf + k, "_%d", index);		
	}
	return buf;
}

const char *normalizePath(const char *path) {
	static char buf[MAX_STRUCT_NAME_LEN];
	int k;
	const char *ptr;

	for(k = 0, ptr = path; ((k < (sizeof(buf) - 1)) && (*ptr != '\0')); ptr++, k++) {
		if(*ptr == '\\') {
			buf[k] = '/';
		} else {
			buf[k] = *ptr;
		}
	}
	buf[k] = '\0';

	return buf;
}

FILE *createFile(const char *base, const char *extension) {
	FILE *fp;
	char buf[MAX_FILE_NAME_LEN];

	snprintf(buf, sizeof(buf) - 1, "%s.%s", base, extension);
	if((fp = fopen(buf, "wt")) == NULL) {
		printf("ERROR: Failed to open the file \"%s\" for writing.\n", buf);
		return NULL;
	}
	return fp;
}

int generateHeader(const char *outfile, const char *symbol, fileInfo_t *info) {
	FILE *fp;

	printf("INFO: Generating header file \"%s.h\"\n", outfile);

	if((fp = createFile(outfile, "h")) == NULL) {
		return 0;
	}

	fprintf(fp, "// This file was generated by the memfile tool. Any changes made to\n");
	fprintf(fp, "// it will be lost is this file is regenerated. Edit at your own risk.\n");
	fprintf(fp,"#ifndef _%s_HEADER_\n", structNameGen(symbol, 1, -1));
	fprintf(fp,"#define _%s_HEADER_\n\n\n", structNameGen(symbol, 1, -1));

	fprintf(fp, "#include <PHYSFS_memfile.h>\n\n");

	fprintf(fp, "extern const PHYSFS_memfile *%s;\n\n", symbol);
    fprintf(fp, "extern PHYSFS_memfile _%s_MEMFILE;\n\n", structNameGen(symbol, 0, 1));
    fprintf(fp, "#define %s_val _%s_MEMFILE\n\n", symbol, structNameGen(symbol, 0, 1));

	fprintf(fp,"\n#endif // _%s_HEADER_\n", structNameGen(symbol, 1, -1));

	fclose(fp);

	return 1;
}

int generateMemfile(FILE *fp, const char *symbol, fileInfo_t *info) {
	FILE *in;
	int c;
	int k = 0;

	printf("INFO: Generating memfile for \"%s\"\n", info->path);

	if((in = fopen(info->path, "rb")) == NULL) {
		printf("ERROR: Failed to open file \"%s\" for input.\n", info->path);
		return 0;
	}
	fprintf(fp, "const char _%s_DATA[] = {", structNameGen(symbol, 0, info->index));
	while((c = fgetc(in)) != EOF) {
		if(!(k % MEMFILE_BYTES_PER_ROW)) {
			fprintf(fp, "%s\n    ", ((k > 0) ? "," : ""));
		}
		fprintf(fp, "%s0x%2.2X", (((k > 0) && (k % MEMFILE_BYTES_PER_ROW)) ? ", " : ""), c);
		k++;
	}
	fprintf(fp, "};\n\n");
	fclose(in);

	fprintf(fp, "PHYSFS_memfile _%s_MEMFILE = {\n", structNameGen(symbol, 0, info->index));
	fprintf(fp, "    .path = \"%s\",\n", normalizePath(((info->asPath != NULL) ? info->asPath : info->path)));
	fprintf(fp, "    .data = _%s_DATA,\n", structNameGen(symbol, 0, info->index));
	fprintf(fp, "    .length = %d,\n", k);
	if(info->next == NULL) {
		fprintf(fp, "    .next = NULL,\n");
	} else {
		fprintf(fp, "    .next = &_%s_MEMFILE,\n", structNameGen(symbol, 0, info->index + 1));
	}
	fprintf(fp, "};\n\n");

	return 1;
}

int reverseStackWalkFileList(FILE *fp, const char *symbol, fileInfo_t *info) {
	if(info->next != NULL) {
		if(!reverseStackWalkFileList(fp, symbol, info->next)) {
			return 0;
		}
	}
	return generateMemfile(fp, symbol, info);
}

int generateSource(const char *outfile, const char *workingPath, const char *symbol, fileInfo_t *info) {
	FILE *fp;
    int offset = 0;

	printf("INFO: Generating source file \"%s.c\"\n", outfile);

	if((fp = createFile(outfile, "c")) == NULL) {
		return 0;
	}

	fprintf(fp, "// Flub memfile generated by the memfile tool\n");
	fprintf(fp, "// Any edits to this file will be overwritten when this file is regenerated.\n\n");
	
	fprintf(fp, "#include <PHYSFS_memfile.h>\n");
    if(workingPath != NULL) {
        if(!strncmp(outfile, workingPath, strlen(workingPath))) {
            offset = strlen(workingPath);
        }
    }
	fprintf(fp, "#include \"%s.h\"\n\n", outfile + offset);

	if(!reverseStackWalkFileList(fp, symbol, info)) {
		return 0;
	}

	fprintf(fp, "const PHYSFS_memfile *%s = &_%s_MEMFILE;\n\n", symbol, structNameGen(symbol, 0, 1));

	fclose(fp);

	return 1;
}

const char *stripProgName(const char *name) {
	const char *ptr;

	if((ptr = strrchr(name, PLATFORM_PATH_SEP)) != NULL) {
		return ptr + 1;
	}
	return name;
}

void usage(void) {
	printf("Usage: %s <OUT> <SYMBOL_NAME> [<FILE> [-p <AS_PATH>] ...]\n", g_progName);
	printf("where:\n\n");
    printf("    -b <BASE_PATH>  (optional) common base path to be stripped from file paths.\n");
	printf("    <OUT>           is the base name of the source file to generate.");
	printf("    <SYMBOL_NAME>   is the name of the generated memfile structure.\n");
	printf("    <FILE>          is the name of the file to generate the memfile from.\n");
	printf("    -p <AS_PATH>    (optional) specifies an alternate path name for the file.\n\n");
}

int main(int argc, char *argv[]) {
	fileInfo_t *info = NULL;
	int k;
	const char *outfile;
	const char *symbol;
	const char *path;
	const char *as;
	int count = 0;
    char *basePath = NULL;

	g_progName = stripProgName(argv[0]);

	if(argc < 4) {
		usage();
		return 1;
	}

	outfile = argv[1];
	symbol = argv[2];

	for(k = 3; k < argc; k++) {
		if(argv[k][0] == '-') {
            if(argv[k][1] == 'b') {
                if((k + 1) >= argc) {
                    printf("ERROR: Missing path from base path option.\n");
                    return 1;
                }
                printf("INFO: Base path [%s] (param %d of %d)\n", argv[k + 1], k + 1, argc);
                basePath = strdup(normalizePath(argv[k + 1]));
                k++;
                continue;
            } else {
                printf("ERROR: Unexpected option \"%s\".\n", argv[k]);
                usage();
                return 1;
            }
		}
		path = argv[k];
		as = NULL;
		if((k + 1) < argc) {
			if(!strcmp(argv[k + 1], "-p")) {
				if((k + 2) < argc) {
					as = argv[k + 2];
				} else {
					printf("ERROR: Missing 'as path' for file \"%s\" with option '-p'\n", path);
					return 1;
				}
				k += 2;
			}
		}
		if(!fileAdd(&info, basePath, path, as)) {
			return 1;
		}
		count++;
	}

	if(!generateHeader(outfile, symbol, info)) {
		return 1;
	}

	if(!generateSource(outfile, basePath, symbol, info)) {
		return 1;
	}

	printf("INFO: Created %d entries in file \"%s.c\"\n", count, outfile);
	printf("INFO: Done!\n");

	return 0;
}

