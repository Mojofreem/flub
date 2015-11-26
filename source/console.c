#include <flub/console.h>
#include <flub/logger.h>
#include <flub/font.h>
#include <flub/video.h>
#include <flub/anim.h>
#include <flub/input.h>
#include <flub/data/critbit.h>
#include <flub/config.h>
#include <flub/memory.h>
#include <flub/data/circbuf.h>
#include <flub/util/string.h>
#include <flub/util/misc.h>
#include <flub/audio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <flub/flubconfig.h>
#include <flub/app.h>
#include <flub/util/color.h>
#include <flub/module.h>


//#define FLUB_CONSOLE_SHOW_DELAY		250
#define FLUB_CONSOLE_SHOW_DELAY		150
#define FLUB_CONSOLE_HEIGHT_RATIO	0.75
#define FLUB_CONSOLE_BG_COLOR       "#333333"
#define FLUB_CONSOLE_BG_ALPHA       0.85

#define FLUB_CONSOLE_BUF_SIZE		8192
#define FLUB_CONSOLE_MESH_SIZE		4096
#define FLUB_CONSOLE_CMD_MESH_SIZE	256
#define FLUB_CONSOLE_BUF_ENTRIES	256
#define FLUB_CONSOLE_CMD_LEN        256
#define FLUB_CONSOLE_PRINT_BUF      512
#define FLUB_CONSOLE_MAX_TOKENS     64


typedef struct consoleCmdEntry_s {
    char *help;
    consoleCmdHandler_t handler;
    consoleCmdHandler_t hint;
} consoleCmdEntry_t;


struct {
	int show;
	int visible;
	int height;
	int width;
	int pos;
	int lines;
	int charsPerLine;
	int cmdCharsPerLine;
    int scrollback;

	flubAnim1Pti_t slideAnim;
	Uint32 lastTick;

    int capture;
    critbit_t handlers;

	char cmdbuf[FLUB_CONSOLE_CMD_LEN];
    char cmdparse[FLUB_CONSOLE_CMD_LEN];
	int cursor;
	int offset;
	int cmdlen;
    char shiftMap[127];

	font_t *font;
	font_t *cmdFont;
	circularBuffer_t *circbuf;
	gfxMeshObj_t *mesh;
	gfxMeshObj_t *cmdMesh;

    sound_t *openSound;
    sound_t *closeSound;
} _consoleCtx = {
		.show = 0,
		.visible = 0,
		.height = 0,
		.width = 0,
		.pos = 0,
		.lines = 0,
		.charsPerLine = 0,
		.cmdCharsPerLine = 0,
        .scrollback = 0,
		//.slideAnim
		.lastTick = 0,
        .capture = 0,
        .handlers = CRITBIT_TREE_INIT,
		//.cmdbuf
        //.cmdparse
		.cursor = 0,
		.offset = 0,
		.cmdlen = 0,
        //.shiftMap
		.font = NULL,
		.circbuf = NULL,
		.mesh = NULL,
        .openSound = NULL,
        .closeSound = NULL,
	};


int _consoleVarValidator(const char *name, const char *value) {
    return 1;
}

flubCfgOptList_t _consoleVars[] = {
    {"console_bg", "", FLUB_CFG_FLAG_CLIENT, _consoleVarValidator},
    {"console_alpha", "", FLUB_CFG_FLAG_CLIENT, _consoleVarValidator},
    {"console_speed", STRINGIFY(FLUB_CONSOLE_SHOW_DELAY), FLUB_CFG_FLAG_CLIENT, _consoleVarValidator},
    {"console_height", STRINGIFY(FLUB_CONSOLE_HEIGHT_RATIO), FLUB_CFG_FLAG_CLIENT, _consoleVarValidator},
    FLUB_CFG_OPT_LIST_END
};

void _consoleLogCB( const logMessage_t *msg ) {
	consolePrintf("%s: %s", msg->levelStr, msg->message);
}

