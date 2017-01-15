#include <psp2/display.h>
#include <psp2/gxm.h>
#include <psp2/types.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/message_dialog.h>
#include <psp2/sysmodule.h>
#include <string.h>
#include <stdlib.h>
#include "vita2d.h"
#include "utils.h"

#ifdef DEBUG_BUILD
#  include <stdio.h>
#  define DEBUG(...) printf(__VA_ARGS__)
#else
#  define DEBUG(...)
#endif

/* Defines */

#define DISPLAY_WIDTH			960
#define DISPLAY_HEIGHT			544
#define DISPLAY_STRIDE_IN_PIXELS	1024
#define DISPLAY_COLOR_FORMAT		SCE_GXM_COLOR_FORMAT_A8B8G8R8
#define DISPLAY_PIXEL_FORMAT		SCE_DISPLAY_PIXELFORMAT_A8B8G8R8
#define DISPLAY_BUFFER_COUNT		3
#define DISPLAY_MAX_PENDING_SWAPS	2
#define MSAA_MODE			SCE_GXM_MULTISAMPLE_NONE
#define DEFAULT_TEMP_POOL_SIZE		(1 * 1024 * 1024)

typedef struct vita2d_display_data {
	void *address;
} vita2d_display_data;

/* Extern */

extern const SceGxmProgram clear_v_gxp_start;
extern const SceGxmProgram clear_f_gxp_start;
extern const SceGxmProgram color_v_gxp_start;
extern const SceGxmProgram color_f_gxp_start;
extern const SceGxmProgram texture_v_gxp_start;
extern const SceGxmProgram texture_f_gxp_start;
extern const SceGxmProgram texture_tint_f_gxp_start;

/* Static variables */

static int pgf_module_was_loaded = 0;

static const SceGxmProgram *const clearVertexProgramGxp         = &clear_v_gxp_start;
static const SceGxmProgram *const clearFragmentProgramGxp       = &clear_f_gxp_start;
static const SceGxmProgram *const colorVertexProgramGxp         = &color_v_gxp_start;
static const SceGxmProgram *const colorFragmentProgramGxp       = &color_f_gxp_start;
static const SceGxmProgram *const textureVertexProgramGxp       = &texture_v_gxp_start;
static const SceGxmProgram *const textureFragmentProgramGxp     = &texture_f_gxp_start;
static const SceGxmProgram *const textureTintFragmentProgramGxp = &texture_tint_f_gxp_start;

static int vita2d_initialized = 0;
static float clear_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
static int clip_rect_x_min = 0;
static int clip_rect_y_min = 0;
static int clip_rect_x_max = DISPLAY_WIDTH;
static int clip_rect_y_max = DISPLAY_HEIGHT;
static int vblank_wait = 1;
static int drawing = 0;
static int clipping_enabled = 0;

static SceUID vdmRingBufferUid;
static SceUID vertexRingBufferUid;
static SceUID fragmentRingBufferUid;
static SceUID fragmentUsseRingBufferUid;

static SceGxmContextParams contextParams;
static SceGxmRenderTarget *renderTarget = NULL;
static SceUID displayBufferUid[DISPLAY_BUFFER_COUNT];
static void *displayBufferData[DISPLAY_BUFFER_COUNT];
static SceUID displayBufferUid[DISPLAY_BUFFER_COUNT];
static SceGxmColorSurface displaySurface[DISPLAY_BUFFER_COUNT];
static SceGxmSyncObject *displayBufferSync[DISPLAY_BUFFER_COUNT];
static SceUID depthBufferUid;
static SceUID stencilBufferUid;
static SceGxmDepthStencilSurface depthSurface;
static void *depthBufferData = NULL;
static void *stencilBufferData = NULL;

static unsigned int backBufferIndex = 0;
static unsigned int frontBufferIndex = 0;

static SceGxmShaderPatcher *shaderPatcher = NULL;
static SceGxmVertexProgram *clearVertexProgram = NULL;
static SceGxmFragmentProgram *clearFragmentProgram = NULL;

static SceGxmShaderPatcherId clearVertexProgramId;
static SceGxmShaderPatcherId clearFragmentProgramId;
static SceGxmShaderPatcherId colorVertexProgramId;
static SceGxmShaderPatcherId colorFragmentProgramId;
static SceGxmShaderPatcherId textureVertexProgramId;
static SceGxmShaderPatcherId textureFragmentProgramId;
static SceGxmShaderPatcherId textureTintFragmentProgramId;

static SceUID patcherBufferUid;
static SceUID patcherVertexUsseUid;
static SceUID patcherFragmentUsseUid;

static SceUID clearVerticesUid;
static SceUID clearIndicesUid;
static vita2d_clear_vertex *clearVertices = NULL;
static uint16_t *clearIndices = NULL;

/* Shared with other .c */
float _vita2d_ortho_matrix[4*4];
SceGxmContext *_vita2d_context = NULL;
SceGxmVertexProgram *_vita2d_colorVertexProgram = NULL;
SceGxmFragmentProgram *_vita2d_colorFragmentProgram = NULL;
SceGxmVertexProgram *_vita2d_textureVertexProgram = NULL;
SceGxmFragmentProgram *_vita2d_textureFragmentProgram = NULL;
SceGxmFragmentProgram *_vita2d_textureTintFragmentProgram = NULL;
const SceGxmProgramParameter *_vita2d_clearClearColorParam = NULL;
const SceGxmProgramParameter *_vita2d_colorWvpParam = NULL;
const SceGxmProgramParameter *_vita2d_textureWvpParam = NULL;
const SceGxmProgramParameter *_vita2d_textureTintColorParam = NULL;

