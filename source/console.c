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
#include <stddef.h>


//#define FLUB_CONSOLE_SHOW_DELAY		250
#define FLUB_CONSOLE_SHOW_DELAY		150
#define FLUB_CONSOLE_HEIGHT_RATIO	0.75
#define FLUB_CONSOLE_BG_COLOR       "#333333D9"
#define FLUB_CONSOLE_BG_ALPHA       0.85

#define CONSOLE_CMDLINE_LINE_LEN    256
#define CONSOLE_CMDLINE_HISTORY_LEN 10
#define FLUB_CONSOLE_BUF_SIZE		8192
#define FLUB_CONSOLE_MESH_SIZE		4096
#define FLUB_CONSOLE_CMD_MESH_SIZE	512
#define FLUB_CONSOLE_BUF_ENTRIES	256
#define FLUB_CONSOLE_CMD_LEN        256
#define FLUB_CONSOLE_PRINT_BUF      512
#define FLUB_CONSOLE_MAX_TOKENS     64


/////////////////////////////////////////////////////////////////////////////
// console module registration
/////////////////////////////////////////////////////////////////////////////

int flubConsoleInit(appDefaults_t *defaults);
int flubConsoleStart(void);
int flubConsoleUpdate(Uint32 ticks, Uint32 elapsed);
void flubConsoleShutdown(void);

static char *_initDeps[] = {"video", "texture", "font", NULL};
static char *_updatePreceed[] = {"video", NULL};
static char *_startDeps[] = {"video", "font", NULL};
flubModuleCfg_t flubModuleConsole = {
    .name = "console",
    .init = flubConsoleInit,
    .start = flubConsoleStart,
    .update = flubConsoleUpdate,
    .initDeps = _initDeps,
    .startDeps = _startDeps,
    .updatePreceed = _updatePreceed,
};

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

typedef struct conCmdLineEntry_s {
    int len;
    int pos;
    int buflen;
    char buf[0];
} conCmdLineEntry_t;

#define CON_HIST_WALK_NONE  -1

typedef struct conCmdLineCtx_s {
    int histCount;
    int histWalk;
    int active;
    conCmdLineEntry_t **history;
    int offset;
    char buffer[0];
} conCmdLineCtx_t;


typedef struct consoleCmdEntry_s {
    char *help;
    consoleCmdHandler_t handler;
    consoleCmdHandler_t hint;
} consoleCmdEntry_t;

struct {
    conCmdLineCtx_t *cmdline;

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

