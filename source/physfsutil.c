#include <flub/physfsutil.h>
#include <flub/app.h>
#include <flub/logger.h>
#include <stdlib.h>
#include <physfs.h>
#include <unistd.h>
#include <physfs_memfile.h>
#include <resources/flub_resources.h>


/////////////////////////////////////////////////////////////////////////////
// physfs module
/////////////////////////////////////////////////////////////////////////////

int flubPhysfsInit(appDefaults_t *defaults);
void flubPhysfsShutdown(void);

flubModuleCfg_t flubModulePhysfs = {
    .name = "physfs",
    .init = flubPhysfsInit,
    .shutdown = flubPhysfsShutdown,
};

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

struct {
    int init;
} _physfsCtx = {
        .init = 0,
    };

static flubResourceList_t _flubMemfileResources[] = {
        {"default resources", &flubDefaultResources_val},
        {NULL, NULL},
    };

static void _physfsLogCallback(const char *msg) {
    debugf(DBG_FILE, DBG_FILE_DTL_PHYSFS, "PHYSFS: %s", msg);
}

int flubPhysfsInit(appDefaults_t *defaults) {
    char working_dir[512];
    int k;
    const char *appPath = defaults->argv[0];

    if(_physfsCtx.init) {
        warning("Ignoring attempt to re-initialize physfs.");
        return 1;
    }

    if(!logValid()) {
        // The logger has not been initiated!
        return 0;
    }

    logDebugRegister("file", DBG_FILE, "general", DBG_FILE_DTL_GENERAL);
    logDebugRegister("file", DBG_FILE, "physfs", DBG_FILE_DTL_PHYSFS);

    //PHYSFS_log_callback = _physfsLogCallback;

    debug(DBG_FILE, DBG_FILE_DTL_GENERAL, "Initializing PHYSFS");

    if(!PHYSFS_init(appPath)) {
        fatal("Failed to initialize the virtual file system.");
        return 0;
    } else {
        if(!PHYSFS_mount(getcwd(working_dir, sizeof(working_dir)), NULL, 1)) {
            fatalf("Failed to mount the current working directory (%s).", working_dir);
            return 0;
        } else {
            infof("Mounted current working directory: %s", working_dir);
        }
        if(!PHYSFS_mount(PHYSFS_getBaseDir(), NULL, 1)) {
            fatalf("Failed to mount the application's base dir (%s).",
                   PHYSFS_getBaseDir());
            return 0;
        } else {
            infof("Mounted base directory: %s", PHYSFS_getBaseDir());
        }
        if(appDefaults.archiveFile != NULL) {
            infof("Mounting app archive file \"%s\".", appDefaults.archiveFile);
            if(!PHYSFS_mount(appDefaults.archiveFile, NULL, 1)) {
                fatalf("Failed to mount the application's archive file: %s", PHYSFS_getLastError());
                return 0;
            }
        } else {
            debug(DBG_FILE, DBG_FILE_DTL_GENERAL, "No application archive file specified.");
        }
        for(k = 0; _flubMemfileResources[k].name != NULL; k++) {
            debugf(DBG_FILE, DBG_FILE_DTL_PHYSFS, "Mounting memory file resource at %p", _flubMemfileResources[k].memfile);
            if(!PHYSFS_mount(PHYSFS_getMemfileName(_flubMemfileResources[k].memfile), NULL, 1)) {
                errorf("Failed to mount %s", _flubMemfileResources[k].name);
            } else {
                debugf(DBG_FILE, DBG_FILE_DTL_GENERAL, "Mounted flub resource file image [%s].", _flubMemfileResources[k].name);
            }
        }
        if(appDefaults.resources != NULL) {
            for(k = 0; appDefaults.resources[k].name != NULL; k++) {
                if(!PHYSFS_mount(PHYSFS_getMemfileName(appDefaults.resources[k].memfile), NULL, 1)) {
                    errorf("Failed to mount %s", appDefaults.resources[k].name);
                } else {
                    debugf(DBG_FILE, DBG_FILE_DTL_GENERAL, "Mounted application resource file image [%s].", appDefaults.resources[k].name);
                }
            }
        } else {
            debugf(DBG_FILE, DBG_FILE_DTL_GENERAL, "No application resource file images specified.");
        }
        debug(DBG_FILE, DBG_FILE_DTL_GENERAL, "Virtual file system started.");
    }

    _physfsCtx.init = 1;

    return 1;
}

int flubPhysfsValid(void) {
    return _physfsCtx.init;
}

void flubPhysfsShutdown(void) {
    if(!_physfsCtx.init) {
        return;
    }

    PHYSFS_deinit();
}

char *PHYSFS_gets(char *str, int maxlen, PHYSFS_File *handle) {
    char byte, *ptr;

    if(maxlen<=0) {
        return ptr;
    }

    maxlen--;

    for(ptr = str; (maxlen > 0); ptr++, maxlen--) {
        if(PHYSFS_read(handle, &byte, 1, 1) != 1) {
            break;
        }
        *ptr=byte;
        if(byte == '\n') {
            ptr++;
            break;
        }
    }
    if(ptr == str) {
        return NULL;
    }

    *ptr = '\0';

    return str;
}

int PHYSFS_fixedEof(PHYSFS_File *handle) {
    if((PHYSFS_eof(handle)) ||
       (PHYSFS_fileLength(handle) == PHYSFS_tell(handle))) {
        return 1;
    }

    return 0;
}