// Temporary memory pool
static void *pool_addr = NULL;
static SceUID poolUid;
static unsigned int pool_index = 0;
static unsigned int pool_size = 0;

/* Static functions */

static void *patcher_host_alloc(void *user_data, unsigned int size)
{
	return malloc(size);
}

static void patcher_host_free(void *user_data, void *mem)
{
	free(mem);
}

static void display_callback(const void *callback_data)
{
	SceDisplayFrameBuf framebuf;
	const vita2d_display_data *display_data = (const vita2d_display_data *)callback_data;

	memset(&framebuf, 0x00, sizeof(SceDisplayFrameBuf));
	framebuf.size        = sizeof(SceDisplayFrameBuf);
	framebuf.base        = display_data->address;
	framebuf.pitch       = DISPLAY_STRIDE_IN_PIXELS;
	framebuf.pixelformat = DISPLAY_PIXEL_FORMAT;
	framebuf.width       = DISPLAY_WIDTH;
	framebuf.height      = DISPLAY_HEIGHT;
	sceDisplaySetFrameBuf(&framebuf, SCE_DISPLAY_SETBUF_NEXTFRAME);

	if (vblank_wait) {
		sceDisplayWaitVblankStart();
	}
}

int vita2d_init()
{
	return vita2d_init_advanced(DEFAULT_TEMP_POOL_SIZE);
}

