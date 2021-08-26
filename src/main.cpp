#include <pspkernel.h>
#include <pspgu.h>
#include <pspge.h>
#include <pspgum.h>
#include <pspdisplay.h>
#include <pspctrl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <malloc.h>

#include "amogus.h"

#define STB_IMAGE_STATIC
//#include "include/stb_image/stb_image.h"

#include "types.h"

#define ALIGN(n) __attribute__((aligned(n)))
#define VERTEX_FORMAT (GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_INDEX_16BIT | GU_TRANSFORM_3D)

#define MAX_LIST_SIZE 262144

#define DISPLAY_W 480
#define DISPLAY_H 272

#define BUFFER_WIDTH 512

PSP_MODULE_INFO("PSP_TESTS", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

static u32 gStaticOffset = 0;
static bool gExitRequest = false;

SceCtrlData gPadData;

static u32 ALIGN(16) gDspList[MAX_LIST_SIZE];

typedef struct {
    f32 uv[2];
    f32 pos[3];
} VertexInput;

const VertexInput ALIGN(16) gVertices[] = {
	{ { 1.0f, 0.0f }, { -1.0f, -1.0f,  1.0f } },
	{ { 0.0f, 0.0f }, {  1.0f, -1.0f,  1.0f } },
	{ { 0.0f, 1.0f }, {  1.0f, -1.0f, -1.0f } },
	{ { 1.0f, 1.0f }, { -1.0f, -1.0f, -1.0f } },
      
	{ { 0.0f, 0.0f }, { -1.0f,  1.0f,  1.0f } },
	{ { 0.0f, 1.0f }, {  1.0f,  1.0f,  1.0f } },
	{ { 1.0f, 1.0f }, {  1.0f,  1.0f, -1.0f } },
	{ { 1.0f, 0.0f }, { -1.0f,  1.0f, -1.0f } },
      
	{ { 1.0f, 1.0f }, { -1.0f,  1.0f, -1.0f } },
	{ { 1.0f, 0.0f }, { -1.0f,  1.0f,  1.0f } },
	{ { 0.0f, 0.0f }, { -1.0f, -1.0f,  1.0f } },
	{ { 0.0f, 1.0f }, { -1.0f, -1.0f, -1.0f } },
      
	{ { 1.0f, 0.0f }, {  1.0f,  1.0f, -1.0f } },
	{ { 0.0f, 0.0f }, {  1.0f,  1.0f,  1.0f } },
	{ { 0.0f, 1.0f }, {  1.0f, -1.0f,  1.0f } },
	{ { 1.0f, 1.0f }, {  1.0f, -1.0f, -1.0f } },
      
	{ { 0.0f, 1.0f }, { -1.0f,  1.0f,  1.0f } },
	{ { 1.0f, 1.0f }, {  1.0f,  1.0f,  1.0f } },
	{ { 1.0f, 0.0f }, {  1.0f, -1.0f,  1.0f } },
	{ { 0.0f, 0.0f }, { -1.0f, -1.0f,  1.0f } },
      
	{ { 0.0f, 1.0f }, { -1.0f,  1.0f, -1.0f } },
	{ { 1.0f, 1.0f }, {  1.0f,  1.0f, -1.0f } },
	{ { 1.0f, 0.0f }, {  1.0f, -1.0f, -1.0f } },
	{ { 0.0f, 0.0f }, { -1.0f, -1.0f, -1.0f } },
};

const u16 ALIGN(16) gIndices[] = {
	 0, 1, 2,   2, 3, 0,
     4, 5, 6,   6, 7, 4,
     8, 9,10,  10,11, 8,
    12,13,14,  14,15,12,
    16,17,18,  18,19,16,
    20,21,22,  22,23,20 
}; const u32 nIndices = sizeof(gIndices) / sizeof(u16);

u8* gAmogusTex = NULL;

// VRAM Alloc

void* GetStaticVramBuffer(u32 width, u32 height, u32 psm);
void* GetStaticVramTexture(u32 width, u32 height, u32 psm);

// Callbacks

s32 ExitCallback(s32 arg1, s32 arg2, void* common);
s32 CallbackThread(SceSize args, void* argp);

// Utils
void PrintFileStr(const char* path);
void TexRGBToBGR(u8* pixelData, const u32 bufferSize);

// MAIN

int main() 
{
    s32 threadId = sceKernelCreateThread("update_thread", CallbackThread, 0x11, 0xFA0, 0, 0);
    assert(threadId >= 0);
    
    sceKernelStartThread(threadId, 0, 0);
    
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    
    void* pFrontBuffer = GetStaticVramBuffer(BUFFER_WIDTH, DISPLAY_H, GU_PSM_8888);
    void* pBackBuffer  = GetStaticVramBuffer(BUFFER_WIDTH, DISPLAY_H, GU_PSM_8888);
    void* pDepthBuffer = GetStaticVramBuffer(BUFFER_WIDTH, DISPLAY_H, GU_PSM_4444);
    
    sceGuInit();
    sceGuStart(GU_DIRECT, gDspList);
    
    // Setup VRAM buffers
    
    sceGuDrawBuffer(GU_PSM_8888, pFrontBuffer, BUFFER_WIDTH);
    sceGuDispBuffer(DISPLAY_W, DISPLAY_H, pBackBuffer, BUFFER_WIDTH);
    
    sceGuOffset(2048 - (DISPLAY_W / 2), 2048 - (DISPLAY_H / 2));
    sceGuViewport(2048, 2048, DISPLAY_W, DISPLAY_H);
    
    // Depth buffer
    
    sceGuDepthBuffer(pDepthBuffer, BUFFER_WIDTH);
    sceGuDepthRange(65535, 0);
    sceGuDepthFunc(GU_GEQUAL);
    
    // Culling
    
    sceGuFrontFace(GU_CCW);
    sceGuDisable(GU_CULL_FACE);
    
    // Misc
    
    sceGuShadeModel(GU_SMOOTH);
    sceGuScissor(0, 0, DISPLAY_W, DISPLAY_H);
    
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuEnable(GU_CLIP_PLANES);
    
    sceGuEnable(GU_TEXTURE_2D);
    sceGuDisable(GU_LIGHTING);
    
    // Setup transform matrices
    
    sceGumMatrixMode(GU_PROJECTION);
    sceGumLoadIdentity();
    sceGumPerspective(45.0f, 16.0f / 9.0f, 1.0f, 100.0f);
    
    sceGumMatrixMode(GU_VIEW);
    sceGumLoadIdentity();

	ScePspFVector3 eye = { 0.0f, 3.0f,-5.0f };
    ScePspFVector3 at  = { 0.0f, 0.0f, 0.0f };
    ScePspFVector3 up  = { 0.0f, 1.0f, 0.0f };
    sceGumLookAt(&eye, &at, &up);
    
    sceGuFinish();
    sceGuSync(0, 0);
    
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
    
    f32 angle = 0.0f;
    
    //u32 w, h;
    //gAmogusTex = stbi_load("data/texture/amogus.png", (int*)&w, (int*)&h, NULL, 4);

    //printf("{ %d, %d }\n", w, h);

    TexRGBToBGR(amogus, sizeof(amogus));

    while (!gExitRequest) {
		angle += 0.02f;
		
        sceCtrlReadBufferPositive(&gPadData, 1);
        
        sceGuStart(GU_DIRECT, gDspList);
        
        sceGuClearColor(0xFF2D2D2D);
        sceGuClearDepth(0);
        
        sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
               
        sceGumMatrixMode(GU_MODEL);
		sceGumLoadIdentity();
		sceGumRotateY(angle);

        // Setup texture
        
        sceGuTexMode(GU_PSM_8888, 0, 0, 0);
        sceGuTexImage(0, 128, 128, 128, amogus);
        sceGuTexFunc(GU_TFX_ADD, GU_TCC_RGBA);
        sceGuTexFilter(GU_LINEAR, GU_LINEAR);
        sceGuTexScale(1.0f,1.0f);

        sceGumDrawArray(GU_TRIANGLES, VERTEX_FORMAT, nIndices, gIndices, gVertices);
        
        sceGuFinish();
        sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
        
        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }
    
    //stbi_image_free(gAmogusTex);

    sceGuTerm();
    sceKernelExitGame();
    
    return 0;
}

// Definitions

s32 ExitCallback(s32 arg1, s32 arg2, void* common) {
    gExitRequest = true;
    return 0;
}

s32 CallbackThread(SceSize args, void* argp) {
    s32 exitCallbackId = sceKernelCreateCallback("Exit Callback", ExitCallback, NULL);
    
    sceKernelRegisterExitCallback(exitCallbackId);
    sceKernelSleepThreadCB();
    
    return 0;
}

static u32 GetMemorySize(u32 width, u32 height, u32 psm) {
    switch (psm) {
        case GU_PSM_T4:
            return (width * height) >> 1;
        
        case GU_PSM_T8:
            return width * height;
        
        case GU_PSM_5650:
        case GU_PSM_5551:
        case GU_PSM_4444:
        case GU_PSM_T16:
            return 2 * width * height;
        
        case GU_PSM_8888:
        case GU_PSM_T32:
            return 4 * width * height;
        
        default:
            return 0;
    }
}

void* GetStaticVramBuffer(u32 width, u32 height, u32 psm) {
    u32 memSize = GetMemorySize(width, height, psm);
    
    void* result = (void*)gStaticOffset;
    gStaticOffset += memSize;
    
    return result;
}

void* GetStaticVramTexture(u32 width, u32 height, u32 psm) {
    void* result = GetStaticVramBuffer(width, height, psm);
    return (void*)(((u32)result) + ((u32)sceGeEdramGetAddr()));
}

void PrintFileStr(const char* path) {
	FILE* fd = fopen(path, "r");

	if (fd == NULL) {
		printf("ERROR: fopen() failed, invalid file path\n");
		return;
	};

	fseek(fd, 0, SEEK_END);

	const u32 size = ftell(fd);
	fseek(fd, 0, SEEK_SET);

	char* buf = (char*) malloc(size);
	const u32 nBytes = fread(buf, 1, size, fd);

	if (nBytes < size) {
		printf("ERROR: read() byte size mismatch\n");

		free(buf);
		fclose(fd);

		return;
	}

	printf("%s\n", buf);
	free(buf);

	fclose(fd);
}

void TexRGBToBGR(u8* pixelData, const u32 bufferSize) {
    for (u32 index = 0; index < bufferSize; index += 4) {
        const u8 temp = pixelData[index];
        pixelData[index]     = pixelData[index + 2];
        pixelData[index + 2] = temp;
    }
}
