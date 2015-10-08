/*
 * This code provides a glue layer between PhysicsFS and Simple Directmedia
 *  Layer's (SDL) RWops i/o abstraction.
 *
 * License: this code is public domain. I make no warranty that it is useful,
 *  correct, harmless, or environmentally safe.
 *
 * This particular file may be used however you like, including copying it
 *  verbatim into a closed-source project, exploiting it commercially, and
 *  removing any trace of my name from the source (although I hope you won't
 *  do that). I welcome enhancements and corrections to this file, but I do
 *  not require you to send me patches if you make changes.
 *
 * Unless otherwise stated, the rest of PhysicsFS falls under the GNU Lesser
 *  General Public License: http://www.gnu.org/licenses/lgpl.txt
 *
 * SDL falls under the LGPL, too. You can get SDL at http://www.libsdl.org/
 *
 *  This file was written by Ryan C. Gordon. (icculus@clutteredmind.org).
 */

#include <stdio.h>  // used for SEEK_SET, SEEK_CUR, SEEK_END ...
#include <stddef.h>
 
#include <flub/physfsrwops.h>
 

static Sint64 physfsrwops_seek(SDL_RWops *rw, Sint64 offset, int whence)
{
    PHYSFS_file *handle = (PHYSFS_file *) rw->hidden.unknown.data1;
    int pos = 0;

    if (whence == SEEK_SET)
    {
        pos = offset;
    }
    else if (whence == SEEK_CUR)
    {
        int current = PHYSFS_tell(handle);
        if (current == -1)
        {
            SDL_SetError("Can't find position in file: %s",
                          PHYSFS_getLastError());
            return(-1);
        }

        if (offset == 0)  // this is a "tell" call. We're done.
            return( current );

        pos = current + offset;
    }
    else if (whence == SEEK_END)
    {
        int len = PHYSFS_fileLength(handle);
        if (len == -1)
        {
            SDL_SetError("Can't find end of file: %s", PHYSFS_getLastError());
            return(-1);
        }

        pos = len + offset;
    }
    else
    {
        SDL_SetError("Invalid 'whence' parameter.");
        return(-1);
    }

    if (!PHYSFS_seek(handle, pos))
    {
        SDL_SetError("PhysicsFS error: %s", PHYSFS_getLastError());
        return(-1);
    }

    return(pos);
}


static size_t physfsrwops_read(SDL_RWops *rw, void *ptr, size_t size, size_t maxnum)
{
    PHYSFS_file *handle = (PHYSFS_file *) rw->hidden.unknown.data1;
    int rc;
	 
	rc = PHYSFS_read(handle, ptr, size, maxnum);
    if (rc != maxnum)
    {
        if (!PHYSFS_eof(handle)) // not EOF? Must be an error.
            SDL_SetError("PhysicsFS error: %s", PHYSFS_getLastError());
    }

    return(rc);
}


static size_t physfsrwops_write(SDL_RWops *rw, const void *ptr, size_t size, size_t num)
{
    PHYSFS_file *handle = (PHYSFS_file *) rw->hidden.unknown.data1;
    int rc;
	 
	rc = PHYSFS_write(handle, ptr, size, num);
    if (rc != num)
        SDL_SetError("PhysicsFS error: %s", PHYSFS_getLastError());

    return(rc);
}


static int physfsrwops_close(SDL_RWops *rw)
{
    PHYSFS_file *handle = (PHYSFS_file *) rw->hidden.unknown.data1;

    if (!PHYSFS_close(handle))
    {
        SDL_SetError("PhysicsFS error: %s", PHYSFS_getLastError());
        return(-1);
    }

    SDL_FreeRW(rw);
    return(0);
}


static SDL_RWops *create_rwops(PHYSFS_file *handle)
{
    SDL_RWops *retval = NULL;

    if (handle == NULL)
        SDL_SetError("PhysicsFS error: %s", PHYSFS_getLastError());
    else
    {
        retval = SDL_AllocRW();
        if (retval != NULL)
        {
            retval->seek  = physfsrwops_seek;
            retval->read  = physfsrwops_read;
            retval->write = physfsrwops_write;
            retval->close = physfsrwops_close;
            retval->hidden.unknown.data1 = handle;
        }
    }

    return(retval);
}


SDL_RWops *PHYSFSRWOPS_makeRWops(PHYSFS_file *handle)
{
    SDL_RWops *retval = NULL;
    if (handle == NULL)
        SDL_SetError("NULL pointer passed to PHYSFSRWOPS_makeRWops().");
    else
        retval = create_rwops(handle);

    return(retval);
}


SDL_RWops *PHYSFSRWOPS_openRead(const char *fname)
{
    return(create_rwops(PHYSFS_openRead(fname)));
}


SDL_RWops *PHYSFSRWOPS_openWrite(const char *fname)
{
    return(create_rwops(PHYSFS_openWrite(fname)));
}


SDL_RWops *PHYSFSRWOPS_openAppend(const char *fname)
{
    return(create_rwops(PHYSFS_openAppend(fname)));
}