int vita2d_init_advanced(unsigned int temp_pool_size)
{
	int err;
	unsigned int i, x, y;
	UNUSED(err);

	if (vita2d_initialized) {
		DEBUG("libvita2d is already initialized!\n");
		return 1;
	}

	SceGxmInitializeParams initializeParams;
	memset(&initializeParams, 0, sizeof(SceGxmInitializeParams));
	initializeParams.flags				= 0;
	initializeParams.displayQueueMaxPendingCount	= DISPLAY_MAX_PENDING_SWAPS;
	initializeParams.displayQueueCallback		= display_callback;
	initializeParams.displayQueueCallbackDataSize	= sizeof(vita2d_display_data);
	initializeParams.parameterBufferSize		= SCE_GXM_DEFAULT_PARAMETER_BUFFER_SIZE;

	err = sceGxmInitialize(&initializeParams);
	DEBUG("sceGxmInitialize(): 0x%08X\n", err);

	// allocate ring buffer memory using default sizes
	void *vdmRingBuffer = gpu_alloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE,
		4,
		SCE_GXM_MEMORY_ATTRIB_READ,
		&vdmRingBufferUid);

	void *vertexRingBuffer = gpu_alloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE,
		4,
		SCE_GXM_MEMORY_ATTRIB_READ,
		&vertexRingBufferUid);

	void *fragmentRingBuffer = gpu_alloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE,
		4,
		SCE_GXM_MEMORY_ATTRIB_READ,
		&fragmentRingBufferUid);

	unsigned int fragmentUsseRingBufferOffset;
	void *fragmentUsseRingBuffer = fragment_usse_alloc(
		SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE,
		&fragmentUsseRingBufferUid,
		&fragmentUsseRingBufferOffset);

	memset(&contextParams, 0, sizeof(SceGxmContextParams));
	contextParams.hostMem				= malloc(SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE);
	contextParams.hostMemSize			= SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE;
	contextParams.vdmRingBufferMem			= vdmRingBuffer;
	contextParams.vdmRingBufferMemSize		= SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE;
	contextParams.vertexRingBufferMem		= vertexRingBuffer;
	contextParams.vertexRingBufferMemSize		= SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE;
	contextParams.fragmentRingBufferMem		= fragmentRingBuffer;
	contextParams.fragmentRingBufferMemSize		= SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE;
	contextParams.fragmentUsseRingBufferMem		= fragmentUsseRingBuffer;
	contextParams.fragmentUsseRingBufferMemSize	= SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE;
	contextParams.fragmentUsseRingBufferOffset	= fragmentUsseRingBufferOffset;

	err = sceGxmCreateContext(&contextParams, &_vita2d_context);
	DEBUG("sceGxmCreateContext(): 0x%08X\n", err);

	// set up parameters
	SceGxmRenderTargetParams renderTargetParams;
	memset(&renderTargetParams, 0, sizeof(SceGxmRenderTargetParams));
	renderTargetParams.flags			= 0;
	renderTargetParams.width			= DISPLAY_WIDTH;
	renderTargetParams.height			= DISPLAY_HEIGHT;
	renderTargetParams.scenesPerFrame		= 1;
	renderTargetParams.multisampleMode		= MSAA_MODE;
	renderTargetParams.multisampleLocations		= 0;
	renderTargetParams.driverMemBlock		= -1; // Invalid UID

	// create the render target
	err = sceGxmCreateRenderTarget(&renderTargetParams, &renderTarget);
	DEBUG("sceGxmCreateRenderTarget(): 0x%08X\n", err);

	// allocate memory and sync objects for display buffers
	for (i = 0; i < DISPLAY_BUFFER_COUNT; i++) {
		// allocate memory for display
		displayBufferData[i] = gpu_alloc(
			SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
			4*DISPLAY_STRIDE_IN_PIXELS*DISPLAY_HEIGHT,
			SCE_GXM_COLOR_SURFACE_ALIGNMENT,
			SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
			&displayBufferUid[i]);

		// memset the buffer to black
		for (y = 0; y < DISPLAY_HEIGHT; y++) {
			unsigned int *row = (unsigned int *)displayBufferData[i] + y*DISPLAY_STRIDE_IN_PIXELS;
			for (x = 0; x < DISPLAY_WIDTH; x++) {
				row[x] = 0xff000000;
			}
		}

		// initialize a color surface for this display buffer
		err = sceGxmColorSurfaceInit(
			&displaySurface[i],
			DISPLAY_COLOR_FORMAT,
			SCE_GXM_COLOR_SURFACE_LINEAR,
			(MSAA_MODE == SCE_GXM_MULTISAMPLE_NONE) ? SCE_GXM_COLOR_SURFACE_SCALE_NONE : SCE_GXM_COLOR_SURFACE_SCALE_MSAA_DOWNSCALE,
			SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT,
			DISPLAY_WIDTH,
			DISPLAY_HEIGHT,
			DISPLAY_STRIDE_IN_PIXELS,
			displayBufferData[i]);

		// create a sync object that we will associate with this buffer
		err = sceGxmSyncObjectCreate(&displayBufferSync[i]);
	}

	// compute the memory footprint of the depth buffer
	const unsigned int alignedWidth = ALIGN(DISPLAY_WIDTH, SCE_GXM_TILE_SIZEX);
	const unsigned int alignedHeight = ALIGN(DISPLAY_HEIGHT, SCE_GXM_TILE_SIZEY);
	unsigned int sampleCount = alignedWidth*alignedHeight;
	unsigned int depthStrideInSamples = alignedWidth;
	if (MSAA_MODE == SCE_GXM_MULTISAMPLE_4X) {
		// samples increase in X and Y
		sampleCount *= 4;
		depthStrideInSamples *= 2;
	} else if (MSAA_MODE == SCE_GXM_MULTISAMPLE_2X) {
		// samples increase in Y only
		sampleCount *= 2;
	}

	// allocate the depth buffer
	depthBufferData = gpu_alloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		4*sampleCount,
		SCE_GXM_DEPTHSTENCIL_SURFACE_ALIGNMENT,
		SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
		&depthBufferUid);

	// allocate the stencil buffer
	stencilBufferData = gpu_alloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		4*sampleCount,
		SCE_GXM_DEPTHSTENCIL_SURFACE_ALIGNMENT,
		SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
		&stencilBufferUid);

	// create the SceGxmDepthStencilSurface structure
	err = sceGxmDepthStencilSurfaceInit(
		&depthSurface,
		SCE_GXM_DEPTH_STENCIL_FORMAT_S8D24,
		SCE_GXM_DEPTH_STENCIL_SURFACE_TILED,
		depthStrideInSamples,
		depthBufferData,
		stencilBufferData);
	
	// set the stencil test reference (this is currently assumed to always remain 1 after here for region clipping)
	sceGxmSetFrontStencilRef(_vita2d_context, 1);
	// set the stencil function (this wouldn't actually be needed, as the set clip rectangle function has to call this at the begginning of every scene)
	sceGxmSetFrontStencilFunc(
		_vita2d_context,
		SCE_GXM_STENCIL_FUNC_ALWAYS,
		SCE_GXM_STENCIL_OP_KEEP,
		SCE_GXM_STENCIL_OP_KEEP,
		SCE_GXM_STENCIL_OP_KEEP,
		0xFF,
		0xFF);

	// set buffer sizes for this sample
	const unsigned int patcherBufferSize		= 64*1024;
	const unsigned int patcherVertexUsseSize	= 64*1024;
	const unsigned int patcherFragmentUsseSize	= 64*1024;

	// allocate memory for buffers and USSE code
	void *patcherBuffer = gpu_alloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		patcherBufferSize,
		4,
		SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
		&patcherBufferUid);

	unsigned int patcherVertexUsseOffset;
	void *patcherVertexUsse = vertex_usse_alloc(
		patcherVertexUsseSize,
		&patcherVertexUsseUid,
		&patcherVertexUsseOffset);

	unsigned int patcherFragmentUsseOffset;
	void *patcherFragmentUsse = fragment_usse_alloc(
		patcherFragmentUsseSize,
		&patcherFragmentUsseUid,
		&patcherFragmentUsseOffset);

	// create a shader patcher
	SceGxmShaderPatcherParams patcherParams;
	memset(&patcherParams, 0, sizeof(SceGxmShaderPatcherParams));
	patcherParams.userData			= NULL;
	patcherParams.hostAllocCallback		= &patcher_host_alloc;
	patcherParams.hostFreeCallback		= &patcher_host_free;
	patcherParams.bufferAllocCallback	= NULL;
	patcherParams.bufferFreeCallback	= NULL;
	patcherParams.bufferMem			= patcherBuffer;
	patcherParams.bufferMemSize		= patcherBufferSize;
	patcherParams.vertexUsseAllocCallback	= NULL;
	patcherParams.vertexUsseFreeCallback	= NULL;
	patcherParams.vertexUsseMem		= patcherVertexUsse;
	patcherParams.vertexUsseMemSize		= patcherVertexUsseSize;
	patcherParams.vertexUsseOffset		= patcherVertexUsseOffset;
	patcherParams.fragmentUsseAllocCallback	= NULL;
	patcherParams.fragmentUsseFreeCallback	= NULL;
	patcherParams.fragmentUsseMem		= patcherFragmentUsse;
	patcherParams.fragmentUsseMemSize	= patcherFragmentUsseSize;
	patcherParams.fragmentUsseOffset	= patcherFragmentUsseOffset;

	err = sceGxmShaderPatcherCreate(&patcherParams, &shaderPatcher);
	DEBUG("sceGxmShaderPatcherCreate(): 0x%08X\n", err);

	// check the shaders
	err = sceGxmProgramCheck(clearVertexProgramGxp);
	DEBUG("clear_v sceGxmProgramCheck(): 0x%08X\n", err);
	err = sceGxmProgramCheck(clearFragmentProgramGxp);
	DEBUG("clear_f sceGxmProgramCheck(): 0x%08X\n", err);
	err = sceGxmProgramCheck(colorVertexProgramGxp);
	DEBUG("color_v sceGxmProgramCheck(): 0x%08X\n", err);
	err = sceGxmProgramCheck(colorFragmentProgramGxp);
	DEBUG("color_f sceGxmProgramCheck(): 0x%08X\n", err);
	err = sceGxmProgramCheck(textureVertexProgramGxp);
	DEBUG("texture_v sceGxmProgramCheck(): 0x%08X\n", err);
	err = sceGxmProgramCheck(textureFragmentProgramGxp);
	DEBUG("texture_f sceGxmProgramCheck(): 0x%08X\n", err);
	err = sceGxmProgramCheck(textureTintFragmentProgramGxp);
	DEBUG("texture_tint_f sceGxmProgramCheck(): 0x%08X\n", err);

	// register programs with the patcher
	err = sceGxmShaderPatcherRegisterProgram(shaderPatcher, clearVertexProgramGxp, &clearVertexProgramId);
	DEBUG("clear_v sceGxmShaderPatcherRegisterProgram(): 0x%08X\n", err);

	err = sceGxmShaderPatcherRegisterProgram(shaderPatcher, clearFragmentProgramGxp, &clearFragmentProgramId);
	DEBUG("clear_f sceGxmShaderPatcherRegisterProgram(): 0x%08X\n", err);

	err = sceGxmShaderPatcherRegisterProgram(shaderPatcher, colorVertexProgramGxp, &colorVertexProgramId);
	DEBUG("color_v sceGxmShaderPatcherRegisterProgram(): 0x%08X\n", err);

	err = sceGxmShaderPatcherRegisterProgram(shaderPatcher, colorFragmentProgramGxp, &colorFragmentProgramId);
	DEBUG("color_f sceGxmShaderPatcherRegisterProgram(): 0x%08X\n", err);

	err = sceGxmShaderPatcherRegisterProgram(shaderPatcher, textureVertexProgramGxp, &textureVertexProgramId);
	DEBUG("texture_v sceGxmShaderPatcherRegisterProgram(): 0x%08X\n", err);

	err = sceGxmShaderPatcherRegisterProgram(shaderPatcher, textureFragmentProgramGxp, &textureFragmentProgramId);
	DEBUG("texture_f sceGxmShaderPatcherRegisterProgram(): 0x%08X\n", err);

	err = sceGxmShaderPatcherRegisterProgram(shaderPatcher, textureTintFragmentProgramGxp, &textureTintFragmentProgramId);
	DEBUG("texture_tint_f sceGxmShaderPatcherRegisterProgram(): 0x%08X\n", err);

	// Fill SceGxmBlendInfo
	static const SceGxmBlendInfo blend_info = {
		.colorFunc = SCE_GXM_BLEND_FUNC_ADD,
		.alphaFunc = SCE_GXM_BLEND_FUNC_ADD,
		.colorSrc  = SCE_GXM_BLEND_FACTOR_SRC_ALPHA,
		.colorDst  = SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.alphaSrc  = SCE_GXM_BLEND_FACTOR_ONE,
		.alphaDst  = SCE_GXM_BLEND_FACTOR_ZERO,
		.colorMask = SCE_GXM_COLOR_MASK_ALL
	};

	// get attributes by name to create vertex format bindings
	const SceGxmProgramParameter *paramClearPositionAttribute = sceGxmProgramFindParameterByName(clearVertexProgramGxp, "aPosition");

	// create clear vertex format
	SceGxmVertexAttribute clearVertexAttributes[1];
	SceGxmVertexStream clearVertexStreams[1];
	clearVertexAttributes[0].streamIndex	= 0;
	clearVertexAttributes[0].offset		= 0;
	clearVertexAttributes[0].format		= SCE_GXM_ATTRIBUTE_FORMAT_F32;
	clearVertexAttributes[0].componentCount	= 2;
	clearVertexAttributes[0].regIndex	= sceGxmProgramParameterGetResourceIndex(paramClearPositionAttribute);
	clearVertexStreams[0].stride		= sizeof(vita2d_clear_vertex);
	clearVertexStreams[0].indexSource	= SCE_GXM_INDEX_SOURCE_INDEX_16BIT;

	// create clear programs
	err = sceGxmShaderPatcherCreateVertexProgram(
		shaderPatcher,
		clearVertexProgramId,
		clearVertexAttributes,
		1,
		clearVertexStreams,
		1,
		&clearVertexProgram);

	DEBUG("clear sceGxmShaderPatcherCreateVertexProgram(): 0x%08X\n", err);

	err = sceGxmShaderPatcherCreateFragmentProgram(
		shaderPatcher,
		clearFragmentProgramId,
		SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		MSAA_MODE,
		NULL,
		clearVertexProgramGxp,
		&clearFragmentProgram);

	DEBUG("clear sceGxmShaderPatcherCreateFragmentProgram(): 0x%08X\n", err);

	// create the clear triangle vertex/index data
	clearVertices = (vita2d_clear_vertex *)gpu_alloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		3*sizeof(vita2d_clear_vertex),
		4,
		SCE_GXM_MEMORY_ATTRIB_READ,
		&clearVerticesUid);

	clearIndices = (uint16_t *)gpu_alloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		3*sizeof(uint16_t),
		2,
		SCE_GXM_MEMORY_ATTRIB_READ,
		&clearIndicesUid);

	clearVertices[0].x = -1.0f;
	clearVertices[0].y = -1.0f;
	clearVertices[1].x =  3.0f;
	clearVertices[1].y = -1.0f;
	clearVertices[2].x = -1.0f;
	clearVertices[2].y =  3.0f;

	clearIndices[0] = 0;
	clearIndices[1] = 1;
	clearIndices[2] = 2;

	const SceGxmProgramParameter *paramColorPositionAttribute = sceGxmProgramFindParameterByName(colorVertexProgramGxp, "aPosition");
	DEBUG("aPosition sceGxmProgramFindParameterByName(): %p\n", paramColorPositionAttribute);

	const SceGxmProgramParameter *paramColorColorAttribute = sceGxmProgramFindParameterByName(colorVertexProgramGxp, "aColor");
	DEBUG("aColor sceGxmProgramFindParameterByName(): %p\n", paramColorColorAttribute);

	// create color vertex format
	SceGxmVertexAttribute colorVertexAttributes[2];
	SceGxmVertexStream colorVertexStreams[1];
	/* x,y,z: 3 float 32 bits */
	colorVertexAttributes[0].streamIndex = 0;
	colorVertexAttributes[0].offset = 0;
	colorVertexAttributes[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	colorVertexAttributes[0].componentCount = 3; // (x, y, z)
	colorVertexAttributes[0].regIndex = sceGxmProgramParameterGetResourceIndex(paramColorPositionAttribute);
	/* color: 4 unsigned char  = 32 bits */
	colorVertexAttributes[1].streamIndex = 0;
	colorVertexAttributes[1].offset = 12; // (x, y, z) * 4 = 12 bytes
	colorVertexAttributes[1].format = SCE_GXM_ATTRIBUTE_FORMAT_U8N;
	colorVertexAttributes[1].componentCount = 4; // (color)
	colorVertexAttributes[1].regIndex = sceGxmProgramParameterGetResourceIndex(paramColorColorAttribute);
	// 16 bit (short) indices
	colorVertexStreams[0].stride = sizeof(vita2d_color_vertex);
	colorVertexStreams[0].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;

	// create color shaders
	err = sceGxmShaderPatcherCreateVertexProgram(
		shaderPatcher,
		colorVertexProgramId,
		colorVertexAttributes,
		2,
		colorVertexStreams,
		1,
		&_vita2d_colorVertexProgram);

	DEBUG("color sceGxmShaderPatcherCreateVertexProgram(): 0x%08X\n", err);

	err = sceGxmShaderPatcherCreateFragmentProgram(
		shaderPatcher,
		colorFragmentProgramId,
		SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		MSAA_MODE,
		&blend_info,
		colorVertexProgramGxp,
		&_vita2d_colorFragmentProgram);

	DEBUG("color sceGxmShaderPatcherCreateFragmentProgram(): 0x%08X\n", err);


	const SceGxmProgramParameter *paramTexturePositionAttribute = sceGxmProgramFindParameterByName(textureVertexProgramGxp, "aPosition");
	DEBUG("aPosition sceGxmProgramFindParameterByName(): %p\n", paramTexturePositionAttribute);

	const SceGxmProgramParameter *paramTextureTexcoordAttribute = sceGxmProgramFindParameterByName(textureVertexProgramGxp, "aTexcoord");
	DEBUG("aTexcoord sceGxmProgramFindParameterByName(): %p\n", paramTextureTexcoordAttribute);

	// create texture vertex format
	SceGxmVertexAttribute textureVertexAttributes[2];
	SceGxmVertexStream textureVertexStreams[1];
	/* x,y,z: 3 float 32 bits */
	textureVertexAttributes[0].streamIndex = 0;
	textureVertexAttributes[0].offset = 0;
	textureVertexAttributes[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	textureVertexAttributes[0].componentCount = 3; // (x, y, z)
	textureVertexAttributes[0].regIndex = sceGxmProgramParameterGetResourceIndex(paramTexturePositionAttribute);
	/* u,v: 2 floats 32 bits */
	textureVertexAttributes[1].streamIndex = 0;
	textureVertexAttributes[1].offset = 12; // (x, y, z) * 4 = 12 bytes
	textureVertexAttributes[1].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	textureVertexAttributes[1].componentCount = 2; // (u, v)
	textureVertexAttributes[1].regIndex = sceGxmProgramParameterGetResourceIndex(paramTextureTexcoordAttribute);
	// 16 bit (short) indices
	textureVertexStreams[0].stride = sizeof(vita2d_texture_vertex);
	textureVertexStreams[0].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;

	// create texture shaders
	err = sceGxmShaderPatcherCreateVertexProgram(
		shaderPatcher,
		textureVertexProgramId,
		textureVertexAttributes,
		2,
		textureVertexStreams,
		1,
		&_vita2d_textureVertexProgram);

	DEBUG("texture sceGxmShaderPatcherCreateVertexProgram(): 0x%08X\n", err);

	err = sceGxmShaderPatcherCreateFragmentProgram(
		shaderPatcher,
		textureFragmentProgramId,
		SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		MSAA_MODE,
		&blend_info,
		textureVertexProgramGxp,
		&_vita2d_textureFragmentProgram);

	DEBUG("texture sceGxmShaderPatcherCreateFragmentProgram(): 0x%08X\n", err);

	err = sceGxmShaderPatcherCreateFragmentProgram(
		shaderPatcher,
		textureTintFragmentProgramId,
		SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		MSAA_MODE,
		&blend_info,
		textureVertexProgramGxp,
		&_vita2d_textureTintFragmentProgram);

	DEBUG("texture_tint sceGxmShaderPatcherCreateFragmentProgram(): 0x%08X\n", err);

	// find vertex uniforms by name and cache parameter information
	_vita2d_clearClearColorParam = sceGxmProgramFindParameterByName(clearFragmentProgramGxp, "uClearColor");
	DEBUG("_vita2d_clearClearColorParam sceGxmProgramFindParameterByName(): %p\n", _vita2d_clearClearColorParam);

	_vita2d_colorWvpParam = sceGxmProgramFindParameterByName(colorVertexProgramGxp, "wvp");
	DEBUG("color wvp sceGxmProgramFindParameterByName(): %p\n", _vita2d_colorWvpParam);

	_vita2d_textureWvpParam = sceGxmProgramFindParameterByName(textureVertexProgramGxp, "wvp");
	DEBUG("texture wvp sceGxmProgramFindParameterByName(): %p\n", _vita2d_textureWvpParam);

	_vita2d_textureTintColorParam = sceGxmProgramFindParameterByName(textureTintFragmentProgramGxp, "uTintColor");
	DEBUG("texture wvp sceGxmProgramFindParameterByName(): %p\n", _vita2d_textureWvpParam);

	// Allocate memory for the memory pool
	pool_size = temp_pool_size;
	pool_addr = gpu_alloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW,
		pool_size,
		sizeof(void *),
		SCE_GXM_MEMORY_ATTRIB_READ,
		&poolUid);

	matrix_init_orthographic(_vita2d_ortho_matrix, 0.0f, DISPLAY_WIDTH, DISPLAY_HEIGHT, 0.0f, 0.0f, 1.0f);

	backBufferIndex = 0;
	frontBufferIndex = 0;

	pgf_module_was_loaded = sceSysmoduleIsLoaded(SCE_SYSMODULE_PGF);

	if (pgf_module_was_loaded != SCE_SYSMODULE_LOADED)
		sceSysmoduleLoadModule(SCE_SYSMODULE_PGF);

	vita2d_initialized = 1;
	return 1;
}