static void _consoleVideoCallback(int width, int height, int fullscreen) {
	_consoleCtx.height = ((float)height) * FLUB_CONSOLE_HEIGHT_RATIO;
	_consoleCtx.width = width;
	_consoleCtx.lines = _consoleCtx.height / fontGetHeight(_consoleCtx.font);
	flubAnimInit1Pti(&(_consoleCtx.slideAnim), 0, _consoleCtx.height, FLUB_CONSOLE_SHOW_DELAY);
    if(!_consoleCtx.visible) {
        flubAnimReverse1Pti(&(_consoleCtx.slideAnim));
    }
	_consoleCtx.charsPerLine = _consoleCtx.width / fontGetCharWidth(_consoleCtx.font, 'A');
	_consoleCtx.cmdCharsPerLine = _consoleCtx.width / fontGetCharWidth(_consoleCtx.cmdFont, 'A');
    _consoleCtx.scrollback = 0;
}

static void _consoleShiftMapInit(void) {
    int k;

    memset(_consoleCtx.shiftMap, 0, sizeof(_consoleCtx.shiftMap));
    for(k = 'a'; k <='z'; k++) {
        _consoleCtx.shiftMap[k] = k + ('A' - 'a');
    }
    _consoleCtx.shiftMap['1'] = '!';
    _consoleCtx.shiftMap['2'] = '@';
    _consoleCtx.shiftMap['3'] = '#';
    _consoleCtx.shiftMap['4'] = '$';
    _consoleCtx.shiftMap['5'] = '%';
    _consoleCtx.shiftMap['6'] = '^';
    _consoleCtx.shiftMap['7'] = '&';
    _consoleCtx.shiftMap['8'] = '*';
    _consoleCtx.shiftMap['9'] = '(';
    _consoleCtx.shiftMap['0'] = ')';
    _consoleCtx.shiftMap['-'] = '_';
    _consoleCtx.shiftMap['='] = '+';
    _consoleCtx.shiftMap['['] = '{';
    _consoleCtx.shiftMap[']'] = '}';
    _consoleCtx.shiftMap['\\'] = '|';
    _consoleCtx.shiftMap[';'] = ':';
    _consoleCtx.shiftMap['\''] = '"';
    _consoleCtx.shiftMap[','] = '<';
    _consoleCtx.shiftMap['.'] = '>';
    _consoleCtx.shiftMap['/'] = '?';
}

static void _consoleShutdown(void) {
	logRemoveNotifiee(_consoleLogCB);

	videoRemoveNotifiee(_consoleVideoCallback);
}

static void _consoleOpenHandler(SDL_Event *event, int pressed, int motion, int x, int y) {
    if(pressed) {
        consoleShow(1);
    }
}

void _consoleVarNotifyCB(const char *name, const char *value) {

}

int consoleInit(appDefaults_t *defaults) {
    _consoleCtx.openSound = audioSoundGet("resources/sounds/consoleopen.wav");
    _consoleCtx.closeSound = audioSoundGet("resources/sounds/consoleclose.wav");

    if(!flubCfgOptListAdd(_consoleVars)) {
        return 0;
    }
    if(!flubCfgPrefixNotifieeAdd("console_", _consoleVarNotifyCB)) {
        return 0;
    }

    infof("COLOR: %d %x", COLOR_FTOI(0.2), COLOR_FTOI(0.2));

    return 1;
}

static void _consoleMeshRebuild(void);
static void _consoleCmdMeshRebuild(void);
static void _consoleHelp(const char *name, int paramc, char **paramv);
static void _consoleList(const char *name, int paramc, char **paramv);
static void _consoleSet(const char *name, int paramc, char **paramv);
static void _consoleBind(const char *name, int paramc, char **paramv);
static void _consoleClear(const char *name, int paramc, char **paramv);
static void _consoleClose(const char *name, int paramc, char **paramv);
static void _consoleVersion(const char *name, int paramc, char **paramv);

