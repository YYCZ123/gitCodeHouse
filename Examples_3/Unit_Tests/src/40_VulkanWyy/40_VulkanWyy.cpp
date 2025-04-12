// Interfaces
#include "../../../../Common_3/Application/Interfaces/IApp.h"
#include "../../../../Common_3/Application/Interfaces/ICameraController.h"
// Renderer
#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"

// Math
#include "../../../../Common_3/Utilities/Math/MathTypes.h"
#include <mutex>
// #include "../../../../Common_3/Utilities/Interfaces/IMemory.h"
#include "../../../../Common_3/Utilities/RingBuffer.h"
// fsl
#include "../../../../Common_3/Graphics/FSL/defaults.h"
#include "../../../../Common_3/Graphics/FSL/vulkan_srt.h"
#include "./Shaders/FSL/Global.srt.h"
#include "MeshCom.h"

#include <iostream>
#include <cstdlib>
#include <ctime>

#include <windows.h>
struct UniformBlock
{
    CameraMatrix mProjectView;
    mat4         mToWorldMat;
    // sun Light Information
    vec3         mLightDir;
    vec4         mLightColor;
    float        mLightIntensity;
    float        mLightAmbient;
};

// But we only need Two sets of resources (one in flight and one being used on CPU)
const uint32_t gDataBufferCount = 2;

uint32_t gImageCount = 3;

Renderer* pRenderer = nullptr;

Queue*   pGraphicsQueue = nullptr;
GpuCmdRing gGraphicsCmdRing = {};

SwapChain*    pSwapChain = nullptr;
RenderTarget* pDepthBuffer = nullptr;
Semaphore*    pImageAcquiredSemaphore = nullptr;
Shader* pTriangleShader = nullptr;

Pipeline* pTrianglePipeline = nullptr;

DescriptorSet*     pDescriptorSetUniforms = { NULL };
ICameraController* pCameraController = NULL;
Buffer*            pEnvironmentDataBuffer[gDataBufferCount] = { NULL };
UniformBlock       gEnvironmentData;
uint32_t           gFrameIndex = 0;
const char*        triangleImageName = "Skybox_top3.tex";
Sampler*           pSamplerTriangle = NULL;
Texture*           pTriangleTexture = NULL;
DescriptorSet*     pDescriptorSetTexture = { NULL };
QueryPool*         pPipelineStatsQueryPool[gDataBufferCount] = {};

struct StMeshObject
{
    MeshCom* meshCom;
    Box*     box;
};


uint16_t gIndex[] = { 0, 1, 2, 2, 3, 0 };

float gRander[] = { 0, 0.2, 0.35, 0.2, 0.5, 0.1, 0.7, 0.9, 0.8, 0.3 };

int        gRanderNum = 0;
static int RandIntRange(int nMin, int nMax) { return nMin + gRander[(gRanderNum++) % 10] * (nMax - nMin); }