void vita2d_wait_rendering_done()
{
	sceGxmFinish(_vita2d_context);
}

int vita2d_fini()
{
	unsigned int i;

	if (!vita2d_initialized) {
		DEBUG("libvita2d is not initialized!\n");
		return 1;
	}

	// wait until rendering is done
	sceGxmFinish(_vita2d_context);

	// clean up allocations
	sceGxmShaderPatcherReleaseFragmentProgram(shaderPatcher, clearFragmentProgram);
	sceGxmShaderPatcherReleaseVertexProgram(shaderPatcher, clearVertexProgram);
	sceGxmShaderPatcherReleaseFragmentProgram(shaderPatcher, _vita2d_colorFragmentProgram);
	sceGxmShaderPatcherReleaseVertexProgram(shaderPatcher, _vita2d_colorVertexProgram);
	sceGxmShaderPatcherReleaseFragmentProgram(shaderPatcher, _vita2d_textureFragmentProgram);
	sceGxmShaderPatcherReleaseFragmentProgram(shaderPatcher, _vita2d_textureTintFragmentProgram);
	sceGxmShaderPatcherReleaseVertexProgram(shaderPatcher, _vita2d_textureVertexProgram);
	gpu_free(clearIndicesUid);
	gpu_free(clearVerticesUid);

	// wait until display queue is finished before deallocating display buffers
	sceGxmDisplayQueueFinish();

	// clean up display queue
	gpu_free(depthBufferUid);
	for (i = 0; i < DISPLAY_BUFFER_COUNT; i++) {
		// clear the buffer then deallocate
		memset(displayBufferData[i], 0, DISPLAY_HEIGHT*DISPLAY_STRIDE_IN_PIXELS*4);
		gpu_free(displayBufferUid[i]);

		// destroy the sync object
		sceGxmSyncObjectDestroy(displayBufferSync[i]);
	}

	// free the depth and stencil buffer
	gpu_free(depthBufferUid);
	gpu_free(stencilBufferUid);

	// unregister programs and destroy shader patcher
	sceGxmShaderPatcherUnregisterProgram(shaderPatcher, clearFragmentProgramId);
	sceGxmShaderPatcherUnregisterProgram(shaderPatcher, clearVertexProgramId);
	sceGxmShaderPatcherUnregisterProgram(shaderPatcher, colorFragmentProgramId);
	sceGxmShaderPatcherUnregisterProgram(shaderPatcher, colorVertexProgramId);
	sceGxmShaderPatcherUnregisterProgram(shaderPatcher, textureFragmentProgramId);
	sceGxmShaderPatcherUnregisterProgram(shaderPatcher, textureTintFragmentProgramId);
	sceGxmShaderPatcherUnregisterProgram(shaderPatcher, textureVertexProgramId);

	sceGxmShaderPatcherDestroy(shaderPatcher);
	fragment_usse_free(patcherFragmentUsseUid);
	vertex_usse_free(patcherVertexUsseUid);
	gpu_free(patcherBufferUid);

	// destroy the render target
	sceGxmDestroyRenderTarget(renderTarget);

	// destroy the _vita2d_context
	sceGxmDestroyContext(_vita2d_context);
	fragment_usse_free(fragmentUsseRingBufferUid);
	gpu_free(fragmentRingBufferUid);
	gpu_free(vertexRingBufferUid);
	gpu_free(vdmRingBufferUid);
	free(contextParams.hostMem);

	gpu_free(poolUid);

	// terminate libgxm
	sceGxmTerminate();

	/* if (pgf_module_was_loaded != SCE_SYSMODULE_LOADED)
		sceSysmoduleUnloadModule(SCE_SYSMODULE_PGF); */

	vita2d_initialized = 0;

	return 1;
}