    texture_t *bgTexture;
    flubColor4f_t colorBackground;
    flubColor4f_t colorCursor;
    flubColor4f_t colorDimCursor;
    flubColor4f_t colorCmdText;
    flubColor4f_t colorOutText;
} _consoleCtx = {
        .handlers = CRITBIT_TREE_INIT,
        .colorBackground = {
            .red = 0.2,
            .green = 0.2,
            .blue = 0.2,
            .alpha = 0.85,
        },
        .colorCursor = {
            .green = 1.0,
        },
        .colorCmdText = {
            .red = 1.0,
            .green = 1.0,
            .blue = 1.0,
        },
        .colorOutText = {
            .red = 0.7,
            .green = 0.7,
            .blue = 0.7,
        },
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

conCmdLineCtx_t *consoleCmdLineInit(int lineLen, int historyLen);
void consoleCmdLineWidthChange(conCmdLineCtx_t *ctx);

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
    consoleCmdLineWidthChange(_consoleCtx.cmdline);
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

void flubConsoleShutdown(void) {
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

int flubConsoleInit(appDefaults_t *defaults) {
    _consoleCtx.openSound = audioSoundGet("resources/sounds/consoleopen.wav");
    _consoleCtx.closeSound = audioSoundGet("resources/sounds/consoleclose.wav");

    if(!flubCfgOptListAdd(_consoleVars)) {
        return 0;
    }
    if(!flubCfgPrefixNotifieeAdd("console_", _consoleVarNotifyCB)) {
        return 0;
    }

    _consoleCtx.cmdline = consoleCmdLineInit(CONSOLE_CMDLINE_LINE_LEN, CONSOLE_CMDLINE_HISTORY_LEN);

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

int flubConsoleStart(void) {
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

    if((_consoleCtx.mesh = gfxMeshCreate(MESH_QUAD_SIZE(FLUB_CONSOLE_MESH_SIZE), GL_TRIANGLES, 1, fontTextureGet(_consoleCtx.font))) == NULL) {
        return 0;
    }

    if((_consoleCtx.cmdMesh = gfxMeshCreate(MESH_QUAD_SIZE(FLUB_CONSOLE_CMD_MESH_SIZE), GL_TRIANGLES, 1, fontTextureGet(_consoleCtx.cmdFont))) == NULL) {
        return 0;
    }

    if(!videoAddNotifiee(_consoleVideoCallback)) {
        return 0;
    }

    _consoleVideoCallback(*videoWidth, *videoHeight, videoIsFullscreen());

    _consoleCmdMeshRebuild();

    logAddNotifiee(_consoleLogCB);

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

static char **_consoleCmdParse(int *count, char *input) {
    static char *tokens[FLUB_CONSOLE_MAX_TOKENS];

    memcpy(_consoleCtx.cmdparse, input, sizeof(_consoleCtx.cmdparse));
    *count = strTokenize(_consoleCtx.cmdparse, tokens, FLUB_CONSOLE_MAX_TOKENS);

    return tokens;
}

void consoleCmdLineRender(conCmdLineCtx_t *ctx);
void consoleCmdLineNext(conCmdLineCtx_t *ctx);
char *consoleCmdLineGetStr(conCmdLineCtx_t *ctx);
void consoleCmdLineHistPrev(conCmdLineCtx_t *ctx);
void consoleCmdLineHistNext(conCmdLineCtx_t *ctx);
void consoleCmdLineCharPrev(conCmdLineCtx_t *ctx);
void consoleCmdLineCharNext(conCmdLineCtx_t *ctx);
void consoleCmdLineWordPrev(conCmdLineCtx_t *ctx);
void consoleCmdLineWordNext(conCmdLineCtx_t *ctx);
void consoleCmdLineInsertStr(conCmdLineCtx_t *ctx, const char *str);
void consoleCmdLineInsertChar(conCmdLineCtx_t *ctx, char c);
void consoleCmdLineDelete(conCmdLineCtx_t *ctx);
void consoleCmdLineBackspace(conCmdLineCtx_t *ctx);


#if 1
static void _consoleInputHandler(SDL_Event *event, int pressed, int motion, int x, int y) {
    char *ptr;
    int k;
    consoleCmdEntry_t *entry;
    char **tokens;
    int tcount;
    char *cmd;
    int updatelog = 0;

    switch(event->type) {
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
                case SDLK_BACKSPACE:
                    consoleCmdLineBackspace(_consoleCtx.cmdline);
                    break;
                case SDLK_RETURN:
                    cmd = consoleCmdLineGetStr(_consoleCtx.cmdline);
                    tokens = _consoleCmdParse(&tcount, cmd);
                    if(tcount > 0) {
                        if(critbitContains(&(_consoleCtx.handlers), tokens[0], ((void **)&entry))) {
                            entry->handler(tokens[0], tcount - 1, tokens + 1);
                        } else {
                            consolePrintf("Unknown command: %s", cmd);
                        }
                    }
                    consoleCmdLineNext(_consoleCtx.cmdline);
                    break;
                case SDLK_UP:
                    consoleCmdLineHistPrev(_consoleCtx.cmdline);
                    break;
                case SDLK_DOWN:
                    consoleCmdLineHistNext(_consoleCtx.cmdline);
                    break;
                case SDLK_RIGHT:
                    if(event->key.keysym.mod & KMOD_CTRL) {
                        consoleCmdLineWordNext(_consoleCtx.cmdline);
                    } else {
                        consoleCmdLineCharNext(_consoleCtx.cmdline);                        
                    }
                    break;
                case SDLK_LEFT:
                    if(event->key.keysym.mod & KMOD_CTRL) {
                        consoleCmdLineWordPrev(_consoleCtx.cmdline);
                    } else {
                        consoleCmdLineCharPrev(_consoleCtx.cmdline);
                    }
                    break;
                case SDLK_HOME:
                    // TODO - home
                    break;
                case SDLK_END:
                    // TODO - End
                    break;
                case SDLK_DELETE:
                    consoleCmdLineDelete(_consoleCtx.cmdline);
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
                        consoleCmdLineInsertChar(_consoleCtx.cmdline, k);
                    }
            }
    }

    consoleCmdLineRender(_consoleCtx.cmdline);
    //_consoleCmdMeshRebuild();
    if(updatelog) {
        _consoleMeshRebuild();
    }
}

#else

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

#endif

static void _consoleCmdMeshRebuild(void) {
	int x;
	int y;
    int len;
    int count;

	y = _consoleCtx.height - fontGetHeight(_consoleCtx.font);

	gfxMeshClear(_consoleCtx.cmdMesh);
	fontPos(0, y);
	fontSetColor(&(_consoleCtx.colorCursor));
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
	fontSetColor(&(_consoleCtx.colorCmdText));
	fontPos(fontGetCharWidth(_consoleCtx.cmdFont, '_'), y);
    if((_consoleCtx.cmdlen - _consoleCtx.offset) < _consoleCtx.charsPerLine) {
        count = _consoleCtx.cmdlen - _consoleCtx.offset;
    } else {
        count = _consoleCtx.charsPerLine;
    }
	fontBlitStrNMesh(_consoleCtx.cmdMesh, _consoleCtx.cmdFont, _consoleCtx.cmdbuf + _consoleCtx.offset, count);
}

int flubConsoleUpdate(Uint32 ticks, Uint32 elapsed2) {
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
        flubColorGLColorAlphaSet(&(_consoleCtx.colorBackground));
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
	pos = _consoleCtx.height - (fontGetHeight(_consoleCtx.font) * 2);
	for(line = count - 1; lines; line--) {
		if(!circBufGetEntry(_consoleCtx.circbuf, line - scroll, ((void **)&buf), NULL)) {
			continue;
		}
		fontPos(0, pos);
        fontSetColor((flubColor4f_t *)buf);
        buf += sizeof(flubColor4f_t);
		fontBlitQCStrMesh(_consoleCtx.mesh, _consoleCtx.font, buf, &(_consoleCtx.colorOutText));
		lines--;
		pos -= fontGetHeight(_consoleCtx.font);
	}
}

void consolePrint(const char *str) {
	char *entry;
	int pos;
    flubColor4f_t color, lineColor;

    flubColorCopy(&(_consoleCtx.colorOutText), &color);
	while(1) {
        flubColorCopy(&color, &lineColor);
		fontGetStrLenQCWrap(_consoleCtx.font, str, *videoWidth, &pos, &color, &(_consoleCtx.colorOutText));
		entry = circBufAllocNextEntryPtr(_consoleCtx.circbuf, pos + sizeof(lineColor) + 1);
        memcpy(entry, &lineColor, sizeof(lineColor));
        entry += sizeof(lineColor);
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




/*

static void _consoleCmdMeshRebuild(void) {
    int x;
    int y;
    int len;
    int count;

    y = _consoleCtx.height - fontGetHeight(_consoleCtx.font);

    gfxMeshClear(_consoleCtx.cmdMesh);
    fontPos(0, y);
    fontSetColor(&(_consoleCtx.colorCursor));
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
    fontSetColor(&(_consoleCtx.colorCmdText));
    fontPos(fontGetCharWidth(_consoleCtx.cmdFont, '_'), y);
    if((_consoleCtx.cmdlen - _consoleCtx.offset) < _consoleCtx.charsPerLine) {
        count = _consoleCtx.cmdlen - _consoleCtx.offset;
    } else {
        count = _consoleCtx.charsPerLine;
    }
    fontBlitStrNMesh(_consoleCtx.cmdMesh, _consoleCtx.cmdFont, _consoleCtx.cmdbuf + _consoleCtx.offset, count);
}

*/

conCmdLineCtx_t *consoleCmdLineInit(int lineLen, int historyLen) {
    conCmdLineCtx_t *ctx;
    char *ptr;
    int size;
    int k;

    size = (lineLen + 1 + sizeof(conCmdLineEntry_t)) * historyLen;
    size += sizeof(conCmdLineCtx_t);
    size += (sizeof(conCmdLineEntry_t *) * historyLen);

    ctx = util_calloc(size, 0, NULL);

    ctx->histCount = historyLen;
    ctx->histWalk = CON_HIST_WALK_NONE;
    ctx->history = ((void *)(ctx->buffer));
    ptr = ctx->buffer;
    ptr += (sizeof(conCmdLineEntry_t *) * historyLen);
    for(k = 0; k < historyLen; k++) {
        ctx->history[k] = ((void *)(ptr));
        infof("Base: %p  Entry %d: %p", ctx->buffer, k, ptr);
        ptr += (lineLen + 1 + sizeof(conCmdLineEntry_t));
        ctx->history[k]->buflen = lineLen - 1;
    }
    return ctx;
    
}

static conCmdLineEntry_t *_consoleCmdLineGetActive(conCmdLineCtx_t *ctx) {
    if(ctx->histWalk != CON_HIST_WALK_NONE) {
        return ctx->history[ctx->histWalk];
    }
    return ctx->history[ctx->active];
}

#define CONSOLE_CMDLINE_RESERVED_CHARS    3

void consoleCmdLineUpdateOffset(conCmdLineCtx_t *ctx) {
    conCmdLineEntry_t *entry;

    entry = _consoleCmdLineGetActive(ctx);
    if(entry->len < (_consoleCtx.charsPerLine - CONSOLE_CMDLINE_RESERVED_CHARS)) {
        ctx->offset = 0;
    } else {
        if(entry->pos < ctx->offset) {
            ctx->offset = entry->pos;
        } else {
            if((entry->pos - ctx->offset) > (_consoleCtx.charsPerLine - CONSOLE_CMDLINE_RESERVED_CHARS)) {
                ctx->offset = (entry->pos - (_consoleCtx.charsPerLine - CONSOLE_CMDLINE_RESERVED_CHARS));
            }
        }
    }
}

void consoleCmdLineWidthChange(conCmdLineCtx_t *ctx) {
    consoleCmdLineUpdateOffset(ctx);
}

void consoleCmdLineRender(conCmdLineCtx_t *ctx) {
    int x;
    int y;
    int len;
    int count;
    conCmdLineEntry_t *entry;

    y = _consoleCtx.height - fontGetHeight(_consoleCtx.font);

    gfxMeshClear(_consoleCtx.cmdMesh);
    fontPos(0, y);
    fontSetColor(&(_consoleCtx.colorCursor));
    if(ctx->offset) { // content is scrolled
        fontBlitCMesh(_consoleCtx.cmdMesh, _consoleCtx.cmdFont, '<');
        fontBlitCMesh(_consoleCtx.cmdMesh, _consoleCtx.cmdFont, '<');
    } else {
        fontBlitCMesh(_consoleCtx.cmdMesh, _consoleCtx.cmdFont, '>');       
        fontBlitCMesh(_consoleCtx.cmdMesh, _consoleCtx.cmdFont, '>');       
    }

    // Set a cursor
    entry = _consoleCmdLineGetActive(ctx);
    x = fontGetCharWidth(_consoleCtx.cmdFont, 'A') * (entry->pos - ctx->offset + 2);
    fontPos(x, y);
    fontBlitCMesh(_consoleCtx.cmdMesh, _consoleCtx.cmdFont, '_');

    // Render the visible command line portion
    fontSetColor(&(_consoleCtx.colorCmdText));
    fontPos((fontGetCharWidth(_consoleCtx.cmdFont, '_') * 2), y);
    if((entry->len - ctx->offset) < _consoleCtx.charsPerLine) {
        count = entry->len - ctx->offset;
    } else {
        count = _consoleCtx.charsPerLine;
    }
    fontBlitStrNMesh(_consoleCtx.cmdMesh, _consoleCtx.cmdFont, entry->buf + ctx->offset, count);
}

static conCmdLineEntry_t *_consoleCmdLineCheckHistory(conCmdLineCtx_t *ctx) {
    if(ctx->histWalk != CON_HIST_WALK_NONE) {
        memcpy(ctx->history[ctx->active]->buf, ctx->history[ctx->histWalk]->buf, ctx->history[ctx->active]->buflen);
        ctx->history[ctx->active]->pos = ctx->history[ctx->histWalk]->pos;
        ctx->history[ctx->active]->len = ctx->history[ctx->histWalk]->len;
        ctx->histWalk = CON_HIST_WALK_NONE;
    }
    return ctx->history[ctx->active];
}

void consoleCmdLineNext(conCmdLineCtx_t *ctx) {
    ctx->active++;
    if(ctx->active >= ctx->histCount) {
        ctx->active = 0;
    }
    ctx->history[ctx->active]->len = 0;
    ctx->history[ctx->active]->pos = 0;
    ctx->offset = 0;
}

void consoleCmdLineHistPrev(conCmdLineCtx_t *ctx) {
    if(ctx->histWalk == CON_HIST_WALK_NONE) {
        ctx->histWalk = ctx->active;
    }
    if(((ctx->histWalk - 1) == ctx->active) ||
        ((ctx->histWalk == 0) && (ctx->active == (ctx->histCount - 1)))) {
        return;
    }
    ctx->histWalk--;
    if(ctx->histWalk < 0) {
        ctx->histWalk = ctx->histCount - 1;
    }
    consoleCmdLineUpdateOffset(_consoleCtx.cmdline);
}

void consoleCmdLineHistNext(conCmdLineCtx_t *ctx) {
    if(ctx->histWalk != CON_HIST_WALK_NONE) {
        ctx->histWalk++;
        if(ctx->histWalk >= ctx->histCount) {
            ctx->histWalk = 0;
        }
        if(ctx->histWalk == ctx->active) {
            ctx->histWalk = CON_HIST_WALK_NONE;
        }
    }
    consoleCmdLineUpdateOffset(_consoleCtx.cmdline);
}

void consoleCmdLineCharPrev(conCmdLineCtx_t *ctx) {
    conCmdLineEntry_t *active = _consoleCmdLineGetActive(ctx);

    if(active->pos > 0) {
        active->pos--;
    }
    consoleCmdLineUpdateOffset(_consoleCtx.cmdline);
}

void consoleCmdLineCharNext(conCmdLineCtx_t *ctx) {
    conCmdLineEntry_t *active = _consoleCmdLineGetActive(ctx);

    if(active->pos < active->len) {
        active->pos++;
    }    
    consoleCmdLineUpdateOffset(_consoleCtx.cmdline);
}

void consoleCmdLineWordPrev(conCmdLineCtx_t *ctx) {
    conCmdLineEntry_t *active = _consoleCmdLineGetActive(ctx);

    if(isspace(active->buf[active->pos])) {
        for(; ((active->pos > 0) && (isspace(active->buf[active->pos]))); active->pos--);
    }
    for(; ((active->pos > 0) && (!isspace(active->buf[active->pos - 1]))); active->pos--);
    consoleCmdLineUpdateOffset(_consoleCtx.cmdline);
}

void consoleCmdLineWordNext(conCmdLineCtx_t *ctx) {
    conCmdLineEntry_t *active = _consoleCmdLineGetActive(ctx);

    if(isspace(active->buf[active->pos])) {
        for(; ((active->pos < active->len) && (isspace(active->buf[active->pos]))); active->pos++);
    } else {
        for(; ((active->pos < active->len) && (!isspace(active->buf[active->pos]))); active->pos++);
        for(; ((active->pos < active->len) && (isspace(active->buf[active->pos]))); active->pos++);
    }
    consoleCmdLineUpdateOffset(_consoleCtx.cmdline);
}

void consoleCmdLineInsertStr(conCmdLineCtx_t *ctx, const char *str) {
    int len;
    int slen;
    conCmdLineEntry_t *active = _consoleCmdLineGetActive(ctx);

    if(active->pos < active->buflen) {
        slen = strlen(str);
        active = _consoleCmdLineCheckHistory(ctx);
        if((active->pos + slen) > active->buflen) {
            slen = active->buflen - active->pos;
            active->len = active->buflen;
        } else {
            len = active->len - active->pos;
            if((active->pos + len + slen) > active->buflen) {
                len = active->buflen - active->pos - slen;
            }
            memmove(&(active->buf[active->pos + slen]), &(active->buf[active->pos]), len);
            active->len += slen;
            if(active->len >= active->buflen) {
                active->len = active->buflen;
            }
        }
        memmove(&(active->buf[active->pos]), str, slen);
        active->pos += slen;
    }
    consoleCmdLineUpdateOffset(_consoleCtx.cmdline);
}

void consoleCmdLineInsertChar(conCmdLineCtx_t *ctx, char c) {
    char str[2];

    str[0] = c;
    str[1] = '\0';
    consoleCmdLineInsertStr(ctx, str);
}

void consoleCmdLineDelete(conCmdLineCtx_t *ctx) {
    int len;
    conCmdLineEntry_t *active = _consoleCmdLineGetActive(ctx);

    if(active->pos < active->len) {
        active = _consoleCmdLineCheckHistory(ctx);
        len = active->len - active->pos - 1;
        if(len) {
            memmove(&(active->buf[active->pos]), &(active->buf[active->pos + 1]), len);
        }
        active->len--;
    }
    consoleCmdLineUpdateOffset(_consoleCtx.cmdline);
}

void consoleCmdLineBackspace(conCmdLineCtx_t *ctx) {
    conCmdLineEntry_t *active = _consoleCmdLineGetActive(ctx);

    if(active->pos > 0) {
        active = _consoleCmdLineCheckHistory(ctx);
        consoleCmdLineCharPrev(ctx);
        consoleCmdLineDelete(ctx);
        consoleCmdLineUpdateOffset(_consoleCtx.cmdline);
    }
}

char *consoleCmdLineGetStr(conCmdLineCtx_t *ctx) {
    conCmdLineEntry_t *entry;

    entry = _consoleCmdLineCheckHistory(ctx);
    return entry->buf;
}