class HelloWyy: public IApp
{
public:
    void prepareDescriptorSets() 
    {
        DescriptorData params[2] = {};

        params[0].mIndex = SRT_RES_IDX(SrtData, Persistent, gRightTexture);
        params[0].ppTextures = &pTriangleTexture;

        params[1].mIndex = SRT_RES_IDX(SrtData, Persistent, gSampler);
        params[1].ppSamplers = &pSamplerTriangle;
        updateDescriptorSet(pRenderer, 0, pDescriptorSetTexture, 2, params);

         for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            DescriptorData uParams[1] = {};
            uParams[0].mIndex = SRT_RES_IDX(SrtData, PerFrame, gUniformBlock);
            uParams[0].ppBuffers = &pEnvironmentDataBuffer[i];
            updateDescriptorSet(pRenderer, i, pDescriptorSetUniforms, 1, uParams);
        }

    }
    bool Init() override
    {
        // window and renderer setup
        RendererDesc settings;
        memset(&settings, 0, sizeof(settings));
        initGPUConfiguration(settings.pExtendedSettings);
        //settings.pContext = NULL;
        initRenderer(GetName(), &settings, &pRenderer);
        // check for init success
        if (!pRenderer)
        {
            ShowUnsupportedMessage("Failed To Initialize renderer!");
            return false;
        }
        setupGPUConfigurationPlatformParameters(pRenderer, settings.pExtendedSettings);

        if (!pRenderer)
            return false;

        if (pRenderer->pGpu->mPipelineStatsQueries)
        {
            QueryPoolDesc poolDesc = {};
            poolDesc.mQueryCount = 2; // The count is 3 due to quest & multi-view use otherwise 2 is enough as we use 2 queries.
            poolDesc.mType = QUERY_TYPE_PIPELINE_STATISTICS;
            for (uint32_t i = 0; i < gDataBufferCount; ++i)
            {
                initQueryPool(pRenderer, &poolDesc, &pPipelineStatsQueryPool[i]);
            }
        }

        QueueDesc queueDesc = {};
        queueDesc.mType = QUEUE_TYPE_GRAPHICS;
        queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
        initQueue(pRenderer, &queueDesc, &pGraphicsQueue);

        GpuCmdRingDesc cmdRingDesc = {};
        cmdRingDesc.pQueue = pGraphicsQueue;
        cmdRingDesc.mPoolCount = gDataBufferCount;
        cmdRingDesc.mCmdPerPoolCount = 1;
        cmdRingDesc.mAddSyncPrimitives = true;
        initGpuCmdRing(pRenderer, &cmdRingDesc, &gGraphicsCmdRing);

        initSemaphore(pRenderer, &pImageAcquiredSemaphore);

        initResourceLoaderInterface(pRenderer);

        RootSignatureDesc rootDesc = {};
        INIT_RS_DESC(rootDesc, "default.rootsig", "compute.rootsig");
        initRootSignature(pRenderer, &rootDesc);

        // Dynamic sampler that is bound at runtime
        SamplerDesc samplerDesc = { FILTER_LINEAR,
                                    FILTER_LINEAR,
                                    MIPMAP_MODE_NEAREST,
                                    ADDRESS_MODE_CLAMP_TO_EDGE,
                                    ADDRESS_MODE_CLAMP_TO_EDGE,
                                    ADDRESS_MODE_CLAMP_TO_EDGE };
        addSampler(pRenderer, &samplerDesc, &pSamplerTriangle);

        {
            TextureLoadDesc textureDesc = {};
            textureDesc.pFileName = triangleImageName;
            textureDesc.ppTexture = &pTriangleTexture;
            // Textures representing color should be stored in SRGB or HDR format
            textureDesc.mCreationFlag = TEXTURE_CREATION_FLAG_SRGB;
            addResource(&textureDesc, NULL);
        }

        gEnvironmentData.mLightColor = vec4(1.0f, 1.0f, 1.0f, 1.0f); // Pale Yellow
        auto dir = vec3(1.0f, 1.0f, 1.0f);
        gEnvironmentData.mLightDir = normalize(dir);
        gEnvironmentData.mLightIntensity = 3;
        gEnvironmentData.mLightAmbient = 0.2;

        uint64_t       camViewDataSize = sizeof(UniformBlock);
        BufferLoadDesc camViewVbDesc = {};
        camViewVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        camViewVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        camViewVbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        camViewVbDesc.mDesc.mSize = camViewDataSize;
        camViewVbDesc.pData = NULL;
        camViewVbDesc.mDesc.pName = "UniformData";
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            camViewVbDesc.ppBuffer = &pEnvironmentDataBuffer[i];
            addResource(&camViewVbDesc, NULL);
        }

        waitForAllResourceLoads();


        CameraMotionParameters cmp{ 160.0f, 600.0f, 200.0f, 0.1f, 1.1f };
        vec3                   camPos{ 20.0f, 20.0f, 20.0f };
        vec3                   lookAt{ vec3(0) };

        pCameraController = initFpsCameraController(camPos, lookAt);

        pCameraController->setMotionParameters(cmp);

        AddCustomInputBindings();
        gFrameIndex = 0;

        return true;
    }
    void Exit() override
    {
        exitCameraController(pCameraController);

        waitQueueIdle(pGraphicsQueue);

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            removeResource(pEnvironmentDataBuffer[i]);
            if (pRenderer->pGpu->mPipelineStatsQueries)
            {
                exitQueryPool(pRenderer, pPipelineStatsQueryPool[i]);
            }
        }

        int meshSize = m_meshList.size();
        if (meshSize != 0)
        {
            for (int i = 0; i < meshSize; i++)
            {
                m_meshList[i].meshCom->Remove();
            }
            m_meshList.clear();
        }
        removeResource(pTriangleTexture);
        removeSampler(pRenderer, pSamplerTriangle);
        removeDescriptorSet(pRenderer, pDescriptorSetTexture);
        removeDescriptorSet(pRenderer, pDescriptorSetUniforms);
        

        
        removeShader(pRenderer, pTriangleShader);

        exitGpuCmdRing(pRenderer, &gGraphicsCmdRing);
        exitSemaphore(pRenderer, pImageAcquiredSemaphore);

        exitRootSignature(pRenderer);
        exitResourceLoaderInterface(pRenderer);
        exitQueue(pRenderer, pGraphicsQueue);
        exitRenderer(pRenderer);
        exitGPUConfiguration();
        pRenderer = NULL;
    }

    bool Load(ReloadDesc* pReloadDesc) override
    {
        if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
        {
            ShaderLoadDesc triangleShaderLoadDesc = {};
            triangleShaderLoadDesc.mVert = { "tri.vert" };
            triangleShaderLoadDesc.mFrag = { "tri.frag" };
            addShader(pRenderer, &triangleShaderLoadDesc, &pTriangleShader);

            DescriptorSetDesc descPersisent = SRT_SET_DESC(SrtData, Persistent, 1, 0);
            addDescriptorSet(pRenderer, &descPersisent, &pDescriptorSetTexture);

            DescriptorSetDesc desc = SRT_SET_DESC(SrtData, PerFrame, gDataBufferCount, 0);
            addDescriptorSet(pRenderer, &desc, &pDescriptorSetUniforms);
            

        }
        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
        {
            if (!addSwapChain())
                return false;

            if (!addDepthBuffer())
                return false;
        }
        if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
        {
            //addPipelines();
            {
                VertexLayout vertexLayout = {};
                vertexLayout.mAttribCount = 3;
                vertexLayout.mBindingCount = 1;
                vertexLayout.mBindings[0].mRate = VERTEX_BINDING_RATE_VERTEX;
                vertexLayout.mBindings[0].mStride = 0;
                vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
                vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
                vertexLayout.mAttribs[0].mBinding = 0;
                vertexLayout.mAttribs[0].mLocation = 0;
                vertexLayout.mAttribs[0].mOffset = 0;
                vertexLayout.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
                vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
                vertexLayout.mAttribs[1].mBinding = 0;
                vertexLayout.mAttribs[1].mLocation = 1;
                vertexLayout.mAttribs[1].mOffset = 3 * sizeof(float);
                vertexLayout.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
                vertexLayout.mAttribs[2].mFormat = TinyImageFormat_R32G32_SFLOAT;
                vertexLayout.mAttribs[2].mBinding = 0;
                vertexLayout.mAttribs[2].mLocation = 2;
                vertexLayout.mAttribs[2].mOffset = 6 * sizeof(float);

                RasterizerStateDesc rasterizerStateDesc = {};
                rasterizerStateDesc.mCullMode = CULL_MODE_NONE;

                DepthStateDesc depthStateDesc = {};
                depthStateDesc.mDepthTest = true;
                depthStateDesc.mDepthWrite = true;
                depthStateDesc.mDepthFunc = CMP_GEQUAL;

                PipelineDesc pipelineDesc = {};
                pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
                PIPELINE_LAYOUT_DESC(pipelineDesc, SRT_LAYOUT_DESC(SrtData, Persistent), SRT_LAYOUT_DESC(SrtData, PerFrame), NULL, NULL);
                GraphicsPipelineDesc& graphicsPipelineDesc = pipelineDesc.mGraphicsDesc;
                graphicsPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
                graphicsPipelineDesc.mRenderTargetCount = 1;
                graphicsPipelineDesc.pDepthState = &depthStateDesc;
                graphicsPipelineDesc.mDepthStencilFormat = pDepthBuffer->mFormat;
                graphicsPipelineDesc.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
                graphicsPipelineDesc.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
                graphicsPipelineDesc.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
                graphicsPipelineDesc.pShaderProgram = pTriangleShader;
                graphicsPipelineDesc.pVertexLayout = &vertexLayout;
                graphicsPipelineDesc.pRasterizerState = &rasterizerStateDesc;
                addPipeline(pRenderer, &pipelineDesc, &pTrianglePipeline);
            }
        }

        prepareDescriptorSets();
        return true;
    }

    void Unload(ReloadDesc* pReloadDesc) override
    {
        waitQueueIdle(pGraphicsQueue);
        if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
        {
            removePipeline(pRenderer, pTrianglePipeline);
        }
        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
        {
            removeSwapChain(pRenderer, pSwapChain);
            removeRenderTarget(pRenderer, pDepthBuffer);
        }
    }

    void Update(float deltaTime) override
    {
        pCameraController->onMove({ inputGetValue(0, CUSTOM_MOVE_X), inputGetValue(0, CUSTOM_MOVE_Y) });
        pCameraController->onRotate({ inputGetValue(0, CUSTOM_LOOK_X), inputGetValue(0, CUSTOM_LOOK_Y) });
        pCameraController->onMoveY(inputGetValue(0, CUSTOM_MOVE_UP));
        pCameraController->onZoom({ inputGetValue(0, CUSTOM_ZOOM), inputGetValue(0, CUSTOM_ZOOM) });
        if (inputGetValue(0, CUSTOM_RESET_VIEW))
        {
            pCameraController->resetView();
        }

        pCameraController->update(deltaTime);

        /************************************************************************/
        // Scene Update
        /************************************************************************/
        static float currentTime = 0.0f;
        currentTime += deltaTime * 1000.0f;

        // update camera with time
        mat4 viewMat = pCameraController->getViewMatrix();

        const float  aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
        const float  horizontal_fov = PI / 2.0f;
        CameraMatrix projMat = CameraMatrix::perspectiveReverseZ(horizontal_fov, aspectInverse, 0.1f, 1000.0f);
        gEnvironmentData.mProjectView = projMat * viewMat;
        gEnvironmentData.mToWorldMat = inverse(viewMat);
        g_RenderNum++;
        if (g_RenderNum == 4000)
        {
            CreatMesh();
        }
    }

    void Draw() override
    {
        uint32_t swapchainImageIndex;
        acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, nullptr, &swapchainImageIndex);

        RenderTarget* renderTarget = pSwapChain->ppRenderTargets[swapchainImageIndex];
        GpuCmdRingElement elem = getNextGpuCmdRingElement(&gGraphicsCmdRing, true, 1);

        //Semaphore*    renderCompleteSemaphore = pRenderCompleteSemaphores[swapchainImageIndex];
        //Fence*        renderCompleteFence = pRenderCompleteFences[swapchainImageIndex];

        // Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
        FenceStatus fenceStatus;
        getFenceStatus(pRenderer, elem.pFence, &fenceStatus);
        if (fenceStatus == FENCE_STATUS_INCOMPLETE)
            waitForFences(pRenderer, 1, &elem.pFence);

        // Update uniform buffers
        m_meshMutx.lock();
        int meshSize = m_meshList.size();
        if (!g_MeshLoaded && meshSize != 0)
        {
            bool bLoaded = true;
            for (int i = 0; i < meshSize; i++)
            {
                if (m_meshList[i].meshCom->IsLoaded())
                {
                    m_meshList[i].meshCom->UpdateRes();
                }
                else
                {
                    bLoaded = false;
                    break;
                }
            }
            g_MeshLoaded = bLoaded;
        }
        m_meshMutx.unlock();
        BufferUpdateDesc viewProjCbv = { pEnvironmentDataBuffer[gFrameIndex] };
        beginUpdateResource(&viewProjCbv);
        memcpy(viewProjCbv.pMappedData, &gEnvironmentData, sizeof(UniformBlock));
        endUpdateResource(&viewProjCbv);

         // Reset cmd pool for this frame
        resetCmdPool(pRenderer, elem.pCmdPool);

        Cmd* cmd = elem.pCmds[0];
        beginCmd(cmd);

        if (pRenderer->pGpu->mPipelineStatsQueries)
        {
            cmdResetQuery(cmd, pPipelineStatsQueryPool[gFrameIndex], 0, 2);
            QueryDesc queryDesc = { 0 };
            cmdBeginQuery(cmd, pPipelineStatsQueryPool[gFrameIndex], &queryDesc);
        }

        RenderTargetBarrier barriers[] = {
            { renderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
        };
        cmdResourceBarrier(cmd, 0, nullptr, 0, nullptr, 1, barriers);

        // simply record the screen cleaning command
        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { renderTarget, LOAD_ACTION_CLEAR };
        bindRenderTargets.mDepthStencil = { pDepthBuffer, LOAD_ACTION_CLEAR };
        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)renderTarget->mWidth, (float)renderTarget->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, renderTarget->mWidth, renderTarget->mHeight);
        
        // draw triangle
        cmdBindPipeline(cmd, pTrianglePipeline);
        cmdBindDescriptorSet(cmd, 0, pDescriptorSetTexture);
        cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetUniforms);
        m_meshMutx.lock();
        // int meshSize = m_meshList.size();
        if (g_MeshLoaded && meshSize != 0)
        {
            for (int i = 0; i < meshSize; i++)
            {
                m_meshList[i].meshCom->DrawMesh(cmd);
            }
        }
        m_meshMutx.unlock();

        cmdBindRenderTargets(cmd, NULL);

         if (pRenderer->pGpu->mPipelineStatsQueries)
        {
            QueryDesc queryDesc = { 0 };
            cmdEndQuery(cmd, pPipelineStatsQueryPool[gFrameIndex], &queryDesc);

            queryDesc = { 1 };
            cmdBeginQuery(cmd, pPipelineStatsQueryPool[gFrameIndex], &queryDesc);
        }

        barriers[0] = { renderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
        cmdResourceBarrier(cmd, 0, nullptr, 0, nullptr, 1, barriers);

        if (pRenderer->pGpu->mPipelineStatsQueries)
        {
            QueryDesc queryDesc = { 1 };
            cmdEndQuery(cmd, pPipelineStatsQueryPool[gFrameIndex], &queryDesc);
            cmdResolveQuery(cmd, pPipelineStatsQueryPool[gFrameIndex], 0, 2);
        }

        endCmd(cmd);

        QueueSubmitDesc submitDesc = {};
        submitDesc.mCmdCount = 1;
        submitDesc.mSignalSemaphoreCount = 1;
        submitDesc.mWaitSemaphoreCount = 1;
        submitDesc.ppCmds = &cmd;
        submitDesc.ppSignalSemaphores = &elem.pSemaphore;
        submitDesc.ppWaitSemaphores = &pImageAcquiredSemaphore;
        submitDesc.pSignalFence = elem.pFence;
        queueSubmit(pGraphicsQueue, &submitDesc);

        QueuePresentDesc presentDesc = {};
        presentDesc.mIndex = (uint8_t)swapchainImageIndex;
        presentDesc.mWaitSemaphoreCount = 1;
        presentDesc.pSwapChain = pSwapChain;
        presentDesc.ppWaitSemaphores = &elem.pSemaphore;
        presentDesc.mSubmitDone = true;
        queuePresent(pGraphicsQueue, &presentDesc);

        gFrameIndex = (gFrameIndex + 1) % gDataBufferCount;
        // Sleep(2000);
    }

    const char* GetName() override { return "WindowsWyy"; }

    bool addSwapChain()
    {
        SwapChainDesc swapChainDesc = {};
        swapChainDesc.mWindowHandle = pWindow->handle;
        swapChainDesc.mPresentQueueCount = 1;
        swapChainDesc.ppPresentQueues = &pGraphicsQueue;
        swapChainDesc.mWidth = mSettings.mWidth;
        swapChainDesc.mHeight = mSettings.mHeight;
        gImageCount = getRecommendedSwapchainImageCount(pRenderer, &pWindow->handle);
        swapChainDesc.mImageCount = gImageCount;
        ;
        swapChainDesc.mColorFormat = getSupportedSwapchainFormat(pRenderer, &swapChainDesc, COLOR_SPACE_SDR_SRGB);
        swapChainDesc.mEnableVsync = mSettings.mVSyncEnabled;
        ::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

        return pSwapChain != nullptr;
    }

    bool addDepthBuffer()
    {
        // Add depth buffer
        RenderTargetDesc depthRT = {};
        depthRT.mArraySize = 1;
        depthRT.mClearValue.depth = 0.0f;
        depthRT.mClearValue.stencil = 0;
        depthRT.mDepth = 1;
        depthRT.mFormat = TinyImageFormat_D32_SFLOAT;
        depthRT.mStartState = RESOURCE_STATE_DEPTH_WRITE;
        depthRT.mHeight = mSettings.mHeight;
        depthRT.mSampleCount = SAMPLE_COUNT_1;
        depthRT.mSampleQuality = 0;
        depthRT.mWidth = mSettings.mWidth;
        depthRT.mFlags = TEXTURE_CREATION_FLAG_ON_TILE | TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
        addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);

        return pDepthBuffer != NULL;
    }

    void CreatMesh()
    {
        // m_meshMutx.lock();
        for (int i = 0; i < 50; i++)
        {
            StMeshObject obj;
            Box::Param   boxParam;
            std::srand(std::time(0));

            // 生成0到99之间的随机整数
            int randomNumber = std::rand() % 100;

            boxParam.posX = std::rand() % 10;
            boxParam.posY = std::rand() % 10;
            boxParam.posZ = std::rand() % 10;
            // boxParam.posX = -2.5;
            // boxParam.posY = -2.5;
            // boxParam.posZ = -2.5;
            boxParam.xLength = 1;
            boxParam.yLength = 1;
            boxParam.zLength = 1;
            //boxParam.zLength = RandIntRange(1, 10);
            MeshCom::MeshParam meshParam;
            obj.box = new Box();
            obj.box->GetMeshParam(meshParam, boxParam);
            obj.meshCom = new MeshCom();
            obj.meshCom->m_posBuffer.m_syncToken = 2 * i;
            obj.meshCom->m_indexBuffer.m_syncToken = 2 * i + 1;

            if (obj.meshCom->Init(meshParam))
            {
                m_meshList.emplace_back(obj);
            }
        }
        // m_meshMutx.unlock();
    }

private:
    std::vector<StMeshObject> m_meshList;
    // MeshCom                   meshCom;
    // Box                       box;
    int                       g_RenderNum = 0;
    bool                      g_MeshLoaded = false;
    std::mutex                m_meshMutx;
};

DEFINE_APPLICATION_MAIN(HelloWyy);
  