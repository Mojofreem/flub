/*
 * Memory archive (compiled resource) I/O support routines for PhysicsFS.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Liam Chasteen.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "physfs.h"
#include "physfs_memfile.h"

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"


typedef struct __MEM_file
{
    const unsigned char *data;
    int size;
    int pos;
} MEM_file;


void (*PHYSFS_log_callback)(const char *msg) = NULL;

#include <stdarg.h>
static void log_messagef(const char *fmt, ...) {
    va_list ap;
    char buf[1024];

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf) - 2, fmt, ap);
    va_end(ap);
    buf[sizeof(buf) - 2] = '\0';
    if(PHYSFS_log_callback != NULL) {
        PHYSFS_log_callback(buf);
    }
}

#define log(m)  if(PHYSFS_log_callback != NULL) {PHYSFS_log_callback(m);}
#define logf(f,...) log_messagef(f,##__VA_ARGS__)


static PHYSFS_memfile *MEM_ptrFromName(const char *name)
{
    char ptrBuf[32];
    const char *ptr;
    int size;
    PHYSFS_memfile *memfile;

    logf("MEM: Determining ptr from name [%s]", name);
    if(!strncmp(name, PHYSFS_memfilePrefix, strlen(PHYSFS_memfilePrefix)))
    {
        log("MEM: Prefix is valid");
        if((ptr = strchr(name, '.')) != NULL) {
            if(!strcmp(ptr, PHYSFS_memfileExtension)) {
                log("MEM: Extension is valid");
                size = ptr - (name + strlen(PHYSFS_memfilePrefix));
                if(size >= sizeof(ptrBuf)) {
                    // Whoa. Pointer value is invalid.
                    log("MEM: Pointer is whack-a-doodle");
                    return NULL;
                }
                strncpy(ptrBuf, name + strlen(PHYSFS_memfilePrefix), size);
                ptrBuf[size] = '\0';
                memfile = (PHYSFS_memfile *)(strtol(ptrBuf, NULL, 0));
#ifdef MACOSX
                logf("MEM: Name [%s] has value [%p]", name, memfile);
#else // MACOSX
                logf("MEM: Name [%s] has value [0x%p]", name, memfile);
#endif // MACOSX
                return memfile;
            }
        }
    }
    log("MEM: Prefix is invalid");
    return NULL;
} /* MEM_ptrFromName */


static PHYSFS_sint64 MEM_read(fvoid *opaque, void *buffer,
                              PHYSFS_uint32 objSize, PHYSFS_uint32 objCount)
{
    MEM_file *file = (MEM_file *)opaque;
    int size;

    if(file->pos == file->size) {
        return -1;
    }
    size = objSize * objCount;
    while((file->pos + size) > file->size) {
        size -= objSize;
        objCount--;
        if((size <= 0) || (objCount <= 0)) {
            return -1;
        }
    }
    memcpy(buffer, file->data + file->pos, size);
    file->pos += size;
    return(objCount);
} /* MEM_read */


static PHYSFS_sint64 MEM_write(fvoid *opaque, const void *buffer,
                               PHYSFS_uint32 objSize, PHYSFS_uint32 objCount)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, -1);
} /* MEM_write */


static int MEM_eof(fvoid *opaque)
{
    if(((MEM_file *)opaque)->pos >= ((MEM_file *)opaque)->size)
    {
        return 1;
    }
    return(0);
} /* MEM_eof */


static PHYSFS_sint64 MEM_tell(fvoid *opaque)
{
    return(((MEM_file *)opaque)->pos);
} /* MEM_tell */


static int MEM_seek(fvoid *opaque, PHYSFS_uint64 offset)
{
    if(((MEM_file *)opaque)->size < offset) {
        offset = ((MEM_file *)opaque)->size;
    }
    ((MEM_file *)opaque)->pos = offset;
    return(1);
} /* MEM_seek */


static PHYSFS_sint64 MEM_fileLength(fvoid *opaque)
{
    return ((MEM_file *)opaque)->size;
} /* MEM_fileLength */


static int MEM_fileClose(fvoid *opaque)
{
    /*
     * we manually flush the buffer, since that's the place a close will
     *  most likely fail, but that will leave the file handle in an undefined
     *  state if it fails. Flush failures we can recover from.
     */
    allocator.Free(opaque);
    return(1);
} /* MEM_fileClose */


static int MEM_isArchive(const char *filename, int forWriting)
{
    if(MEM_ptrFromName(filename) != NULL) {
        logf("MEM: File [%s] is a memory archive", filename);
        return 1;
    }
    logf("MEM: File [%s] is not a memory archive", filename);
    return 0;
} /* MEM_isArchive */