int consoleStart(void) {
    _consoleCtx.cmdbuf[0] = '\0';
    _consoleShiftMapInit();

    if((_consoleCtx.circbuf = circBufInit(FLUB_CONSOLE_BUF_SIZE, FLUB_CONSOLE_BUF_ENTRIES, 1)) == NULL) {
        return 0;
    }

    if((!consoleCmdRegister("help", "help [prefix]", _consoleHelp, NULL)) ||
       (!consoleCmdRegister("list", "list [prefix]", _consoleList, NULL)) ||
       (!consoleCmdRegister("bind", "bind <key> <action>", _consoleBind, NULL)) ||
       (!consoleCmdRegister("clear", "clear", _consoleClear, NULL)) ||
       (!consoleCmdRegister("close", "close", _consoleClose, NULL)) ||
       (!consoleCmdRegister("version", "version", _consoleVersion, NULL)) ||
       (!consoleCmdRegister("set", "set <var> <value>", _consoleSet, NULL))) {
        return 0;
    }

    if(!inputActionRegister("General", 0, "showconsole", "Display the console", _consoleOpenHandler)) {
        return 0;
    }

    if((_consoleCtx.font = fontGet("courier", 12, 1)) == NULL) {
        return 0;
    }

    if((_consoleCtx.cmdFont = fontGet("courier", 12, 1)) == NULL) {
        return 0;
    }

    if((_consoleCtx.mesh = gfxMeshCreate(FLUB_CONSOLE_MESH_SIZE, GL_TRIANGLES, 1, fontTextureGet(_consoleCtx.font))) == NULL) {
        return 0;
    }

    if((_consoleCtx.cmdMesh = gfxMeshCreate(FLUB_CONSOLE_CMD_MESH_SIZE, GL_TRIANGLES, 1, fontTextureGet(_consoleCtx.cmdFont))) == NULL) {
        return 0;
    }

    if(!videoAddNotifiee(_consoleVideoCallback)) {
        return 0;
    }

    _consoleVideoCallback(*videoWidth, *videoHeight, videoIsFullscreen());

    _consoleCmdMeshRebuild();

    logAddNotifiee(_consoleLogCB);

    atexit(_consoleShutdown);

    return 1;
}

static void _consoleDeleteChar(int pos) {
    if(pos < 0) {
        return;
    }
    if(pos == (_consoleCtx.cmdlen - 1)) {
        _consoleCtx.cmdbuf[pos] = '\0';
    } else {
        memmove(_consoleCtx.cmdbuf + pos, _consoleCtx.cmdbuf + pos + 1, _consoleCtx.cmdlen - pos);
    }
    _consoleCtx.cmdlen--;
}

static void _consoleScroll(int amount) {
    _consoleCtx.scrollback += amount;
    if(_consoleCtx.scrollback > (circBufGetCount(_consoleCtx.circbuf) - _consoleCtx.lines)) {
        _consoleCtx.scrollback = (circBufGetCount(_consoleCtx.circbuf) - _consoleCtx.lines);
    }
    if(_consoleCtx.scrollback < 0) {
        _consoleCtx.scrollback = 0;
    }
}

static char **_consoleCmdParse(int *count) {
    static char *tokens[FLUB_CONSOLE_MAX_TOKENS];

    memcpy(_consoleCtx.cmdparse, _consoleCtx.cmdbuf, sizeof(_consoleCtx.cmdparse));
    *count = strTokenize(_consoleCtx.cmdparse, tokens, FLUB_CONSOLE_MAX_TOKENS);

    return tokens;
}