void vita2d_clear_screen()
{
	// set clear shaders
	sceGxmSetVertexProgram(_vita2d_context, clearVertexProgram);
	sceGxmSetFragmentProgram(_vita2d_context, clearFragmentProgram);

	// set the clear color
	void *color_buffer;
	sceGxmReserveFragmentDefaultUniformBuffer(_vita2d_context, &color_buffer);
	sceGxmSetUniformDataF(color_buffer, _vita2d_clearClearColorParam, 0, 4, clear_color);

	// draw the clear triangle
	sceGxmSetVertexStream(_vita2d_context, 0, clearVertices);
	sceGxmDraw(_vita2d_context, SCE_GXM_PRIMITIVE_TRIANGLES, SCE_GXM_INDEX_FORMAT_U16, clearIndices, 3);
}

void vita2d_swap_buffers()
{
	sceGxmPadHeartbeat(&displaySurface[backBufferIndex], displayBufferSync[backBufferIndex]);

	// queue the display swap for this frame
	vita2d_display_data displayData;
	displayData.address = displayBufferData[backBufferIndex];
	sceGxmDisplayQueueAddEntry(
		displayBufferSync[frontBufferIndex],	// OLD fb
		displayBufferSync[backBufferIndex],	// NEW fb
		&displayData);

	// update buffer indices
	frontBufferIndex = backBufferIndex;
	backBufferIndex = (backBufferIndex + 1) % DISPLAY_BUFFER_COUNT;
}

