#include <flub/flub.h>
#include <ctype.h>
#include <unistd.h>
#include <flub/physfsutil.h>
#include <physfs.h>
#include <flub/theme.h>


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
};

void physfs_log(const char *msg) {
    infof("PHYSFS: %s", msg);
}

font_t *fpsFont;

void doFpsCounter( void )
{
    static Uint32 last = 0;
    static int frames = 0;
    static int fps = 0;
    Uint32 now;

    if(last == 0) {
        last = SDL_GetTicks();
    }
    now = SDL_GetTicks();

    frames++;

    if((now - last) >= 1000) {
        fps = frames;
        frames = 0;
        last += 1000;
    }

    fontPos(620, 5);
    glColor3f(0.5, 0.5, 1.0);
    fontBlitInt(fpsFont, fps);
}

#define DUMP_LIMIT 40

void dumpImg(const unsigned char *data, int width, int height) {
    char buf[DUMP_LIMIT + 1];
    int x, y;
    unsigned char c;

    infof("Dumping image %dx%d", width, height);
    for(y = 0; ((y < DUMP_LIMIT) && (y < height)); y++) {
        for(x = 0; ((x < DUMP_LIMIT) && (x < width)); x++) {
            c = data[(y * width) + x];
            buf[x] = (isalnum(c) ? c : '-');
        }
        buf[x] = '\0';
        infof("%.2d [%s]", y + 1, buf);
    }
}

int BULK_EVENT_LIMIT = 15;

GLuint testVBOid;
GLuint testTexid;
GLuint testIBOid;
GLuint testTex;
const texture_t *footex;
//GLint vertices[] = {30, 30, 130, 30, 130, 130, 30, 130};
GLint vertices[] = {30, 30, 130, 30, 130, 130, 30, 30, 130, 130, 30, 130};
GLfloat texcoords[] = {0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 1.0};
//GLint vertices[] = {40, 40, 0, 140, 40, 0, 140, 140, 0, 40, 40, 0, 140, 140, 0, 40, 140, 0};
//float vertices[] = {30, 30, 0, 130, 30, 0, 130, 130, 0, 30, 130, 0};
GLuint indices[] = {1, 2, 4, 2, 3, 4};

void vboTestInit(GLuint id) {
    testTex = id;
    glGenBuffers(1, &testVBOid);
    glBindBuffer(GL_ARRAY_BUFFER, testVBOid);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGenBuffers(1, &testTexid);
    glBindBuffer(GL_ARRAY_BUFFER, testTexid);
    glBufferData(GL_ARRAY_BUFFER, sizeof(texcoords), texcoords, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    /*
    glGenBuffers(1, &testIBOid);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, testIBOid);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_DYNAMIC_DRAW);
    */
}

void vboTestRender(void) {
    int k;
    glLoadIdentity();
    glColor3f(1.0, 1.0, 1.0);

    //gfxTexBlit(footex, 500, 300);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_BLEND);
    //glEnable(GL_CULL_FACE);
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBlendFunc(GL_ONE, GL_ZERO);

/*
	glBegin( GL_TRIANGLES );
	for(k = 0; k < 9; k += 3) {
		glVertex3i(vertices[k], vertices[k+ 1] + 120, vertices[k+ 2]);
	}
	glEnd();

	glBegin( GL_TRIANGLES );
	for(k = 9; k < 18; k += 3) {
		glVertex3i(vertices[k], vertices[k+ 1] + 120, vertices[k+ 2]);
	}
	glEnd();
*/

/*
	glBegin( GL_QUADS );
		glVertex2i(140, 30);
		glVertex2i(240, 30);
		glVertex2i(240, 130);
		glVertex2i(140, 130);
	glEnd();
*/


    glEnableClientState(GL_VERTEX_ARRAY);
    glBindBuffer(GL_ARRAY_BUFFER, testVBOid);
    glVertexPointer(2, GL_INT, 0, (void*)(0));

    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glBindBuffer(GL_ARRAY_BUFFER, testTexid);
    glTexCoordPointer(2, GL_FLOAT, 0, (void*)(0));


    //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, testIBOid);
    //glDrawElements(GL_TRIANGLES, 2, GL_UNSIGNED_INT, NULL);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glDisableClientState(GL_VERTEX_ARRAY);

    glFlush();

    gfxTexBlit(footex, 500, 150);
}

int foolio_1(void);
int foolio_2(void);

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

void blitMeshSpriteMap(gfxMeshObj2_t *mesh, flubSprite_t *sprite, int x, int y, int *dict, char **map, int layers) {
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
                gfxSpriteMeshBlit2(mesh, sprite, idx, workX, workY);
                workX += sprite->width;
            }
        }
    }
}