static void _consoleInputHandler(SDL_Event *event, int pressed, int motion, int x, int y) {
    int updatelog = 0;
    char *ptr;
    int k;
    char **tokens;
    int tcount;
    consoleCmdEntry_t *entry;

    switch(event->type) {
        case SDL_KEYUP:
            /*
            switch(event->key.keysym.sym) {
                case SDLK_BACKQUOTE:
                    consoleShow(0);
                    break;
            }
             */
            break;
        case SDL_KEYDOWN:
            switch(event->key.keysym.sym) {
                case SDLK_BACKQUOTE:
                    consoleShow(0);
                    break;
                //case SDLK_BACKQUOTE:
                    // We catch this to avoid an unwanted stray backtick when opening/closing the console
                    break;
                case SDLK_TAB:
                    // Command hinting
                    break;
                case SDLK_CAPSLOCK:
                    break;
                case SDLK_BACKSPACE:
                    if(_consoleCtx.cursor == 0) {
                        break;
                    }
                    _consoleDeleteChar(_consoleCtx.cursor - 1);
                    _consoleCtx.cursor--;
                    break;
                case SDLK_RETURN:
                    if(_consoleCtx.cmdlen > 0) {
                        _consoleCtx.cmdbuf[_consoleCtx.cmdlen] = '\0';
                        tokens = _consoleCmdParse(&tcount);
                        if(tcount > 0) {
                            if(critbitContains(&(_consoleCtx.handlers), tokens[0], ((void **)&entry))) {
                                entry->handler(tokens[0], tcount - 1, tokens + 1);
                            } else {
                                consolePrintf("Unknown command: %s", _consoleCtx.cmdbuf);
                            }
                        }
                        _consoleCtx.cmdbuf[0] = '\0';
                        _consoleCtx.cmdlen = 0;
                        _consoleCtx.cursor = 0;
                    }
                    break;
                case SDLK_UP:
                    break;
                case SDLK_DOWN:
                    break;
                case SDLK_RIGHT:
                    if(event->key.keysym.mod & KMOD_CTRL) {
                        // Skip to the start of the next word
                        ptr = (_consoleCtx.cmdbuf + _consoleCtx.cursor);
                        for(; isspace(*ptr); ptr++);
                        for(; ((*ptr != '\0') && (!isspace(*ptr))); ptr++);
                        if(*ptr == '\0') {
                            // There is no next word
                            _consoleCtx.cursor = _consoleCtx.cmdlen;
                        } else {
                            _consoleCtx.cursor = ptr - _consoleCtx.cmdbuf;
                        }
                    } else if(_consoleCtx.cursor < _consoleCtx.cmdlen) {
                        _consoleCtx.cursor++;
                    }
                    break;
                case SDLK_LEFT:
                    if(event->key.keysym.mod & KMOD_CTRL) {
                        // Skip to the start of the previous word
                        ptr = (_consoleCtx.cmdbuf + _consoleCtx.cursor);
                        for(; ((isspace(*ptr)) && (ptr > _consoleCtx.cmdbuf)); ptr--);
                        for(; ((!isspace(*ptr)) && (ptr > _consoleCtx.cmdbuf)); ptr--);
                        if(ptr < _consoleCtx.cmdbuf) {
                            ptr = _consoleCtx.cmdbuf;
                        }
                        _consoleCtx.cursor = ptr - _consoleCtx.cmdbuf;
                    } else if(_consoleCtx.cursor > 0) {
                        _consoleCtx.cursor--;
                    }
                    break;
                case SDLK_HOME:
                    _consoleCtx.cursor = 0;
                    break;
                case SDLK_END:
                    _consoleCtx.cursor = _consoleCtx.cmdlen;
                    break;
                case SDLK_DELETE:
                    if(_consoleCtx.cursor < _consoleCtx.cmdlen) {
                        _consoleDeleteChar(_consoleCtx.cursor);
                    }
                    break;
                case SDLK_PAGEUP:
                    _consoleScroll(_consoleCtx.lines - 1);
                    updatelog = 1;
                    break;
                case SDLK_PAGEDOWN:
                    _consoleScroll(-_consoleCtx.lines - 1);
                    updatelog = 1;
                    break;
                default:
                    // Process the key
                    if((isprint(event->key.keysym.sym) && (event->key.keysym.sym >= 32))) {
                        if(_consoleCtx.cursor < (sizeof(_consoleCtx.cmdbuf) - 1)) {
                            k = event->key.keysym.sym;
                            if(event->key.keysym.mod & KMOD_SHIFT) {
                                if(_consoleCtx.shiftMap[k] != 0) {
                                    k = _consoleCtx.shiftMap[k];
                                }
                            } else if(event->key.keysym.mod & KMOD_CAPS) {
                                if((k >= 'a') && (k <= 'z')) {
                                    k += 'A' - 'a';
                                }
                            }

                            if(_consoleCtx.cursor < _consoleCtx.cmdlen) {
                                // Insert
                                memmove(_consoleCtx.cmdbuf + _consoleCtx.cursor + 1, _consoleCtx.cmdbuf + _consoleCtx.cursor, _consoleCtx.cmdlen - _consoleCtx.cursor);

                                _consoleCtx.cmdbuf[_consoleCtx.cursor] = k;
                            } else {
                                // Append
                                _consoleCtx.cmdbuf[_consoleCtx.cursor] = k;
                            }
                            _consoleCtx.cursor++;
                            _consoleCtx.cmdlen++;
                        }
                    }
            }
    }
    if(_consoleCtx.cmdlen < (_consoleCtx.cmdCharsPerLine - 1)) {
        if(_consoleCtx.offset > 0) {
            _consoleCtx.offset = 0;
        }
    }
    if(_consoleCtx.cursor < _consoleCtx.offset) {
        _consoleCtx.offset = _consoleCtx.cursor;
    }
    if((_consoleCtx.cursor - _consoleCtx.offset) >= _consoleCtx.cmdCharsPerLine) {
        _consoleCtx.offset = _consoleCtx.cursor - _consoleCtx.cmdCharsPerLine;
    }

    _consoleCmdMeshRebuild();
    if(updatelog) {
        _consoleMeshRebuild();
    }
}