void vita2d_start_drawing()
{
	/* Reset the temporary memory pool */
	vita2d_pool_reset();

	sceGxmBeginScene(
		_vita2d_context,
		0,
		renderTarget,
		NULL,
		NULL,
		displayBufferSync[backBufferIndex],
		&displaySurface[backBufferIndex],
		&depthSurface);
	
	drawing = 1;
	// in the current way, the library keeps the region clip across scenes
	if(clipping_enabled) {
		vita2d_set_clip_rectangle(clip_rect_x_min, clip_rect_y_min, clip_rect_x_max, clip_rect_y_max);
	}
}

void vita2d_end_drawing()
{
	sceGxmEndScene(_vita2d_context, NULL, NULL);
	drawing = 0;
}

void vita2d_enable_clipping()
{
	clipping_enabled = 1;
	vita2d_set_clip_rectangle(clip_rect_x_min, clip_rect_y_min, clip_rect_x_max, clip_rect_y_max);
}

void vita2d_disable_clipping()
{
	clipping_enabled = 0;
	sceGxmSetFrontStencilFunc(
			_vita2d_context,
			SCE_GXM_STENCIL_FUNC_ALWAYS,
			SCE_GXM_STENCIL_OP_KEEP,
			SCE_GXM_STENCIL_OP_KEEP,
			SCE_GXM_STENCIL_OP_KEEP,
			0xFF,
			0xFF);
}