gfxMeshObj2_t *effectMesh;
int effectStart;
int effectStop;

void fadeinCompleted(gfxEffect_t *oldEffect, void *context);


void fadeoutCompleted(gfxEffect_t *oldEffect, void *context) {
    gfxEffect_t *effect;

    effect = gfxEffectFade(effectMesh, effectStart, effectStop, 0.0, 1.0, 2000);
    effect->completed = fadeinCompleted;
    gfxEffectRegister(effect);
}

void fadeinCompleted(gfxEffect_t *oldEffect, void *context) {
    gfxEffect_t *effect;

    effect = gfxEffectFade(effectMesh, effectStart, effectStop, 1.0, 0.0, 500);
    effect->completed = fadeoutCompleted;
    gfxEffectRegister(effect);
}

extern void (*log_message)(const char *msg);

int main(int argc, char *argv[]) {
    eCmdLineStatus_t status;
    int keepGoing = 1;
    Uint32 lastTick;
    Uint32 current;
    Uint32 elapsed;
    Uint32 counter = 0;
    //GLuint img, fid;
    int iw, ih, fw, fh;
    texture_t *dlg;
    texture_t *misc;
    flubSlice_t *slice;
    //flubSlice_t *sliceTest;
    font_t *fnt;
    int eventCount;
    int pos;
    int w;
    gfxMeshObj2_t *mesh;
    gfxMeshObj2_t *fontMesh;
    sound_t *sound;
    char cwdbuf[1024];
    font_t *pfont;

    texture_t *tex_misc;
    texture_t *tex_dlg;
    texture_t *tex_tiles;
    flubSlice_t *dlg_title;
    flubSlice_t *dlg_body;
    gfxMeshObj2_t *meshFont;
    gfxMeshObj2_t *meshChain;
    gfxMeshObj2_t *meshDlg;
    gfxMeshObj2_t *meshTiles;
    gfxMeshObj2_t *meshMisc;
    gfxEffect_t *effect;

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

#if 0
    infof("### Adding font ###################################");
    if(!flubFontLoad("flub/font/times.12.stbfont")) {
        errorf("Unable to load times font.");
    }
    if(!flubFontLoad("flub/font/courier.12.stbfont")) {
        errorf("Unable to load courier font.");
    }
#endif

    status = appStart(NULL);
    if(status != eCMDLINE_OK) {
        return ((status == eCMDLINE_EXIT_SUCCESS) ? 0 : 1);
    }

    //infof("Working dir: [%s]", getcwd(cwdbuf, sizeof(cwdbuf)));


    //enumDir("");
    //enumDir("assets");

    fnt = fontGet("consolas", 12, 0);
    if(fnt == NULL) {
        infof("Unable to get font");
        return 1;
    }
    fpsFont = fnt;
    //fid = fontGetGLImage(fnt, &fw, &fh);
    //fid = fontGetGLTexture(fnt);

    //info("Font loaded, targeting images.");

    //infof("Working dir: [%s]", getcwd(cwdbuf, sizeof(cwdbuf)));

    misc = texmgrGet("flub-keycap-misc");
    dlg = texmgrLoad( "work/dungeondlg2.gif", "dungeondlg2", GL_NEAREST, GL_NEAREST, 1, 255, 0, 255);
    //img = texmgrQuickLoad( "assets/misc/test_img.gif", GL_NEAREST, GL_NEAREST, 0, 0, 0, 0, &iw, &ih);
    footex = dlg;

    slice = gfxSliceCreate(dlg, 0, 0, 18, 22, 74, 69, 126, 106);
    //sliceTest = gfxSliceCreate(dlg, 145, 6, 150, 11, 182, 27, 186, 31);
    //infof("Texture id for flubmisc1 is %d", misc->id);

    rawSlices = texmgrGet("flub-slice-test");
    if(rawSlices == NULL) {
        fatal("Failed to load slices");
        return 1;
    }

    healthBar = gfx3x1SliceCreate(rawSlices, 0, 106, 28, 130, 34, 42);
    info("== Expbar =========");
    expBar = gfx1x3SliceCreate(rawSlices, 44, 106, 60, 128, 137, 152);
    info("== Done expbar ====");
    //vboTestInit(misc->id);

    sound = audioSoundGet("resources/sounds/menumove.wav");

    mesh = gfxMeshCreate2(MESH_QUAD_SIZE(40), 0, misc);
    //infof("The mesh is 0x%p", mesh);
    gfxTexMeshBlit2(mesh, misc, 20, 20);
    //gfxMeshBlit(mesh, 220, 20);
    //gfxMeshBlit(mesh, 420, 20);
    //gfxMeshBlit(mesh, 20, 220);
    //gfxMeshBlit(mesh, 220, 220);
    //gfxMeshBlit(mesh, 420, 220);

    //infof("Vertices: %d", mesh->pos);

    fontMesh = gfxMeshCreate2(256 * 2, GFX_MESH_FLAG_COLOR, NULL);
    gfxMeshTextureAssign2(fontMesh, fontTextureGet(fnt));

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
    fontBlitQCStrMesh(fontMesh, fnt, "font^2Blit^1QC^wStr();");
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

    dlg_title = gfxSliceCreate(tex_misc, 41, 17, 43, 19, 60, 26, 62, 28);
    dlg_body = gfxSliceCreate(tex_misc, 41, 29, 43, 30, 60, 40, 62, 42);

    // Create meshes for font, flubmisc, and flubsimplegui
    meshFont = gfxMeshCreate2(MESH_QUAD_SIZE(100), GFX_MESH_FLAG_COLOR, fontTextureGet(fnt));
    meshDlg = gfxMeshCreate2(MESH_QUAD_SIZE(200), GFX_MESH_FLAG_COLOR, tex_dlg);
    meshTiles = gfxMeshCreate2(MESH_QUAD_SIZE(400), 0, tex_tiles);
    meshMisc = gfxMeshCreate2(MESH_QUAD_SIZE(400), GFX_MESH_FLAG_COLOR, tex_misc);

    meshChain = meshFont;
    gfxMeshAppendToChain2(meshChain, meshMisc);
    gfxMeshAppendToChain2(meshChain, meshDlg);
    gfxMeshAppendToChain2(meshChain, meshTiles);

    // Excercise mesh blitters
    gfxTexMeshBlit2(meshChain, tex_misc, 400, 5);
    gfxTexMeshBlitSub2(meshChain, tex_dlg, 145, 6, 186, 31, 400, 200, 450, 250);
    gfxTexMeshTile2(meshChain, tex_dlg, 150, 11, 180, 25, 400, 260, 500, 400);
    blitMeshSpriteMap(meshChain, sprites, 200, 20, tileMap, scene, 3);
    gfxSpriteMeshBlitResize2(meshChain, sprites, 36, 0, 0, 63, 63);


    gfxSliceMeshBlit2(meshChain, dlg_title, 200, 300, 320, 315);
    gfxSliceMeshBlit2(meshChain, dlg_body, 200, 316, 320, 440);
    gfxKeycapMeshBlit2(meshChain, fnt, "META_WINDOWS", 200, 260, NULL, NULL);
    gfxKeycapMeshBlit2(meshChain, fnt, "Ctrl", 240, 260, NULL, NULL);
    gfxKeycapMeshBlit2(meshChain, fnt, "PgDn", 280, 260, NULL, NULL);

    effectMesh = meshDlg;
    effectStart = meshDlg->pos;
    gfxSliceMeshBlit2(meshDlg, slice, 10, 50, 150, 190);
    effectStop = meshDlg->pos;

    effect = gfxEffectFade(effectMesh, effectStart, effectStop, 0.0, 1.0, 2000);
    effect->completed = fadeinCompleted;
    gfxEffectRegister(effect);


    /*
    if((!flubFontLoad("pirulen.30.stbfont")) ||
       ((pfont = fontGet("pirulen", 30, 0)) == NULL)) {
        fatal("Unable to load pirulen font");
        return 0;
    }
    */

    inputActionBind("KEY_BACKQUOTE", "showconsole");
    //glBlendFunc(GL_ONE, GL_ZERO);
    //glDisable(GL_BLEND);
    //glEnable(GL_TEXTURE_2D);

    info("### Loading theme ###############");
    flubGuiThemeLoad("assets/data/flub-basic.theme");
    info("### Done loading theme ##########");

    lastTick = SDL_GetTicks();
    while (keepGoing) {
        current = SDL_GetTicks();
        elapsed = current - lastTick;
        lastTick = current;
        counter += elapsed;
        if(counter >= 2500) {
            if(sound != NULL) {
                //Mix_PlayChannel(0, sound, 0);
            }
            counter -= 2500;
        }
        // Process every event
        SDL_Event ev;
        // wait no more than 15ms to avoid exceeding 67 fps
        //while (SDL_WaitEventTimeout(&ev, 15)) {
        eventCount = 0;
        while(inputPollEvent(&ev)) {
            eventCount++;
            switch (ev.type) {
                case SDL_QUIT:
                    keepGoing = 0;
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
                            keepGoing = 0;
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
                        case SDLK_ESCAPE:
                            keepGoing = 0;
                            break;
                        case SDLK_BACKQUOTE:
                            /*
                            if(consoleVisible()) {
                                consoleShow(0);
                            } else {
                                consoleShow(1);
                            }
                            */
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
            if(eventCount >= BULK_EVENT_LIMIT) {
                break;
            }
        }
        videoClear();

        //glDisable(GL_DEPTH_TEST);
        //glDisable(GL_CULL_FACE);
        //glDisable(GL_BLEND);

        videoPushGLState();
        videoOrthoMode();

        glLoadIdentity();
        glColor3f(1.0, 1.0, 1.0);
        //glEnable(GL_BLEND);
        //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        //fontSetColor(1.0, 1.0, 1.0);
        //gfxBlitKeyStr(fnt, "Use [META_UP] and [META_DOWN] to select a menu option.", 10, 440, NULL, NULL);
        //gfxBlitKeyStr(fnt, "Use [~] to open the console.", 10, 460, NULL, NULL);


        glLoadIdentity();

        //gfxTexTile(tex_dlg, 150, 11, 180, 25, 200, 260, 298, 400);
        //gfxTexBlit(tex_misc, 200, 5);
        //gfxTexBlitSub(tex_misc, 0, 0, 127, 127, 0, 5, 127, 132);

        //gfxMeshRender(fontMesh);

        //gfxSliceBlit(dlg_title, 200, 300, 320, 315);
        //gfxSliceBlit(dlg_body, 200, 316, 320, 440);
        //gfxSliceBlit(slice, 10, 50, 150, 190);


        glLoadIdentity();

        //fontMode();
        //fontPos(10, 200);
        //fontBlitStr(pfont, "This Is A Test! Werd, yo!");
        //glLoadIdentity();
        //gfxTexBlit(fontTextureGet(pfont), 10, 200);

        //videoPushGLState();
        //videoOrthoMode();

        //vboTestRender();

        //gfxGLBlit(img, 380, 10, 380 + iw, 10 + ih);


        //fontSetColor(0.5, 1.0, 0.5);
        //fontSetColor(1.0, 1.0, 1.0);
        //gfxGLBlit(fid, 5, 5, fw, fh);

        //gfxTexBlit(misc, 25, 120);
        //gfxGLBlit(misc->id, 5, 100, 5 + misc->width, 100 + misc->height);


        gfxMeshRender2(meshChain);

        gfxSliceBlit(slice, 100, 150, 300, 280);
        gfxSliceBlit(slice, 400, 200, 410, 280);
        gfxSliceBlit(slice, 50, 300, 320, 450);
        //gfxSliceBlit(healthBar, 10, *videoHeight - healthBar->height - 10, 300, *videoHeight - 10);
        //gfxSliceBlit(expBar, 10, *videoWidth - expBar->width - 10, *videoHeight - 10, *videoWidth - 10);
        //gfxSliceBlit(expBar, 10, 10, 50, 200);
        //videoPopGLState();

        if(!appUpdate(current)) {
            keepGoing = 0;
        }
    }

    gfxMeshDestroy2(mesh);

    return 0;
}

