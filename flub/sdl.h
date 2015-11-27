#ifndef _FLUB_SDL_HEADER_
#define _FLUB_SDL_HEADER_

#include <SDL_events.h>
#include <flub/module.h>


///////////////////////////////////////////////////////////////////////////////
/// \brief Filters SDL input events to generate text input.
///
/// Extracts text characters from SDL input events. Useful for text input
/// parsing. This function handles keyboard mod states (CapsLock and Shift) to
/// return the appropriate character.
///
/// \param event An SDL event message
/// \param c Pointer to a character buffer to store a text character, if found.
/// \return True if the event generated a text character, false if not.
///////////////////////////////////////////////////////////////////////////////
int flubSDLTextInputFilter(SDL_Event *event, char *c);


#endif // _FLUB_SDL_HEADER_
