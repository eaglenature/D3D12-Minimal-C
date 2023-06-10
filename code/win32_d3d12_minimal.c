// Minimal example of how to set up D3D12 rendering on Windows in C

#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#pragma warning(push)
#pragma warning(disable:4115) // Supress 'ID3D11ModuleInstance': named type definition in parentheses
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#pragma warning(pop)

#include <stdint.h>
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;
typedef float    f32;
typedef double   f64;
typedef uint32_t b32;

#define AssertBreak() (*(volatile int *)0 = 0)
#define Assert(Expression) if(!(Expression)) { AssertBreak(); }
#define AssertHR(HResult) Assert(SUCCEEDED(HResult))
#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

#define DEBUG_ENABLED 1

static LRESULT CALLBACK WindowProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    switch(Message)
    {
        case WM_KEYDOWN:
        {
            if(WParam == VK_ESCAPE)
            {
                PostQuitMessage(0);
                return 0;
            }
            break;
        }        
        case WM_CLOSE:
        case WM_DESTROY:
        {
            PostQuitMessage(0);
            return 0;
        }
        default:
        {
            break;
        }
    }
    return DefWindowProc(Window, Message, WParam, LParam);
}

int WINAPI WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, PSTR CommandLine, INT ShowCode)
{
    // Create system window

    b32 SmallWindow = 0;
    b32 Borderless  = 1;
    u32 ResX = SmallWindow ? 960 : 1280;
    u32 ResY = SmallWindow ? 540 : 720;

    HWND Window = NULL;

    {
        WNDCLASSEX WindowClass = {0};
        WindowClass.cbSize        = sizeof(WNDCLASSEX);
        WindowClass.style         = CS_HREDRAW|CS_VREDRAW;
        WindowClass.lpfnWndProc   = WindowProc;
        WindowClass.hInstance     = Instance;
        WindowClass.hCursor       = LoadCursor(0, IDC_ARROW);
        WindowClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        WindowClass.lpszClassName = "D3D12WindowClass";

        ATOM RegistrationResult = RegisterClassEx(&WindowClass);
        Assert(RegistrationResult && "Failed to register window class");

        RECT WindowRectangle = {0};
        WindowRectangle.right  = ResX;
        WindowRectangle.bottom = ResY;

        POINT ScreenCenter = {0};
        ScreenCenter.x = (GetSystemMetrics(SM_CXSCREEN) - WindowRectangle.right)/2;
        ScreenCenter.y = (GetSystemMetrics(SM_CYSCREEN) - WindowRectangle.bottom)/2;

        DWORD ExtendedWindowStyle = 0;
        DWORD WindowStyle = WS_OVERLAPPEDWINDOW;
        if(Borderless)
        {
            WindowStyle = WS_EX_TOPMOST|WS_POPUP;
        }
        AdjustWindowRect(&WindowRectangle, WindowStyle, FALSE);

        Window = CreateWindowExA(
            ExtendedWindowStyle,
            WindowClass.lpszClassName,
            "D3D12 Window",
            WindowStyle,
            ScreenCenter.x,
            100,
            WindowRectangle.right  - WindowRectangle.left,
            WindowRectangle.bottom - WindowRectangle.top,
            NULL, NULL,
            WindowClass.hInstance,
            NULL);

        Assert(Window && "Failed to create window");
    }

    //------------------------------------------------------------------------
    //

    HRESULT Result;

    #if DEBUG_ENABLED
    ID3D12Debug *DebugInterface = NULL;

    {
        Result = D3D12GetDebugInterface(&IID_ID3D12Debug, &DebugInterface);
        AssertHR(Result);

        ID3D12Debug_EnableDebugLayer(DebugInterface);
    }
    #endif


    IDXGIFactory4 *Factory = NULL;

    {
        UINT Flags = 0;
        #if DEBUG_ENABLED
        {
            Flags |= DXGI_CREATE_FACTORY_DEBUG;
        }
        #endif

        Result = CreateDXGIFactory2(Flags, &IID_IDXGIFactory4, &Factory);
        AssertHR(Result);
    }



    // Enumerate hardware that support Direct3D 12.1
    D3D_FEATURE_LEVEL MaximumFeatureLevel = D3D_FEATURE_LEVEL_12_1;
    D3D_FEATURE_LEVEL MinimumFeatureLevel = MaximumFeatureLevel;

    IDXGIAdapter1 *Adapter = NULL;

    {
        for(u32 AdapterIndex = 0; DXGI_ERROR_NOT_FOUND != IDXGIFactory1_EnumAdapters1(Factory, AdapterIndex, &Adapter); ++AdapterIndex)
        {
            DXGI_ADAPTER_DESC1 AdapterDesc = {0};
            IDXGIAdapter1_GetDesc1(Adapter, &AdapterDesc);

            if(AdapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;

            Result = D3D12CreateDevice((IUnknown*)Adapter, MinimumFeatureLevel, &IID_ID3D12Device, NULL);
            if(SUCCEEDED(Result))
            {
                break;
            }
        }

        Assert(Adapter && "No hardware adapter found");
    }

    // Create device
    ID3D12Device* Device = NULL;

    {
        Result = D3D12CreateDevice((IUnknown*)Adapter, MinimumFeatureLevel, &IID_ID3D12Device, &Device);
        AssertHR(Result);
    }


    // Check feature support (just load not use yet)
    {
        D3D12_FEATURE_DATA_D3D12_OPTIONS FeatureInfo = {0};
        Result = ID3D12Device_CheckFeatureSupport(Device, D3D12_FEATURE_D3D12_OPTIONS, &FeatureInfo, sizeof(FeatureInfo));
        AssertHR(Result);
    }
    {
        D3D12_FEATURE_DATA_ARCHITECTURE FeatureInfo = {0};
        Result = ID3D12Device_CheckFeatureSupport(Device, D3D12_FEATURE_ARCHITECTURE, &FeatureInfo, sizeof(FeatureInfo));
        AssertHR(Result);

        // https://therealmjp.github.io/posts/gpu-memory-pool/#how-it-works-in-d3d12
        // The UMA flag tells you if the GPU uses a Unified Memory Architecture where a single physical pool of RAM
        // is shared between the CPU and GPU, like what you’d normally find on an integrated GPU
        //Assert(FeatureInfo.UMA == 0);

        // If UMA is true and CacheCoherentUMA is also true, then the CPU and GPU caches are fully coherent with each other.
        // In this situation using uncached writes for filling GPU-visible SysRAM does not make sense,
        // since the GPU can benefit from data being resident in cache.
        //Assert(FeatureInfo.CacheCoherentUMA == 0);
    }
    {
        D3D12_FEATURE_DATA_SHADER_MODEL FeatureInfo = {0};
        FeatureInfo.HighestShaderModel = D3D_SHADER_MODEL_6_5;
        Result = ID3D12Device_CheckFeatureSupport(Device, D3D12_FEATURE_SHADER_MODEL, &FeatureInfo, sizeof(FeatureInfo));
        AssertHR(Result);

        // https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d_shader_model
        // D3D_SHADER_MODEL_6_4 : required for DirectX Raytracing (DXR)
        // D3D_SHADER_MODEL_6_5 : required for Direct Machine Learning
        //Assert(FeatureInfo.HighestShaderModel == D3D_SHADER_MODEL_6_5);
    }


    // Create direct command queue
    ID3D12CommandQueue *DirectQueue = NULL;

    {
        D3D12_COMMAND_QUEUE_DESC CommandQueueDesc = {0};
        CommandQueueDesc.Type     = D3D12_COMMAND_LIST_TYPE_DIRECT;
        CommandQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        CommandQueueDesc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
        CommandQueueDesc.NodeMask = 0;

        Result = ID3D12Device_CreateCommandQueue(Device, &CommandQueueDesc, &IID_ID3D12CommandQueue, &DirectQueue);
        AssertHR(Result);
    }


    // Create double buffered SwapChain
    IDXGISwapChain4 *SwapChain = NULL;
    ID3D12Resource *RenderTargets[2] = {0};

    {
        DXGI_SWAP_CHAIN_DESC1 SwapChainDesc = {0};
        SwapChainDesc.Width              = ResX;
        SwapChainDesc.Height             = ResY;
        SwapChainDesc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
        SwapChainDesc.Stereo             = FALSE;
        SwapChainDesc.SampleDesc.Count   = 1;
        SwapChainDesc.SampleDesc.Quality = 0;
        SwapChainDesc.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        SwapChainDesc.BufferCount        = ArrayCount(RenderTargets);
        SwapChainDesc.Scaling            = DXGI_SCALING_NONE;
        SwapChainDesc.SwapEffect         = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        SwapChainDesc.AlphaMode          = DXGI_ALPHA_MODE_UNSPECIFIED;
        SwapChainDesc.Flags              = 0;

        Result = IDXGIFactory4_CreateSwapChainForHwnd(Factory, (IUnknown*)DirectQueue, Window, &SwapChainDesc, NULL, NULL, (IDXGISwapChain1**)&SwapChain);
        AssertHR(Result);
    }

    // This sample does not support fullscreen transitions
    IDXGIFactory4_MakeWindowAssociation(Factory, Window, DXGI_MWA_NO_ALT_ENTER);

    u32 FrameIndex = IDXGISwapChain3_GetCurrentBackBufferIndex(SwapChain);

    // Create render target descriptor heaps
    ID3D12DescriptorHeap *RtvDescriptorHeap = NULL;

    {
        D3D12_DESCRIPTOR_HEAP_DESC DescriptorHeapDesc = {0};
        DescriptorHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        DescriptorHeapDesc.NumDescriptors = ArrayCount(RenderTargets);
        DescriptorHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        DescriptorHeapDesc.NodeMask       = 0;

        Result = ID3D12Device_CreateDescriptorHeap(Device, &DescriptorHeapDesc, &IID_ID3D12DescriptorHeap, &RtvDescriptorHeap);
        AssertHR(Result);
    }

    // Create render targets for each frame
    {
        u32 RtvDescriptorSize = ID3D12Device_GetDescriptorHandleIncrementSize(Device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        // GetCPUDescriptorHandleForHeapStart declaration is broken in Windows C interface.
        // The result should be passed as an output parameter, rather than a return value.
        // https://joshstaiger.org/notes/C-Language-Problems-in-Direct3D-12-GetCPUDescriptorHandleForHeapStart.html
        typedef void(get_cpu_descriptor_handle_for_heap_start)(ID3D12DescriptorHeap*, D3D12_CPU_DESCRIPTOR_HANDLE*);
        get_cpu_descriptor_handle_for_heap_start *GetCPUDescriptorHandleForHeapStart = (get_cpu_descriptor_handle_for_heap_start *)RtvDescriptorHeap->lpVtbl->GetCPUDescriptorHandleForHeapStart;

        D3D12_CPU_DESCRIPTOR_HANDLE RtvDescriptorHandle = {0};
        GetCPUDescriptorHandleForHeapStart(RtvDescriptorHeap, &RtvDescriptorHandle);

        for(u32 RenderTargetIndex = 0; RenderTargetIndex < ArrayCount(RenderTargets); ++RenderTargetIndex)
        {
            Result = IDXGISwapChain1_GetBuffer(SwapChain, RenderTargetIndex, &IID_ID3D12Resource, &RenderTargets[RenderTargetIndex]);
            AssertHR(Result);

            ID3D12Device_CreateRenderTargetView(Device, RenderTargets[RenderTargetIndex], NULL, RtvDescriptorHandle);
            RtvDescriptorHandle.ptr += RtvDescriptorSize;
        }
    }

    // Create command allocator
    ID3D12CommandAllocator *DirectQueueCommandAllocator = NULL;

    {
        Result = ID3D12Device_CreateCommandAllocator(Device, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, &DirectQueueCommandAllocator);
        AssertHR(Result);
    }


    //------------------------------------------------------------------------
    //

    // Create an empty root signature
    ID3D12RootSignature *RootSignature = NULL;

    {
        D3D12_VERSIONED_ROOT_SIGNATURE_DESC RootSignatureDesc = {0};
        RootSignatureDesc.Version        = D3D_ROOT_SIGNATURE_VERSION_1_0;
        RootSignatureDesc.Desc_1_0.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ID3DBlob* SerializedRootSignature = NULL;
        Result = D3D12SerializeVersionedRootSignature(&RootSignatureDesc, &SerializedRootSignature, NULL);
        AssertHR(Result);

        Result = ID3D12Device_CreateRootSignature(Device, 0, ID3D10Blob_GetBufferPointer(SerializedRootSignature), ID3D10Blob_GetBufferSize(SerializedRootSignature), &IID_ID3D12RootSignature, &RootSignature);
        AssertHR(Result);

        ID3D10Blob_Release(SerializedRootSignature);
    }


    // Create pipeline state object (PSO)
    ID3D12PipelineState *PSO = NULL;

    {
        ID3DBlob *VertexShader = NULL;
        ID3DBlob *PixelShader  = NULL;

        const char ShaderSource[] =
            "struct PSInput\n"
            "{\n"
            "   float4 position : SV_POSITION;\n"
            "   float4 color : COLOR;\n"
            "};\n"
            "PSInput VSMain(float4 position : POSITION0, float4 color : COLOR0)\n"
            "{\n"
            "   PSInput result;\n"
            "   result.position = position;\n"
            "   result.color = color;\n"
            "   return result;\n"
            "}\n"
            "float4 PSMain(PSInput input) : SV_TARGET\n"
            "{\n"
            "   return input.color;\n"
            "}\n"; 
            
        u32 CompilationFlags = 0;
        #if DEBUG_ENABLED
        {
            CompilationFlags |= D3DCOMPILE_DEBUG;
            CompilationFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
        }
        #endif

        Result = D3DCompile(ShaderSource, ArrayCount(ShaderSource), NULL, NULL, NULL, "VSMain", "vs_5_0", CompilationFlags, 0, &VertexShader, NULL);
        AssertHR(Result);

        Result = D3DCompile(ShaderSource, ArrayCount(ShaderSource), NULL, NULL, NULL, "PSMain", "ps_5_0", CompilationFlags, 0, &PixelShader, NULL);
        AssertHR(Result);

        D3D12_INPUT_ELEMENT_DESC InputElementDescs[] =
        {
            {
                .SemanticName         = "POSITION",
                .SemanticIndex        = 0,
                .Format               = DXGI_FORMAT_R32G32B32_FLOAT,
                .InputSlot            = 0,
                .AlignedByteOffset    = 0,
                .InputSlotClass       = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                .InstanceDataStepRate = 0
            },
            {
                .SemanticName         = "COLOR",
                .SemanticIndex        = 0,
                .Format               = DXGI_FORMAT_R32G32B32A32_FLOAT,
                .InputSlot            = 0,
                .AlignedByteOffset    = sizeof(f32) * 3,
                .InputSlotClass       = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                .InstanceDataStepRate = 0
            }
        };

        D3D12_RENDER_TARGET_BLEND_DESC DefaultBlendState = {0};
        DefaultBlendState.BlendEnable           = FALSE;
        DefaultBlendState.LogicOpEnable         = FALSE;
        DefaultBlendState.SrcBlend              = D3D12_BLEND_ONE;
        DefaultBlendState.DestBlend             = D3D12_BLEND_ZERO;
        DefaultBlendState.BlendOp               = D3D12_BLEND_OP_ADD;
        DefaultBlendState.SrcBlendAlpha         = D3D12_BLEND_ONE;
        DefaultBlendState.DestBlendAlpha        = D3D12_BLEND_ZERO;
        DefaultBlendState.BlendOpAlpha          = D3D12_BLEND_OP_ADD;
        DefaultBlendState.LogicOp               = D3D12_LOGIC_OP_NOOP;
        DefaultBlendState.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        // Describe and create graphics pipeline state object (PSO).
        D3D12_GRAPHICS_PIPELINE_STATE_DESC PsoDesc = {0};
        PsoDesc.pRootSignature                        = RootSignature;
        PsoDesc.VS.pShaderBytecode                    = ID3D10Blob_GetBufferPointer(VertexShader);
        PsoDesc.VS.BytecodeLength                     = ID3D10Blob_GetBufferSize(VertexShader);
        PsoDesc.PS.pShaderBytecode                    = ID3D10Blob_GetBufferPointer(PixelShader);
        PsoDesc.PS.BytecodeLength                     = ID3D10Blob_GetBufferSize(PixelShader);
        PsoDesc.DS.pShaderBytecode                    = NULL;
        PsoDesc.DS.BytecodeLength                     = 0;
        PsoDesc.HS.pShaderBytecode                    = NULL;
        PsoDesc.HS.BytecodeLength                     = 0;
        PsoDesc.GS.pShaderBytecode                    = NULL;
        PsoDesc.GS.BytecodeLength                     = 0;
        PsoDesc.BlendState.AlphaToCoverageEnable      = FALSE;
        PsoDesc.BlendState.IndependentBlendEnable     = FALSE;
        PsoDesc.BlendState.RenderTarget[0]            = DefaultBlendState;
        PsoDesc.SampleMask                            = 0xFFFFFFFF;
        PsoDesc.RasterizerState.FillMode              = D3D12_FILL_MODE_SOLID;
        PsoDesc.RasterizerState.CullMode              = D3D12_CULL_MODE_BACK;
        PsoDesc.RasterizerState.FrontCounterClockwise = FALSE;
        PsoDesc.RasterizerState.DepthBias             = 0;
        PsoDesc.RasterizerState.DepthBiasClamp        = 0;
        PsoDesc.RasterizerState.SlopeScaledDepthBias  = 0;
        PsoDesc.RasterizerState.DepthClipEnable       = TRUE;
        PsoDesc.RasterizerState.MultisampleEnable     = FALSE;
        PsoDesc.RasterizerState.AntialiasedLineEnable = FALSE;
        PsoDesc.RasterizerState.ForcedSampleCount     = 0;
        PsoDesc.RasterizerState.ConservativeRaster    = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
        PsoDesc.DepthStencilState.DepthEnable         = FALSE;
        PsoDesc.DepthStencilState.StencilEnable       = FALSE;
        PsoDesc.InputLayout.pInputElementDescs        = InputElementDescs;
        PsoDesc.InputLayout.NumElements               = ArrayCount(InputElementDescs);
        PsoDesc.PrimitiveTopologyType                 = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        PsoDesc.NumRenderTargets                      = 1;
        PsoDesc.RTVFormats[0]                         = DXGI_FORMAT_R8G8B8A8_UNORM;
        PsoDesc.DSVFormat                             = DXGI_FORMAT_UNKNOWN;
        PsoDesc.SampleDesc.Count                      = 1;
        PsoDesc.SampleDesc.Quality                    = 0;
        PsoDesc.NodeMask                              = 0;

        Result = ID3D12Device_CreateGraphicsPipelineState(Device, &PsoDesc, &IID_ID3D12PipelineState, &PSO);
        AssertHR(Result);

        ID3D10Blob_Release(VertexShader);
        ID3D10Blob_Release(PixelShader);
    }

    // Create the command list
    ID3D12GraphicsCommandList *CommandList = NULL;

    {
        Result = ID3D12Device_CreateCommandList(Device, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, DirectQueueCommandAllocator, PSO, &IID_ID3D12CommandList, &CommandList);
        AssertHR(Result);

        // Command lists are created in the recording state, but there is nothing
        // to record yet. The main loop expects it to be closed, so close it now.
        Result = ID3D12GraphicsCommandList_Close(CommandList);
        AssertHR(Result);
    }

    // Create the vertex buffer
    ID3D12Resource *VertexBuffer = NULL;
    D3D12_VERTEX_BUFFER_VIEW VertexBufferView = {0};

    {
        const f32 AspectRatio = (f32)ResX/(f32)ResY;
        const f32 Vertices[] = 
        {
             0.00f,  0.25f*AspectRatio, 0.0f,     1.0f, 0.0f, 0.0f, 0.0f,
             0.25f, -0.25f*AspectRatio, 0.0f,     0.0f, 1.0f, 0.0f, 0.0f,
            -0.25f, -0.25f*AspectRatio, 0.0f,     0.0f, 0.0f, 1.0f, 0.0f,
        };

        D3D12_HEAP_PROPERTIES HeapProperties = {0};
        HeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC ResourceDesc = {0};
        ResourceDesc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
        ResourceDesc.Alignment          = 0;
        ResourceDesc.Width              = sizeof(Vertices);
        ResourceDesc.Height             = 1;
        ResourceDesc.DepthOrArraySize   = 1;
        ResourceDesc.MipLevels          = 1;
        ResourceDesc.Format             = DXGI_FORMAT_UNKNOWN;
        ResourceDesc.SampleDesc.Count   = 1;
        ResourceDesc.SampleDesc.Quality = 0;
        ResourceDesc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ResourceDesc.Flags              = D3D12_RESOURCE_FLAG_NONE;

        Result = ID3D12Device_CreateCommittedResource(Device, &HeapProperties, D3D12_HEAP_FLAG_NONE, &ResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource, &VertexBuffer);
        AssertHR(Result);

        // Copy the triangle data to the GPU memory
        u8 *DestBlock = NULL;
        D3D12_RANGE ReadRange = {0};

        Result = ID3D12Resource_Map(VertexBuffer, 0, &ReadRange, &DestBlock);
        AssertHR(Result);

        Assert(DestBlock);
        memcpy(DestBlock, Vertices, sizeof(Vertices));

        ID3D12Resource_Unmap(VertexBuffer, 0, NULL);

        // Initialize the vertex buffer view
        VertexBufferView.BufferLocation = ID3D12Resource_GetGPUVirtualAddress(VertexBuffer);
        VertexBufferView.StrideInBytes  = sizeof(f32) * 7;
        VertexBufferView.SizeInBytes    = sizeof(Vertices);
    }


    // Create synchronization objects and wait until assets have been uploaded to the GPU
    ID3D12Fence *Fence = NULL;
    HANDLE FenceEvent = NULL;
    u64 FenceValue = 0;

    {
        Result = ID3D12Device_CreateFence(Device, FenceValue, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, &Fence);
        AssertHR(Result);

        ++FenceValue;

        FenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        Assert(FenceEvent);

        // Wait for the command queue to complete execution
        {
            u64 CurrentFenceValue = FenceValue;
            Result = ID3D12CommandQueue_Signal(DirectQueue, Fence, CurrentFenceValue);
            AssertHR(Result);

            ++FenceValue;

            u64 Completed = ID3D12Fence_GetCompletedValue(Fence);
            if(Completed < CurrentFenceValue)
            {
                Result = ID3D12Fence_SetEventOnCompletion(Fence, CurrentFenceValue, FenceEvent);
                AssertHR(Result);

                WaitForSingleObject(FenceEvent, INFINITE);
            }

            FrameIndex = IDXGISwapChain3_GetCurrentBackBufferIndex(SwapChain);
        }
    }


    //------------------------------------------------------------------------
    // - Enter main loop

    ShowWindow(Window, SW_SHOWDEFAULT);

    for(;;)
    {
        MSG Message;
        if(PeekMessage(&Message, NULL, 0, 0, PM_REMOVE))
        {
            if(Message.message == WM_QUIT)
            {
                break;
            }
            TranslateMessage(&Message);
            DispatchMessageA(&Message);
            continue;
        }

        // Record all the commands we need to render the scene into the command list
        {
            Result = ID3D12CommandAllocator_Reset(DirectQueueCommandAllocator);
            AssertHR(Result);

            Result = ID3D12GraphicsCommandList_Reset(CommandList, DirectQueueCommandAllocator, PSO);
            AssertHR(Result);

            // Sets the layout of the graphics root signature
            ID3D12GraphicsCommandList_SetGraphicsRootSignature(CommandList, RootSignature);

            // Bind an array of viewports to the rasterizer stage of the pipeline
            {
                D3D12_VIEWPORT Viewports[] =
                {
                    {
                        .TopLeftX = 0.0f,
                        .TopLeftY = 0.0f,
                        .Width    = (f32)ResX,
                        .Height   = (f32)ResY,
                        .MinDepth = 0.0f,
                        .MaxDepth = 0.0f
                    }
                };

                ID3D12GraphicsCommandList_RSSetViewports(CommandList, ArrayCount(Viewports), Viewports);
            }

            // Binds an array of scissor rectangles to the rasterizer stage
            {
                D3D12_RECT ScissorRectangles[] = 
                {
                    {
                        .left   = 0,
                        .top    = 0,
                        .right  = ResX,
                        .bottom = ResY
                    }
                };

                ID3D12GraphicsCommandList_RSSetScissorRects(CommandList, ArrayCount(ScissorRectangles), ScissorRectangles);
            }


            // Indicate that the back buffer will be used as a render target
            D3D12_CPU_DESCRIPTOR_HANDLE RtvDescriptorHandle = {0};

            {
                D3D12_RESOURCE_BARRIER ResourceBarriers[] = 
                {
                    {
                        .Type       = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
                        .Flags      = D3D12_RESOURCE_BARRIER_FLAG_NONE,
                        .Transition = 
                        {
                            .pResource   = RenderTargets[FrameIndex],
                            .StateBefore = D3D12_RESOURCE_STATE_PRESENT,
                            .StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET,
                            .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES
                        }
                    }
                };

                ID3D12GraphicsCommandList_ResourceBarrier(CommandList, ArrayCount(ResourceBarriers), ResourceBarriers);


                // GetCPUDescriptorHandleForHeapStart declaration is broken in Windows C interface.
                // The result should be passed as an output parameter, rather than a return value.
                // https://joshstaiger.org/notes/C-Language-Problems-in-Direct3D-12-GetCPUDescriptorHandleForHeapStart.html
                typedef void(get_cpu_descriptor_handle_for_heap_start)(ID3D12DescriptorHeap*, D3D12_CPU_DESCRIPTOR_HANDLE*);
                get_cpu_descriptor_handle_for_heap_start *GetCPUDescriptorHandleForHeapStart = (get_cpu_descriptor_handle_for_heap_start *)RtvDescriptorHeap->lpVtbl->GetCPUDescriptorHandleForHeapStart;

                GetCPUDescriptorHandleForHeapStart(RtvDescriptorHeap, &RtvDescriptorHandle);
                if(FrameIndex > 0)
                {
                    u32 Size = ID3D12Device_GetDescriptorHandleIncrementSize(Device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
                    RtvDescriptorHandle.ptr += FrameIndex * Size;
                }

                ID3D12GraphicsCommandList_OMSetRenderTargets(CommandList, 1, &RtvDescriptorHandle, FALSE, NULL);
            }

            // Record commands
            {
                const f32 ClearColor[] = { 0.05f, 0.05f, 0.05f, 1.0f };
                ID3D12GraphicsCommandList_ClearRenderTargetView(CommandList, RtvDescriptorHandle, ClearColor, 0, NULL);
                ID3D12GraphicsCommandList_IASetPrimitiveTopology(CommandList, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                ID3D12GraphicsCommandList_IASetVertexBuffers(CommandList, 0, 1, &VertexBufferView);
                ID3D12GraphicsCommandList_DrawInstanced(CommandList, 3, 1, 0, 0);
                
                D3D12_RESOURCE_BARRIER ResourceBarriers[] = 
                {
                    {
                        .Type       = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
                        .Flags      = D3D12_RESOURCE_BARRIER_FLAG_NONE,
                        .Transition = 
                        {
                            .pResource   = RenderTargets[FrameIndex],
                            .StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET,
                            .StateAfter  = D3D12_RESOURCE_STATE_PRESENT,
                            .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES
                        }
                    }
                };

                ID3D12GraphicsCommandList_ResourceBarrier(CommandList, ArrayCount(ResourceBarriers), ResourceBarriers);
            }

            Result = ID3D12GraphicsCommandList_Close(CommandList);
            AssertHR(Result);
        }

        // Execute command lists
        {
            ID3D12GraphicsCommandList *CommandLists[] = { CommandList };
            ID3D12CommandQueue_ExecuteCommandLists(DirectQueue, ArrayCount(CommandLists), (ID3D12CommandList **)CommandLists);
        }

        // Present the frame
        {
            u32 SyncInterval = 1;
            u32 PresentFlags = 0;
            DXGI_PRESENT_PARAMETERS PresentParameters = {0};

            Result = IDXGISwapChain1_Present1(SwapChain, SyncInterval, PresentFlags, &PresentParameters);
            AssertHR(Result);
        }

        // Wait for the command queue to complete execution
        {
            u64 CurrentFenceValue = FenceValue;
            Result = ID3D12CommandQueue_Signal(DirectQueue, Fence, CurrentFenceValue);
            AssertHR(Result);

            ++FenceValue;

            u64 Completed = ID3D12Fence_GetCompletedValue(Fence);
            if(Completed < CurrentFenceValue)
            {
                Result = ID3D12Fence_SetEventOnCompletion(Fence, CurrentFenceValue, FenceEvent);
                AssertHR(Result);

                WaitForSingleObject(FenceEvent, INFINITE);
            }

            FrameIndex = IDXGISwapChain3_GetCurrentBackBufferIndex(SwapChain);
            Assert(FrameIndex < ArrayCount(RenderTargets));
        }
    }

    //------------------------------------------------------------------------
    // - Shutdown

    // Wait for the command queue to complete execution
    {
        u64 CurrentFenceValue = FenceValue;
        Result = ID3D12CommandQueue_Signal(DirectQueue, Fence, CurrentFenceValue);
        AssertHR(Result);

        ++FenceValue;

        u64 Completed = ID3D12Fence_GetCompletedValue(Fence);
        if(Completed < CurrentFenceValue)
        {
            Result = ID3D12Fence_SetEventOnCompletion(Fence, CurrentFenceValue, FenceEvent);
            AssertHR(Result);

            WaitForSingleObject(FenceEvent, INFINITE);
        }

        FrameIndex = IDXGISwapChain3_GetCurrentBackBufferIndex(SwapChain);
    }

    ID3D12Fence_Release(Fence);
    CloseHandle(FenceEvent);
    ID3D12Resource_Release(VertexBuffer);

    for(u32 RenderTargetIndex = 0; RenderTargetIndex < ArrayCount(RenderTargets); ++RenderTargetIndex)
    {
        ID3D12Resource_Release(RenderTargets[RenderTargetIndex]);
    }

    ID3D12GraphicsCommandList_Release(CommandList);
    ID3D12PipelineState_Release(PSO);
    ID3D12RootSignature_Release(RootSignature);
    ID3D12DescriptorHeap_Release(RtvDescriptorHeap);
    IDXGISwapChain1_Release(SwapChain);
    ID3D12CommandAllocator_Release(DirectQueueCommandAllocator);
    ID3D12CommandQueue_Release(DirectQueue);
    ID3D12Device_Release(Device);
    IDXGIAdapter1_Release(Adapter);
    IDXGIFactory4_Release(Factory);

    #if DEBUG_ENABLED
    {
        ID3D12Debug_Release(DebugInterface);
    }
    #endif

    return 0;
}