static void *MEM_openArchive(const char *name, int forWriting)
{
    PHYSFS_memfile *memfile;

    /* !!! FIXME: when is this not called right before openArchive? */
    BAIL_IF_MACRO(!MEM_isArchive(name, forWriting),
                    ERR_UNSUPPORTED_ARCHIVE, 0);

    memfile = MEM_ptrFromName(name);
    return(memfile);
} /* MEM_openArchive */


static void MEM_enumerateFiles(dvoid *opaque, const char *dname,
                               int omitSymLinks, PHYSFS_EnumFilesCallback cb,
                               const char *origdir, void *callbackdata)
{
    PHYSFS_memfile *memfile = (PHYSFS_memfile *)opaque;
    PHYSFS_memfile *walk;

    if(*dname == '\0')
    {
        for(walk = memfile; walk != NULL; walk = walk->next) {
            cb(callbackdata, origdir, walk->path);
        }
    }
} /* MEM_enumerateFiles */


static int MEM_exists(dvoid *opaque, const char *name)
{
    PHYSFS_memfile *memfile = (PHYSFS_memfile *)opaque;
    PHYSFS_memfile *walk;

    for(walk = memfile; walk != NULL; walk = walk->next) {
        if(!strcmp(name, memfile->path)) {
            return 1;
        }
    }
    return 0;
} /* MEM_exists */


static int MEM_isDirectory(dvoid *opaque, const char *name, int *fileExists)
{
    return 0;
} /* MEM_isDirectory */


static int MEM_isSymLink(dvoid *opaque, const char *name, int *fileExists)
{
    return 0;
} /* MEM_isSymLink */


static PHYSFS_sint64 MEM_getLastModTime(dvoid *opaque,
                                        const char *name,
                                        int *fileExists)
{
    return 0;
} /* MEM_getLastModTime */


static fvoid *MEM_openRead(dvoid *opaque, const char *fnm, int *exist)
{
    PHYSFS_memfile *memfile = (PHYSFS_memfile *)opaque;
    PHYSFS_memfile *walk;
    MEM_file *file;
    const char *check;

    if(fnm[0] == '/') {
        fnm++;
    }
    logf("Checking in mem for [%s]", fnm);
    for(walk = memfile; walk != NULL; walk = walk->next) {
        check = walk->path;
        if(check[0] == '/') {
            check++;
        }
        if(!strcmp(fnm, check)) {
            logf("....[%s]: YES", check);
            file = (MEM_file *) allocator.Malloc(sizeof(MEM_file));
            if(file == NULL)
            {
                BAIL_MACRO(ERR_OUT_OF_MEMORY, NULL);
            }
            file->data = walk->data;
            file->size = walk->length;
            file->pos = 0;

            return file;
        }
        else
        {
            logf("....[%s]: NO", check);
        }
    }
    return NULL;
} /* MEM_openRead */


static fvoid *MEM_openWrite(dvoid *opaque, const char *filename)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, NULL);
} /* MEM_openWrite */


static fvoid *MEM_openAppend(dvoid *opaque, const char *filename)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, NULL);
} /* MEM_openAppend */


static int MEM_remove(dvoid *opaque, const char *name)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, 0);
} /* MEM_remove */


static int MEM_mkDir(dvoid *opaque, const char *name)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, 0);
} /* MEM_mkdir */


static void MEM_dirClose(dvoid *opaque)
{
    // Nothing to do here
} /* MEM_dirClose */



const PHYSFS_ArchiveInfo __PHYSFS_ArchiveInfo_MEM =
{
    "memfile",
    MEM_ARCHIVE_DESCRIPTION,
    "Liam Chasteen <liam.chasteen@gmail.com>",
    "http://liamchasteen.net/projects/physfs/",
};



const PHYSFS_Archiver __PHYSFS_Archiver_MEM =
{
    &__PHYSFS_ArchiveInfo_MEM,
    MEM_isArchive,          /* isArchive() method      */
    MEM_openArchive,        /* openArchive() method    */
    MEM_enumerateFiles,     /* enumerateFiles() method */
    MEM_exists,             /* exists() method         */
    MEM_isDirectory,        /* isDirectory() method    */
    MEM_isSymLink,          /* isSymLink() method      */
    MEM_getLastModTime,     /* getLastModTime() method */
    MEM_openRead,           /* openRead() method       */
    MEM_openWrite,          /* openWrite() method      */
    MEM_openAppend,         /* openAppend() method     */
    MEM_remove,             /* remove() method         */
    MEM_mkDir,              /* mkDir() method          */
    MEM_dirClose,           /* dirClose() method       */
    MEM_read,               /* read() method           */
    MEM_write,              /* write() method          */
    MEM_eof,                /* eof() method            */
    MEM_tell,               /* tell() method           */
    MEM_seek,               /* seek() method           */
    MEM_fileLength,         /* fileLength() method     */
    MEM_fileClose           /* fileClose() method      */
};

/* end of mem.c ... */