int vita2d_get_clipping_enabled()
{
	return clipping_enabled;
}

void vita2d_set_clip_rectangle(int x_min, int y_min, int x_max, int y_max)
{
	clip_rect_x_min = x_min;
	clip_rect_y_min = y_min;
	clip_rect_x_max = x_max;
	clip_rect_y_max = y_max;
	// we can only draw during a scene, but we can cache the values since they're not going to have any visible effect till the scene starts anyways
	if(drawing) {
		// clear the stencil buffer to 0
		sceGxmSetFrontStencilFunc(
			_vita2d_context,
			SCE_GXM_STENCIL_FUNC_NEVER,
			SCE_GXM_STENCIL_OP_ZERO,
			SCE_GXM_STENCIL_OP_ZERO,
			SCE_GXM_STENCIL_OP_ZERO,
			0xFF,
			0xFF);
		vita2d_draw_rectangle(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, 0);
		// set the stencil to 1 in the desired region
		sceGxmSetFrontStencilFunc(
			_vita2d_context,
			SCE_GXM_STENCIL_FUNC_NEVER,
			SCE_GXM_STENCIL_OP_REPLACE,
			SCE_GXM_STENCIL_OP_REPLACE,
			SCE_GXM_STENCIL_OP_REPLACE,
			0xFF,
			0xFF);
		vita2d_draw_rectangle(x_min, y_min, x_max - x_min, y_max - y_min, 0);
		if(clipping_enabled) {
			// set the stencil function to only accept pixels where the stencil is 1
			sceGxmSetFrontStencilFunc(
				_vita2d_context,
				SCE_GXM_STENCIL_FUNC_EQUAL,
				SCE_GXM_STENCIL_OP_KEEP,
				SCE_GXM_STENCIL_OP_KEEP,
				SCE_GXM_STENCIL_OP_KEEP,
				0xFF,
				0xFF);
		} else {
			sceGxmSetFrontStencilFunc(
				_vita2d_context,
				SCE_GXM_STENCIL_FUNC_ALWAYS,
				SCE_GXM_STENCIL_OP_KEEP,
				SCE_GXM_STENCIL_OP_KEEP,
				SCE_GXM_STENCIL_OP_KEEP,
				0xFF,
				0xFF);
		}
	}
}