static void _consoleCmdMeshRebuild(void) {
	int x;
	int y;
    int len;
    int count;

	y = _consoleCtx.height - fontGetHeight(_consoleCtx.font);

	gfxMeshClear(_consoleCtx.cmdMesh);
	fontPos(0, y);
	fontSetColor(0.0, 1.0, 0.0);
	if(_consoleCtx.offset) { // content is scrolled
		fontBlitCMesh(_consoleCtx.cmdMesh, _consoleCtx.cmdFont, '<');
	} else {
		fontBlitCMesh(_consoleCtx.cmdMesh, _consoleCtx.cmdFont, '>');		
	}

	// Set a cursor
	x = fontGetCharWidth(_consoleCtx.cmdFont, 'A') * (_consoleCtx.cursor - _consoleCtx.offset + 1);
	fontPos(x, y);
	fontBlitCMesh(_consoleCtx.cmdMesh, _consoleCtx.cmdFont, '_');

	// Render the visible command line portion
	fontSetColor(1.0, 1.0, 1.0);
	fontPos(fontGetCharWidth(_consoleCtx.cmdFont, '_'), y);
    if((_consoleCtx.cmdlen - _consoleCtx.offset) < _consoleCtx.charsPerLine) {
        count = _consoleCtx.cmdlen - _consoleCtx.offset;
    } else {
        count = _consoleCtx.charsPerLine;
    }
	fontBlitStrNMesh(_consoleCtx.cmdMesh, _consoleCtx.cmdFont, _consoleCtx.cmdbuf + _consoleCtx.offset, count);
}

int consoleUpdate(Uint32 ticks, Uint32 elapsed2) {
	int display = 0;
    // TODO - use the passed elapsed value
	Uint32 elapsed = flubAnimTicksElapsed(&(_consoleCtx.lastTick), ticks);

	if(_consoleCtx.visible) {
		flubAnimTick1Pti(&(_consoleCtx.slideAnim), elapsed, &(_consoleCtx.pos));
	}
	
	if(_consoleCtx.show) {
		display = 1;
	} else {
		if(_consoleCtx.pos > 0) {
			display = 1;
		} else {
			_consoleCtx.visible = 0;
		}
	}

	if(display){
		videoOrthoMode();
		glDisable(GL_TEXTURE_2D);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		// Draw the console background
		glColor4f(0.2, 0.2, 0.2, 0.85);
		glBegin(GL_QUADS);
			glVertex2i(0, 0);
			glVertex2i(_consoleCtx.width, 0);
			glVertex2i(_consoleCtx.width, _consoleCtx.pos);
			glVertex2i(0, _consoleCtx.pos);
		glEnd();

		// Draw the console text
		glLoadIdentity();
	    glTranslated(0, _consoleCtx.pos - _consoleCtx.height, 0);
		gfxMeshRender(_consoleCtx.mesh);

		// Draw the console input field
		glLoadIdentity();
	    glTranslated(0, _consoleCtx.pos - _consoleCtx.height, 0);
		gfxMeshRender(_consoleCtx.cmdMesh);
	}
    return 1;
}

