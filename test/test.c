#include <flub/flub.h>
#include <ctype.h>
#include <unistd.h>
#include <flub/physfsutil.h>
#include <physfs.h>
#include <flub/theme.h>
#include <flub/memory.h>


eCmdLineStatus_t cmdlineHandler(const char *arg, void *context) {
    warningf("Ignoring command line option \"%s\"", arg);
    return eCMDLINE_OK;
}

appDefaults_t appDefaults = {
        .title = "FlubTest",
        .configFile = "test.cfg",
        .bindingFile = "flubtest.bind",
        //.archiveFile = "FlubTest.zip",
        .videoMode = "640x480",
        .fullscreen = "false",
        .allowVideoModeChange = 1,
        .allowFullscreenChange = 1,
        .cmdlineHandler = cmdlineHandler,
        .cmdlineParamStr = "[param]",
        .resources = NULL,
        .frameStackSize = 1048576,
};

void physfs_log(const char *msg) {
    infof("PHYSFS: %s", msg);
}

font_t *fpsFont;

void doFpsCounter(Uint32 elapsed) {
    static int frames = 0;
    static Uint32 last = 0;
    static int fps = 0;

    frames++;
    last += elapsed;
    if(last >= 1000) {
        fps = frames;
        frames = 0;
        last -= 1000;
    }

    fontPos(620, 5);
    glColor3f(0.5, 0.5, 1.0);
    fontBlitInt(fpsFont, fps);
}

void blitSpriteMap(flubSprite_t *sprite, int x, int y, int *dict, char **map, int layers) {
    int layer;
    int idx;
    int pos;
    int workX;
    int workY;

    for(layer = 0; layer < layers; layer++) {
        workX = x;
        workY = y;
        for(pos = 0; map[layer][pos] != '\0'; pos++) {
            if(map[layer][pos] == ' ') {
                workX += sprite->width;
            } else if(map[layer][pos] == '\n') {
                workX = x;
                workY += sprite->height;
            } else {
                idx = dict[map[layer][pos]];
                gfxSpriteBlit(sprite, idx, workX, workY);
                workX += sprite->width;
            }
        }
    }
}

void blitMeshSpriteMap(gfxMeshObj_t *mesh, flubSprite_t *sprite, int x, int y, int *dict, char **map, int layers) {
    int layer;
    int idx;
    int pos;
    int workX;
    int workY;

    for(layer = 0; layer < layers; layer++) {
        workX = x;
        workY = y;
        for(pos = 0; map[layer][pos] != '\0'; pos++) {
            if(map[layer][pos] == ' ') {
                workX += sprite->width;
            } else if(map[layer][pos] == '\n') {
                workX = x;
                workY += sprite->height;
            } else {
                idx = dict[map[layer][pos]];
                gfxSpriteMeshBlit(mesh, sprite, idx, workX, workY);
                workX += sprite->width;
            }
        }
    }
}

texture_t *snapAndRescale(int w, int h) {
    int size;
    texture_t *tex;
    unsigned char *data;

    data = util_alloc((w * h * 3), NULL);
    videoScreenCapture(data, w, h);
    tex = texmgrCreate(NULL, GL_NEAREST, GL_NEAREST, 0, 0, 0, 0, 3, GL_RGB, data, w, h);
    return tex;
}

extern void (*log_message)(const char *msg);

