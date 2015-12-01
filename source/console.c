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
#include <flub/util/parse.h>
#include <flub/module.h>
#include <stddef.h>


#define FLUB_CONSOLE_VAR_BG_COLOR   "#333333"
#define FLUB_CONSOLE_VAR_BG_ALPHA   0.85
#define FLUB_CONSOLE_VAR_SPEED      150
#define FLUB_CONSOLE_VAR_HEIGHT     0.75


#define FLUB_CONSOLE_SHOW_DELAY		150
#define FLUB_CONSOLE_HEIGHT_RATIO	0.75
#define FLUB_CONSOLE_BG_COLOR       "#333333"
#define FLUB_CONSOLE_BG_ALPHA       0.85

#define CONSOLE_MISC_BUF_SIZE       256
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
    float heightRatio;

    int speed;
	flubAnim1Pti_t slideAnim;
	Uint32 lastTick;

    int capture;
    critbit_t handlers;

    char cmdparse[FLUB_CONSOLE_CMD_LEN];
    char shiftMap[127];

    char miscbuf[CONSOLE_MISC_BUF_SIZE];
    int misclen;

	font_t *font;
	font_t *cmdFont;
	circularBuffer_t *circbuf;
	gfxMeshObj_t *mesh;
	gfxMeshObj_t *cmdMesh;
    texture_t *bgImage;

    sound_t *openSound;
    sound_t *closeSound;

    texture_t *bgTexture;
    float bgAlpha;
    flubColor4f_t colorBackground;
    flubColor4f_t colorCursor;
    flubColor4f_t colorDimCursor;
    flubColor4f_t colorCmdText;
    flubColor4f_t colorOutText;
} _consoleCtx = {
        .handlers = CRITBIT_TREE_INIT,
        .heightRatio = FLUB_CONSOLE_VAR_HEIGHT,
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

static void _consoleUpdateDisplaySettings(int width, int height);

static int _consoleVarValidateBgColor(const char *name, const char *value) {
    flubColor4f_t color;

    if(flubColorParse(value, &color)) {
        color.alpha = _consoleCtx.bgAlpha;
        flubColorCopy(&color, &(_consoleCtx.colorBackground));
        _consoleUpdateDisplaySettings(*videoWidth, *videoHeight);
        return 1;
    }

    return 0;
}

static int _consoleVarValidateBgAlpha(const char *name, const char *value) {
    float alpha;

    alpha = utilFloatFromString(value, -1.0);
    if((alpha < 0.0) || (alpha > 1.0)) {
        return 0;
    }
    _consoleCtx.bgAlpha = alpha;
    _consoleCtx.colorBackground.alpha = alpha;
    _consoleUpdateDisplaySettings(*videoWidth, *videoHeight);

    return 1;
}

static int _consoleVarValidateSpeed(const char *name, const char *value) {
    int speed;

    speed = utilIntFromString(value, -1);
    if((speed < 50) || (speed > 10000)) {
        return 0;
    }
    _consoleCtx.speed = speed;
    flubAnimInit1Pti(&(_consoleCtx.slideAnim), 0, _consoleCtx.height, speed);
    if(!_consoleCtx.visible) {
        flubAnimReverse1Pti(&(_consoleCtx.slideAnim));
    }
    _consoleUpdateDisplaySettings(*videoWidth, *videoHeight);

    return 1;
}

static int _consoleVarValidateHeight(const char *name, const char *value) {
    float ratio;

    ratio = utilFloatFromString(value, -1.0);
    if(!((ratio < 0.10) || (ratio > 1.0))) {
        _consoleCtx.heightRatio = ratio;
        _consoleUpdateDisplaySettings(*videoWidth, *videoHeight);
        return 1;
    }

    return 0;
}

flubCfgOptList_t _consoleVars[] = {
    {"console_bg", FLUB_CONSOLE_VAR_BG_COLOR, FLUB_CFG_FLAG_CLIENT, _consoleVarValidateBgColor, _consoleVarValidateBgColor},
    {"console_alpha", STRINGIFY(FLUB_CONSOLE_VAR_BG_ALPHA), FLUB_CFG_FLAG_CLIENT, _consoleVarValidateBgAlpha, _consoleVarValidateBgAlpha},
    {"console_speed", STRINGIFY(FLUB_CONSOLE_VAR_SPEED), FLUB_CFG_FLAG_CLIENT, _consoleVarValidateSpeed, _consoleVarValidateSpeed},
    {"console_height", STRINGIFY(FLUB_CONSOLE_VAR_HEIGHT), FLUB_CFG_FLAG_CLIENT, _consoleVarValidateHeight, _consoleVarValidateHeight},
    FLUB_CFG_OPT_LIST_END
};

void _consoleLogCB( const logMessage_t *msg ) {
    static const char *_qcCode = "57311";
	consolePrintf("^%c%s^=: %s", _qcCode[msg->level], msg->levelStr, msg->message);
}

static void _consoleMiscBufClear(void) {
    _consoleCtx.miscbuf[0] = '\0';
    _consoleCtx.misclen = 0;
}

static void _consoleMiscBufStrncat(const char *str, int len, int pad) {
    char *ptr = _consoleCtx.miscbuf + _consoleCtx.misclen;
    int *pos = &(_consoleCtx.misclen);

    while((len) && (*str != '\0') && (_consoleCtx.misclen < sizeof(_consoleCtx.miscbuf))) {
        *ptr = *str;
        ptr++;
        str++;
        *pos += 1;
        len--;
    }
    if(pad) {
        while((len) && (_consoleCtx.misclen < sizeof(_consoleCtx.miscbuf))) {
            *ptr = ' ';
            ptr++;
            *pos += 1;
            len--;
        }
    }
    *ptr = '\0';
}

void consoleTabularDivider(int border, int columns, int *colWidths) {
    int k, j;
    char *ptr;

    _consoleMiscBufClear();
    if(columns == 0) {
        return;
    }
    ptr = _consoleCtx.miscbuf;
    memcpy(ptr, "^7", 3);
    ptr+= 2;

    for(k = 0; k < columns; k++) {
        if(border) {
            *ptr = '+';
            ptr++;
        } else {
            if(k != 0) {
                *ptr = ' ';
                ptr++;
            }
        }
        for(j = 0; j < colWidths[k]; j++) {
            *ptr = '=';
            ptr++;
        }
    }
    if(border) {
        *ptr = '+';
        ptr++;
    }
    *ptr ='\0';
    consolePrint(_consoleCtx.miscbuf);
}

void consoleTabularPrint(int border, int columns, int *colWidths, const char * const * const colStrs) {
    int k, j;
    char *ptr;

    _consoleMiscBufClear();
    if(columns == 0) {
        return;
    }
    ptr = _consoleCtx.miscbuf;
    for(k = 0; k < columns; k++) {
        if(border) {
            memcpy(ptr, "^7|^=", 5);
            ptr += 5;
            _consoleCtx.misclen += 5;
        } else {
            if(k != 0) {
                *ptr = ' ';
                ptr++;
                _consoleCtx.misclen++;
            }
        }
        *ptr = '\0';
        _consoleMiscBufStrncat(colStrs[k], colWidths[k], 1);
        ptr += colWidths[k];
    }
    if(border) {
        memcpy(ptr, "^7|^=", 5);
        ptr += 5;
        _consoleCtx.misclen += 5;
    }
    *ptr = '\0';
    consolePrint(_consoleCtx.miscbuf);
}

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

static void _consoleUpdateDisplaySettings(int width, int height) {
    _consoleCtx.height = ((float)height) * _consoleCtx.heightRatio;
    _consoleCtx.width = width;
    _consoleCtx.lines = _consoleCtx.height / fontGetHeight(_consoleCtx.font);
    flubAnimInit1Pti(&(_consoleCtx.slideAnim), 0, _consoleCtx.height, _consoleCtx.speed);
    if(!_consoleCtx.visible) {
        flubAnimReverse1Pti(&(_consoleCtx.slideAnim));
    }
    _consoleCtx.charsPerLine = _consoleCtx.width / fontGetCharWidth(_consoleCtx.font, 'A');
    _consoleCtx.cmdCharsPerLine = _consoleCtx.width / fontGetCharWidth(_consoleCtx.cmdFont, 'A');
    _consoleCtx.scrollback = 0;
    consoleCmdLineWidthChange(_consoleCtx.cmdline);
}

static void _consoleVideoCallback(int width, int height, int fullscreen) {
    _consoleUpdateDisplaySettings(width, height);
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
    flubCfgVarUpdateInList(_consoleVars, name, value);
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
static void _consoleHelp(const char *name, int paramc, char **paramv);
static void _consoleList(const char *name, int paramc, char **paramv);
static void _consoleSet(const char *name, int paramc, char **paramv);
static void _consoleBind(const char *name, int paramc, char **paramv);
static void _consoleClear(const char *name, int paramc, char **paramv);
static void _consoleClose(const char *name, int paramc, char **paramv);
static void _consoleVersion(const char *name, int paramc, char **paramv);

consoleCmd_t _consoleCmdList[] = {
    {"help", "help [prefix]", _consoleHelp},
    {"list", "list [prefix]", _consoleList},
    {"bind", "bind <key> <action>", _consoleBind},
    {"clear", "clear", _consoleClear},
    {"close", "close", _consoleClose},
    {"version", "version", _consoleVersion},
    {"set", "set <var> <value>", _consoleSet},
    CONSOLE_COMMAND_LIST_END(),
};

void consoleCmdLineRender(conCmdLineCtx_t *ctx);

int flubConsoleStart(void) {
    //_consoleCtx.cmdbuf[0] = '\0';
    _consoleShiftMapInit();

    if((_consoleCtx.circbuf = circBufInit(FLUB_CONSOLE_BUF_SIZE, FLUB_CONSOLE_BUF_ENTRIES, 1)) == NULL) {
        return 0;
    }

    if(!consoleCmdListRegister(_consoleCmdList)) {
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

    flubCfgVarUpdateAllInList(_consoleVars);

    _consoleVideoCallback(*videoWidth, *videoHeight, videoIsFullscreen());

    consoleCmdLineRender(_consoleCtx.cmdline);

    logAddNotifiee(_consoleLogCB);

    return 1;
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
    if(updatelog) {
        _consoleMeshRebuild();
    }
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

int consoleCmdRegister(const char *name, const char *help, consoleCmdHandler_t handler) {
    consoleCmdEntry_t *entry;

    entry = util_calloc(sizeof(consoleCmdEntry_t), 0, NULL);

    entry->help = util_strdup(help);
    entry->handler = handler;

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
        if(!consoleCmdRegister(list[k].name, list[k].help, list[k].handler)) {
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

#define CONSOLE_MAX_PRINT_COLS      10
#define CONSOLE_COLUMNS_SPACING     3

typedef struct conOutColumnator_s {
    int maxlen;
    int border;
    int pos;
    int columns;
    int colWidths[CONSOLE_MAX_PRINT_COLS];
    const char *colStrs[CONSOLE_MAX_PRINT_COLS];
} conOutColumnator_t;

void consoleColumnatorInit(conOutColumnator_t *col, int border);
void consoleColumnatorCheckSize(conOutColumnator_t *col, int width);
void consoleColumnatorBegin(conOutColumnator_t *col);
void consoleColumnatorAdd(conOutColumnator_t *col, const char *str);
void consoleColumnatorEnd(conOutColumnator_t *col);

void consoleColumnatorInit(conOutColumnator_t *col, int border) {
    col->maxlen = 0;
    col->border = border;
    col->pos = 0;
    col->columns = 0;
}

void consoleColumnatorCheckSize(conOutColumnator_t *col, int width) {
    if(width > col->maxlen) {
        col->maxlen = width;
    }
}

void consoleColumnatorBegin(conOutColumnator_t *col) {
    int k;
    int spacing;

    if(!(col->border)) {
        spacing = CONSOLE_COLUMNS_SPACING;
    } else {
        spacing = 0;
    }

    for(col->columns = CONSOLE_MAX_PRINT_COLS; col->columns > 0; col->columns--) {
        if(((col->columns * col->maxlen) + ((col->columns - 1) * spacing)) < _consoleCtx.charsPerLine) {
            break;
        }
    }
    for(k = 0; k < col->columns; k++) {
        col->colWidths[k] = col->maxlen;
    }
    col->pos = 0;
}

void consoleColumnatorAdd(conOutColumnator_t *col, const char *str) {
    col->colStrs[col->pos] = str;
    col->pos++;
    if(col->pos >= col->columns) {
        consoleTabularPrint(col->border, col->columns, col->colWidths, col->colStrs);
        col->pos = 0;
    }
}

void consoleColumnatorEnd(conOutColumnator_t *col) {
    if(col->pos > 0) {
        consoleTabularPrint(col->border, col->pos, col->colWidths, col->colStrs);
        col->pos = 0;        
    }
}

static int _consoleCmdEnumCB(const char *name, void *data, void *arg) {
    conOutColumnator_t *col = ((conOutColumnator_t *)arg);
    consoleColumnatorAdd(col, name);
    return 1;
}

static int _consoleCmdLenEnumCB(const char *name, void *data, void *arg) {
    conOutColumnator_t *col = ((conOutColumnator_t *)arg);
    consoleColumnatorCheckSize(col, strlen(name));
    return 1;
}

static void _consoleHelp(const char *name, int paramc, char **paramv) {
    char *prefix = "";
    conOutColumnator_t col;

    consoleColumnatorInit(&col, 0);

    if(paramc > 0) {
        prefix = paramv[0];
    }

    consolePrint("^7Console commands:^=");

    critbitAllPrefixed(&(_consoleCtx.handlers), prefix, _consoleCmdLenEnumCB, (void *)(&col));
    consoleColumnatorBegin(&col);
    critbitAllPrefixed(&(_consoleCtx.handlers), prefix, _consoleCmdEnumCB, (void *)(&col));
    consoleColumnatorEnd(&col);
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
    consolePrintf("^7%s^= version ^5%d^=.^5%d^=", appDefaults.title, appDefaults.major, appDefaults.minor);
    consolePrintf("^7Flub^= version ^5%s^=", FLUB_VERSION_STRING);
}

void consoleBgImageSet(texture_t *tex) {
    _consoleCtx.bgImage = tex;
}