static char *_initDeps[] = {"video", "texture", "font", NULL};
static char *_updatePreceed[] = {"video", NULL};
static char *_startDeps[] = {"video", "font", NULL};
flubModuleCfg_t flubModuleConsole = {
    .name = "console",
    .init = consoleInit,
    .start = consoleStart,
    .update = consoleUpdate,
    .initDeps = _initDeps,
    .startDeps = _startDeps,
    .updatePreceed = _updatePreceed,
};


void consoleShow(int show) {
	if(_consoleCtx.show != show) {
		flubAnimReverse1Pti(&(_consoleCtx.slideAnim));
		_consoleCtx.show = show;
		if(show) {
			_consoleCtx.visible = 1;
            inputEventCapture(_consoleInputHandler);
            _consoleCtx.capture = 1;
            if(_consoleCtx.openSound != NULL) {
                Mix_PlayChannel(0, _consoleCtx.openSound, 0);
            }
		} else {
            if(_consoleCtx.capture) {
                _consoleCtx.capture = 0;
                inputEventRelease(_consoleInputHandler);
            }
            if(_consoleCtx.closeSound != NULL) {
                Mix_PlayChannel(0, _consoleCtx.closeSound, 0);
            }
		}
	}
}

void consoleClear(void) {
    circBufClear(_consoleCtx.circbuf);
    _consoleMeshRebuild();
}

int consoleVisible(void) {
	return _consoleCtx.show;
}

static void _consoleMeshRebuild(void) {
	int lines;
	int count;
	int line;
	int pos;
	char *buf;
	int scroll;

	// Determine the line count
	count = circBufGetCount(_consoleCtx.circbuf);
	if(count > (_consoleCtx.lines - 1)) {
		lines = (_consoleCtx.lines - 1);
	} else {
		lines = count;
	}

	// Generate the scrollback mesh
	if(count > lines) {
        if(_consoleCtx.scrollback > (count - lines)) {
            scroll = count - lines;
        } else {
            scroll = _consoleCtx.scrollback;
        }
    } else {
        scroll = 0;
    }

	gfxMeshClear(_consoleCtx.mesh);
	fontSetColor(1.0, 1.0, 1.0);
	pos = _consoleCtx.height - (fontGetHeight(_consoleCtx.font) * 2);
	for(line = count - 1; lines; line--) {
		if(!circBufGetEntry(_consoleCtx.circbuf, line - scroll, ((void **)&buf), NULL)) {
			continue;
		}
		fontPos(0, pos);
		fontBlitStrMesh(_consoleCtx.mesh, _consoleCtx.font, buf);
		lines--;
		pos -= fontGetHeight(_consoleCtx.font);
	}
}

void consolePrint(const char *str) {
	char *entry;
	int pos;

	while(1) {
		pos = fontGetStrLenWrap(_consoleCtx.font, str, *videoWidth);
        //pos = strlen(str);
		entry = circBufAllocNextEntryPtr(_consoleCtx.circbuf, pos + 1);
		strncpy(entry, str, pos);
		entry[pos] = '\0';
		if(str[pos] == '\0') {
			break;
		}
		str += pos;
		_consoleCtx.scrollback--;
		if(_consoleCtx.scrollback < 0) {
			_consoleCtx.scrollback = 0;
		}
	}

	_consoleMeshRebuild();
}