int main(int argc, char *argv[]) {
    eCmdLineStatus_t status;
    int keepGoing = 1;
    Uint32 current;
    Uint32 elapsed;
    Uint32 wait;
    SDL_Event ev;

    int iw, ih, fw, fh;
    texture_t *dlg;
    texture_t *misc;
    flubSlice_t *slice;

    font_t *fnt;
    int pos;
    int w;
    gfxMeshObj_t *mesh;
    gfxMeshObj_t *fontMesh;
    sound_t *sound;
    char cwdbuf[1024];
    font_t *pfont;
    texture_t *scaled = NULL;

    texture_t *tex_misc;
    texture_t *tex_dlg;
    texture_t *tex_tiles;
    flubSlice_t *dlg_title;
    flubSlice_t *dlg_body;
    flubSlice_t *dlg_tile;
    gfxMeshObj_t *meshFont;
    gfxMeshObj_t *meshChain;
    gfxMeshObj_t *meshDlg;
    gfxMeshObj_t *meshTiles;
    gfxMeshObj_t *meshMisc;
    //gfxEffect_t *effect;

    flubSlice_t *healthBar;
    flubSlice_t *expBar;
    texture_t *rawSlices;

    int tileMap[127];
    char *scene[3] = {
            "#######\n#UIIIO#\n#JKKKL#\n#WWWWW#\n#######\n#######\n#######",
            "       \n       \n       \n  w P  \n    F  \nCCCCVCC\n       ",
            "       \n       \n       \n    D  \n       \n       \n       ",
    };

    tileMap['#'] = 0;
    tileMap['U'] = 23;
    tileMap['I'] = 7;
    tileMap['O'] = 24;
    tileMap['J'] = 21;
    tileMap['K'] = 5;
    tileMap['L'] = 22;
    tileMap['W'] = 16;
    tileMap['w'] = 32;
    tileMap['P'] = 36;
    tileMap['D'] = 37;
    tileMap['F'] = 33;
    tileMap['C'] = 34;
    tileMap['V'] = 19;

    flubSprite_t *sprites;

    //log_message = physfs_log;

    if(!appInit(argc, argv)) {
        return 1;
    }

    // Register command line params and config vars

    status = appStart(NULL);
    if(status != eCMDLINE_OK) {
        return ((status == eCMDLINE_EXIT_SUCCESS) ? 0 : 1);
    }

    fnt = fontGet("consolas", 12, 0);
    if(fnt == NULL) {
        infof("Unable to get font");
        return 1;
    }
    fpsFont = fnt;

    misc = texmgrGet("flub-keycap-misc");
    dlg = texmgrLoad( "work/dungeondlg2.gif", "dungeondlg2", GL_NEAREST, GL_NEAREST, 1, 255, 0, 255);

    slice = gfxSliceCreate(dlg, GFX_SLICE_NOTILE_ALL, 0, 0, 18, 22, 74, 69, 126, 106);

    rawSlices = texmgrGet("flub-slice-test");
    if(rawSlices == NULL) {
        fatal("Failed to load slices");
        return 1;
    }

    healthBar = gfx3x1SliceCreate(rawSlices, GFX_SLICE_NOTILE_ALL, 0, 106, 28, 130, 34, 42);
    info("== Expbar =========");
    expBar = gfx1x3SliceCreate(rawSlices, GFX_SLICE_NOTILE_ALL, 44, 106, 60, 128, 137, 152);
    info("== Done expbar ====");
    dlg_tile = gfxSliceCreate(rawSlices, GFX_SLICE_NOTILE_LEFT|GFX_SLICE_NOTILE_TOP|GFX_SLICE_NOTILE_RIGHT|GFX_SLICE_NOTILE_BOTTOM, 145, 36, 150, 41, 182, 57, 186, 61);

    sound = audioSoundGet("resources/sounds/menumove.wav");

    mesh = gfxMeshCreate(MESH_QUAD_SIZE(40), GL_TRIANGLES, 1, misc);
    gfxTexMeshBlit(mesh, misc, 20, 20);

    fontMesh = gfxMeshCreate(MESH_QUAD_SIZE(256), GL_TRIANGLES, 1, NULL);
    gfxMeshTextureAssign(fontMesh, fontTextureGet(fnt));

    fontPos(150, 240);
    fontBlitCMesh(fontMesh, fnt, 'F');
    fontBlitCMesh(fontMesh, fnt, 'o');
    fontBlitCMesh(fontMesh, fnt, 'o');
    fontPos(150, 260);
    fontBlitStrMesh(fontMesh, fnt, "fontBlitStrMesh();");
    fontPos(150, 280);
    fontBlitStrNMesh(fontMesh, fnt, "fontBlitStrNMesh(); is too long", 19);
    fontPos(150, 300);
    fontBlitStrfMesh(fontMesh, fnt, "font%sStrf();", "Blit");
    fontPos(150, 320);
    fontBlitQCStrMesh(fontMesh, fnt, "font^2Blit^1QC^wStr();", NULL);
    fontPos(150, 340);
    fontBlitIntMesh(fontMesh, fnt, 12345);
    fontPos(150, 360);
    fontBlitFloatMesh(fontMesh, fnt, 12.345, 3);

    // Blitter test resources
    tex_misc = texmgrGet("flub-keycap-misc");
    tex_dlg = texmgrGet("dungeondlg2");
    tex_tiles = texmgrLoad( "work/tiletest.gif", "testtiles", GL_NEAREST, GL_NEAREST, 1, 255, 0, 255);
    if(tex_tiles == NULL) {
        fatal("Failed to load the tiles");
        return 1;
    } else {
        infof("Tileset loaded: %dx%d", tex_tiles->width, tex_tiles->height);
    }

    sprites = gfxSpriteCreate(tex_tiles, 16, 16);
    infof("Sprite set: %d - %dx%d, %d per row", sprites->texture->id,
          sprites->width, sprites->height, sprites->spritesPerRow);

    dlg_title = gfxSliceCreate(tex_misc, GFX_SLICE_NOTILE_ALL, 41, 17, 43, 19, 60, 26, 62, 28);
    dlg_body = gfxSliceCreate(tex_misc, GFX_SLICE_NOTILE_ALL, 41, 29, 43, 30, 60, 40, 62, 42);

    // Create meshes for font, flubmisc, and flubsimplegui
    meshFont = gfxMeshCreate(MESH_QUAD_SIZE(100), GL_TRIANGLES, 1, fontTextureGet(fnt));
    meshDlg = gfxMeshCreate(MESH_QUAD_SIZE(200), GL_TRIANGLES, 1, tex_dlg);
    meshTiles = gfxMeshCreate(MESH_QUAD_SIZE(400), GL_TRIANGLES, 1, tex_tiles);
    meshMisc = gfxMeshCreate(MESH_QUAD_SIZE(400), GL_TRIANGLES, 1, tex_misc);

    meshChain = meshFont;
    gfxMeshAppendToChain(meshChain, meshMisc);
    gfxMeshAppendToChain(meshChain, meshDlg);
    gfxMeshAppendToChain(meshChain, meshTiles);

    // Excercise mesh blitters
    gfxTexMeshBlit(meshChain, tex_misc, 400, 5);
    gfxTexMeshBlitSub(meshChain, tex_dlg, 145, 6, 186, 31, 400, 200, 450, 250);
    gfxTexMeshTile(meshChain, tex_dlg, 150, 11, 180, 25, 400, 260, 500, 400);
    blitMeshSpriteMap(meshChain, sprites, 200, 20, tileMap, scene, 3);
    gfxSpriteMeshBlitResize(meshChain, sprites, 36, 0, 0, 63, 63);


    gfxSliceMeshBlit(meshChain, dlg_title, 200, 300, 320, 315);
    gfxSliceMeshBlit(meshChain, dlg_body, 200, 316, 320, 440);
    gfxKeycapMeshBlit(meshChain, fnt, "META_WINDOWS", 200, 260, NULL, NULL);
    gfxKeycapMeshBlit(meshChain, fnt, "Ctrl", 240, 260, NULL, NULL);
    gfxKeycapMeshBlit(meshChain, fnt, "PgDn", 280, 260, NULL, NULL);


    inputActionBind("KEY_BACKQUOTE", "showconsole");

    info("### Loading theme ###############");
    flubGuiThemeLoad("assets/data/flub-basic.theme");
    info("### Done loading theme ##########");

    current = flubTicksRefresh(&elapsed);
    while(appUpdate(current, elapsed)) {
        current = flubTicksRefresh(&elapsed);

        // Process every event
        // wait no more than 15ms to avoid exceeding 67 fps
        //while (SDL_WaitEventTimeout(&ev, 15)) {
        wait = 15;
        while(inputPollEvent(&ev)) {
            switch (ev.type) {
                case SDL_QUIT:
                    appQuit();
                    break;
                case SDL_TEXTINPUT:
                    break;
                case SDL_KEYUP:
                    switch (ev.key.keysym.sym) {
                        case SDLK_HOME:
                            break;
                        case SDLK_END:
                            break;
                        case SDLK_PAGEUP:
                            break;
                        case SDLK_PAGEDOWN:
                            break;
                        case SDLK_UP:
                        case SDLK_e:
                            break;
                        case SDLK_DOWN:
                        case SDLK_d:
                            break;
                        case SDLK_w:
                            break;
                        case SDLK_r:
                            break;
                        case SDLK_LEFT:
                        case SDLK_s:
                            break;
                        case SDLK_RIGHT:
                        case SDLK_f:
                            break;
                        case SDLK_ESCAPE:
                            appQuit();
                            break;
                        case SDLK_BACKQUOTE:
                            break;
    					case SDLK_RETURN:
    						break;
                    }
                    break;
                case SDL_KEYDOWN:
                    switch (ev.key.keysym.sym) {
                        case SDLK_HOME:
                            videoScreenshot("screenshot");
                            break;
                        case SDLK_END:
                            break;
                        case SDLK_PAGEUP:
                            break;
                        case SDLK_PAGEDOWN:
                            break;
                        case SDLK_UP:
                        case SDLK_e:
                            break;
                        case SDLK_DOWN:
                        case SDLK_d:
                            break;
                        case SDLK_w:
                            break;
                        case SDLK_r:
                            break;
                        case SDLK_LEFT:
                        case SDLK_s:
                            break;
                        case SDLK_RIGHT:
                        case SDLK_f:
                            break;
                        case SDLK_t:
                            if(scaled != NULL) {
                                texmgrRelease(scaled);
                            }
                            scaled = snapAndRescale(320,200);
                            break;
                        case SDLK_p:
                            videoScreenshot("screenshot");
                            break;
                        case SDLK_ESCAPE:
                            keepGoing = 0;
                            break;
                    }
                    break;
                case SDL_CONTROLLERBUTTONDOWN:
                    break;
                case SDL_CONTROLLERBUTTONUP:
                    break;
                case SDL_CONTROLLERAXISMOTION:
                    break;
            }
        }
        videoClear();
        videoPushGLState();
        videoOrthoMode();
        glLoadIdentity();
        glColor3f(1.0, 1.0, 1.0);

        gfxMeshRender(meshChain);

        gfxTexBlit(dlg, 0, 0);

        gfxSliceBlit(dlg_tile, 100, 150, 300, 280);
        gfxSliceBlit(slice, 400, 200, 410, 280);
        gfxSliceBlit(slice, 50, 300, 320, 450);
        gfxSliceBlit(healthBar, 10, *videoHeight - healthBar->height - 10, 300, *videoHeight - 10);
        gfxSliceBlit(expBar, 10, *videoWidth - expBar->width - 10, *videoHeight - 10, *videoWidth - 10);
        gfxSliceBlit(expBar, 10, 10, 50, 200);

        if(scaled != NULL) {
            gfxTexBlit(scaled, 10, 10);
        }
        doFpsCounter(elapsed);
    }

    gfxMeshDestroy(mesh);

    appShutdown();

    return 0;
}