void vita2d_get_clip_rectangle(int *x_min, int *y_min, int *x_max, int *y_max)
{
	*x_min = clip_rect_x_min;
	*y_min = clip_rect_y_min;
	*x_max = clip_rect_x_max;
	*y_max = clip_rect_y_max;
}

int vita2d_common_dialog_update()
{
	SceCommonDialogUpdateParam updateParam;
	memset(&updateParam, 0, sizeof(updateParam));

	updateParam.renderTarget.colorFormat    = DISPLAY_COLOR_FORMAT;
	updateParam.renderTarget.surfaceType    = SCE_GXM_COLOR_SURFACE_LINEAR;
	updateParam.renderTarget.width          = DISPLAY_WIDTH;
	updateParam.renderTarget.height         = DISPLAY_HEIGHT;
	updateParam.renderTarget.strideInPixels = DISPLAY_STRIDE_IN_PIXELS;

	updateParam.renderTarget.colorSurfaceData = displayBufferData[backBufferIndex];
	updateParam.renderTarget.depthSurfaceData = depthBufferData;
	updateParam.displaySyncObject = displayBufferSync[backBufferIndex];

	return sceCommonDialogUpdate(&updateParam);
}

void vita2d_set_clear_color(unsigned int color)
{
	clear_color[0] = ((color >> 8*0) & 0xFF)/255.0f;
	clear_color[1] = ((color >> 8*1) & 0xFF)/255.0f;
	clear_color[2] = ((color >> 8*2) & 0xFF)/255.0f;
	clear_color[3] = ((color >> 8*3) & 0xFF)/255.0f;
}

void vita2d_set_vblank_wait(int enable)
{
	vblank_wait = enable;
}

void *vita2d_get_current_fb()
{
	return displayBufferData[frontBufferIndex];
}

void vita2d_set_region_clip(SceGxmRegionClipMode mode, unsigned int x_min, unsigned int y_min, unsigned int x_max, unsigned int y_max)
{
	sceGxmSetRegionClip(_vita2d_context, mode, x_min, y_min, x_max, y_max);
}

void *vita2d_pool_malloc(unsigned int size)
{
	if ((pool_index + size) < pool_size) {
		void *addr = (void *)((unsigned int)pool_addr + pool_index);
		pool_index += size;
		return addr;
	}
	return NULL;
}

void *vita2d_pool_memalign(unsigned int size, unsigned int alignment)
{
	unsigned int new_index = (pool_index + alignment - 1) & ~(alignment - 1);
	if ((new_index + size) < pool_size) {
		void *addr = (void *)((unsigned int)pool_addr + new_index);
		pool_index = new_index + size;
		return addr;
	}
	return NULL;
}

unsigned int vita2d_pool_free_space()
{
	return pool_size - pool_index;
}

void vita2d_pool_reset()
{
	pool_index = 0;
}