int consoleCmdRegister(const char *name, const char *help, consoleCmdHandler_t handler, consoleCmdHandler_t hint) {
    consoleCmdEntry_t *entry;

    entry = util_calloc(sizeof(consoleCmdEntry_t), 0, NULL);

    entry->help = util_strdup(help);
    entry->handler = handler;
    entry->hint = hint;

    if(!critbitInsert(&(_consoleCtx.handlers), name, entry, NULL)) {
        errorf("Failed to add console command \"%s\"", name);
        return 0;
    }
    return 1;
}

static int _consoleCmdDeleteCB(void *data, void *arg) {
    consoleCmdEntry_t *entry = (consoleCmdEntry_t *)data;

    util_free(entry->help);
    util_free(entry);

    return 1;
}

int consoleCmdUnregister(const char *name) {
    return critbitDelete(&(_consoleCtx.handlers), name, _consoleCmdDeleteCB, NULL);
}

int consoleCmdListRegister(consoleCmd_t *list) {
    int k;

    for(k = 0; list[k].name != NULL; k++) {
        if(!consoleCmdRegister(list[k].name, list[k].help, list[k].handler, list[k].hint)) {
            return 0;
        }
    }
    return 1;
}

void consolePrintf(const char *fmt, ...) {
    char buf[FLUB_CONSOLE_PRINT_BUF];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    buf[sizeof(buf) - 1] = '\0';

    consolePrint(buf);
}

static int _consoleCmdEnumCB(const char *name, void *data, void *arg) {
    consolePrint(name);
    return 1;
}

static void _consoleHelp(const char *name, int paramc, char **paramv) {
    void *data;
    char *prefix = "";

    if(paramc > 0) {
        prefix = paramv[0];
    }

    consolePrint("Console commands:");

    critbitAllPrefixed(&(_consoleCtx.handlers), prefix, _consoleCmdEnumCB, NULL);
}

static int _consoleCfgEnumCB(void *ctx, const char *name, const char *value, const char *defValue, int flags) {
    consolePrintf("* %s = %s", name, value);
    return 1;
}

static void _consoleList(const char *name, int paramc, char **paramv) {
    char *prefix = "";

    if(paramc > 0) {
        prefix = paramv[0];
    }

    consolePrintf("Config vars:");
    flubCfgOptEnum(prefix, _consoleCfgEnumCB, NULL);
}

static void _consoleSet(const char *name, int paramc, char **paramv) {
    if(paramc != 2) {
        consolePrint("ERROR: set <name> <value>");
        return;
    }

    flubCfgOptStringSet(paramv[0], paramv[1], 0);
}

static void _consoleBind(const char *name, int paramc, char **paramv) {
    if(paramc != 2) {
        consolePrint("ERROR: bind <key> <event>");
        return;
    }

    inputActionBind(paramv[1], paramv[0]);
}

static void _consoleClear(const char *name, int paramc, char **paramv) {
    consoleClear();
}

static void _consoleClose(const char *name, int paramc, char **paramv) {
    consoleShow(0);
}

static void _consoleVersion(const char *name, int paramc, char **paramv) {
    consolePrintf("%s version %d.%d", appDefaults.title, appDefaults.major, appDefaults.minor);
    consolePrintf("Flub version %s", FLUB_VERSION_STRING);
}

void consoleBgImageSet(texture_t *tex);

void consolePrint(const char *str);
void consolePrintf(const char *fmt, ...);
void consolePrintQC(const char *str);
void consolePrintfQC(const char *fmt, ...);

int consoleWindowCharWidth(void);
void consoleColorSet(int red, int green, int blue);

void consoleTabularDivider(int border, int columns, int *colWidths);
void consoleTabularPrint(int border, int columns, int *colWidths, const char *colStrs);

/*
typedef int (*consoleCmdParamValidator_t)(void *context, int paramPos,
                                          const char *param);
typedef int (*consoleCmdParamCB_t)(void *context, int paramPos,
                                   const char **params, char *acBuffer,
                                   int acLen);

typedef struct consoleCmdItem_s {
    const char *name;
    const char *help;
    const char *detailedHelp;
    consoleCmdParamCB_t validator;
    consoleCmdParamCB_t hinter;
    consoleCmdHandler_t handler;
} consoleCmdItem_t;
*/

