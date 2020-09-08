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

#define ALIGN(n) __attribute__((aligned(n)))

#define VERTEX_FORMAT (GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D)

#define SCREEN_W 480
#define SCREEN_H 272

#define BUFFER_WIDTH 512

PSP_MODULE_INFO("PSP_TESTS", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

static uint32_t g_staticOffset = 0;
static int32_t g_exitRequest = 0;

SceCtrlData g_padData;

static uint32_t ALIGN(16) g_dspList[262144];

typedef struct
{
    uint32_t DiffuseColor;
    float Position[3];
} VertexInput;

const VertexInput ALIGN(16) vertices[] =
{
    { 0xFF0000FF, { -0.50f ,-0.25f, 1.0f } },
    { 0xFF00FF00, {  0.00f , 0.25f, 1.0f } },
    { 0xFFFF0000, {  0.50f ,-0.25f, 1.0f } },
}; const uint32_t nVertices = sizeof(vertices) / sizeof(VertexInput);

// VRAM Alloc

void* GetStaticVramBuffer(uint32_t width, uint32_t height, uint32_t psm);
void* GetStaticVramTexture(uint32_t width, uint32_t height, uint32_t psm);

// Callbacks

int32_t ExitCallback(int32_t arg1, int32_t arg2, void* common);
int32_t CallbackThread(SceSize args, void* argp);

// MAIN

int main()
{
    int32_t threadId = sceKernelCreateThread("update_thread", CallbackThread, 0x11, 0xFA0, 0, 0);
    assert(threadId >= 0);
    
    sceKernelStartThread(threadId, 0, 0);
    
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    
    void* pFrontBuffer = GetStaticVramBuffer(BUFFER_WIDTH, SCREEN_H, GU_PSM_8888);
    void* pBackBuffer  = GetStaticVramBuffer(BUFFER_WIDTH, SCREEN_H, GU_PSM_8888);
    void* pDepthBuffer = GetStaticVramBuffer(BUFFER_WIDTH, SCREEN_H, GU_PSM_4444);
    
    sceGuInit();
    sceGuStart(GU_DIRECT, g_dspList);
    
    // Setup VRAM buffers
    
    sceGuDrawBuffer(GU_PSM_8888, pFrontBuffer, BUFFER_WIDTH);
    sceGuDispBuffer(SCREEN_W, SCREEN_H, pBackBuffer, BUFFER_WIDTH);
    
    sceGuOffset(2048 - (SCREEN_W / 2), 2048 - (SCREEN_H / 2));
    sceGuViewport(2048, 2048, SCREEN_W, SCREEN_H);
    
    // Depth buffer
    
    sceGuDepthBuffer(pDepthBuffer, BUFFER_WIDTH);
    sceGuDepthRange(65535, 0);
    sceGuDepthFunc(GU_GEQUAL);
    
    // Culling
    
    sceGuFrontFace(GU_CCW);
    sceGuDisable(GU_CULL_FACE);
    
    // Misc
    
    sceGuShadeModel(GU_SMOOTH);
    sceGuScissor(0, 0, SCREEN_W, SCREEN_H);
    
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuEnable(GU_CLIP_PLANES);
    
    sceGuDisable(GU_TEXTURE_2D);
    
    sceGuFinish();
    sceGuSync(0, 0);
    
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
    
    float angle = 0.0f; 
    
    while (!g_exitRequest)
    {
        sceCtrlReadBufferPositive(&g_padData, 1);
        
        if (!(g_padData.Buttons & PSP_CTRL_CROSS)) {
            angle += 0.05f;
        }
        
        sceGuStart(GU_DIRECT, g_dspList);
        
        sceGuClearColor(0xFF2D2D2D);
        sceGuClearDepth(0);
        
        sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
        
        // Setup matrices
        
        sceGumMatrixMode(GU_PROJECTION);
        sceGumLoadIdentity();
        sceGumPerspective(75.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
        
        sceGumMatrixMode(GU_VIEW);
        sceGumLoadIdentity();
        {
            ScePspFVector3 eye = { 0.0f, 0.0f,-2.0f };
            ScePspFVector3 at  = { 0.0f, 0.0f, 0.0f }; 
            ScePspFVector3 up  = { 0.0f, 1.0f, 0.0f };
            
            sceGumLookAt(&eye, &at, &up);
        }
        
        sceGumMatrixMode(GU_MODEL);
        sceGumLoadIdentity();
        sceGumRotateY(angle);
        
        sceGumDrawArray(GU_TRIANGLES, VERTEX_FORMAT, nVertices, NULL, vertices);
        
        sceGuFinish();
        sceGuSync(0, 0);
        
        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }
    
    sceGuTerm();
    sceKernelExitGame();
    
    return 0;
}

// Definitions

int32_t ExitCallback(int32_t arg1, int32_t arg2, void* common)
{
    g_exitRequest = 1;
    return 0;
}

int32_t CallbackThread(SceSize args, void* argp)
{
    int32_t exitCallbackId = sceKernelCreateCallback("Exit Callback", ExitCallback, NULL);
    
    sceKernelRegisterExitCallback(exitCallbackId);
    sceKernelSleepThreadCB();
    
    return 0;
}

static uint32_t GetMemorySize(uint32_t width, uint32_t height, uint32_t psm)
{
    switch (psm)
    {
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

void* GetStaticVramBuffer(uint32_t width, uint32_t height, uint32_t psm)
{
    uint32_t memSize = GetMemorySize(width, height, psm);
    
    void* result = (void*)g_staticOffset;
    g_staticOffset += memSize;
    
    return result;
}

void* GetStaticVramTexture(uint32_t width, uint32_t height, uint32_t psm)
{
    void* result = GetStaticVramBuffer(width, height, psm);
    return (void*)(((uint32_t)result) + ((uint32_t)sceGeEdramGetAddr()));
}
