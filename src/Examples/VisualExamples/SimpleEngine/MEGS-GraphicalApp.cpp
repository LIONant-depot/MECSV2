#include "MEGS-GraphicalApp.h"


std::unique_ptr<graphical_app>  graphical_app::s_pTheApp;

//---------------------------------------------------------------------------------------------------------

graphical_app::graphical_app()
{
}

//---------------------------------------------------------------------------------------------------------

graphical_app::~graphical_app()
{
    m_pImmediateContext->Flush();
}

//---------------------------------------------------------------------------------------------------------

bool graphical_app::InitializeDiligentEngine(HWND hWnd)
{
#    ifdef _XCORE_RELEASE
    SetDllDirectoryA("../../dependencies/DiligentEngine/libs/Release");
#else
    SetDllDirectoryA("../../dependencies/DiligentEngine/libs/Debug");
#endif

    std::vector<Diligent::IDeviceContext*> ppContexts;
    ppContexts.resize(1 + xcore::get().m_Scheduler.getWorkerCount());
    Diligent::SwapChainDesc SCDesc;
    switch (m_DeviceType)
    {
#if D3D11_SUPPORTED
    case Diligent::RENDER_DEVICE_TYPE_D3D11:
    {
        Diligent::EngineD3D11CreateInfo EngineCI;

        EngineCI.NumDeferredContexts = static_cast<Diligent::Uint32>(ppContexts.size() - 1);

#    ifdef _DEBUG
        EngineCI.DebugFlags |=
            Diligent::D3D11_DEBUG_FLAG_CREATE_DEBUG_DEVICE |
            Diligent::D3D11_DEBUG_FLAG_VERIFY_COMMITTED_SHADER_RESOURCES;
#    endif
#    if ENGINE_DLL
        // Load the dll and import GetEngineFactoryD3D11() function
        auto GetEngineFactoryD3D11 = Diligent::LoadGraphicsEngineD3D11();
#    endif
        auto* pFactoryD3D11 = GetEngineFactoryD3D11();
        pFactoryD3D11->CreateDeviceAndContextsD3D11(EngineCI, &m_pDevice, ppContexts.data());
        Diligent::Win32NativeWindow Window{ hWnd };
        pFactoryD3D11->CreateSwapChainD3D11(m_pDevice, ppContexts[0], SCDesc, Diligent::FullScreenModeDesc{}, Window, &m_pSwapChain);
        m_pEngineFactory = pFactoryD3D11;
    }
    break;
#endif


#if D3D12_SUPPORTED
    case Diligent::RENDER_DEVICE_TYPE_D3D12:
    {
#    if ENGINE_DLL
        // Load the dll and import GetEngineFactoryD3D12() function
        auto GetEngineFactoryD3D12 = Diligent::LoadGraphicsEngineD3D12();
#    endif
        Diligent::EngineD3D12CreateInfo EngineCI;
        EngineCI.NumDeferredContexts = static_cast<Diligent::Uint32>(ppContexts.size() - 1);

#    ifdef _DEBUG
        // There is a bug in D3D12 debug layer that causes memory leaks in this tutorial
        //EngineCI.EnableDebugLayer = true;
#    endif
        auto* pFactoryD3D12 = GetEngineFactoryD3D12();
        pFactoryD3D12->CreateDeviceAndContextsD3D12(EngineCI, &m_pDevice, ppContexts.data());
        Diligent::Win32NativeWindow Window{ hWnd };
        pFactoryD3D12->CreateSwapChainD3D12(m_pDevice, ppContexts[0], SCDesc, Diligent::FullScreenModeDesc{}, Window, &m_pSwapChain);
        m_pEngineFactory = pFactoryD3D12;
    }
    break;
#endif


#if GL_SUPPORTED
    case Diligent::RENDER_DEVICE_TYPE_GL:
    {

#    if EXPLICITLY_LOAD_ENGINE_GL_DLL
        // Load the dll and import GetEngineFactoryOpenGL() function
        auto GetEngineFactoryOpenGL = Diligent::LoadGraphicsEngineOpenGL();
#    endif
        auto* pFactoryOpenGL = GetEngineFactoryOpenGL();

        if (ppContexts.size() > 1)
        {
            LOG_ERROR_MESSAGE("Deferred contexts are not supported in OpenGL mode");
        }

        Diligent::EngineGLCreateInfo EngineCI;
        EngineCI.Window.hWnd = hWnd;
        pFactoryOpenGL->CreateDeviceAndSwapChainGL(EngineCI, &m_pDevice, ppContexts.data(), SCDesc, &m_pSwapChain);
        m_pEngineFactory = pFactoryOpenGL;
    }
    break;
#endif


#if VULKAN_SUPPORTED
    case Diligent::RENDER_DEVICE_TYPE_VULKAN:
    {
#    if EXPLICITLY_LOAD_ENGINE_VK_DLL
        // Load the dll and import GetEngineFactoryVk() function
        auto GetEngineFactoryVk = Diligent::LoadGraphicsEngineVk();
#    endif
        Diligent::EngineVkCreateInfo EngineCI;
        EngineCI.NumDeferredContexts = static_cast<Diligent::Uint32>(ppContexts.size() - 1);

#    ifdef _DEBUG
        EngineCI.EnableValidation = true;
#    endif
        auto* pFactoryVk = GetEngineFactoryVk();
        pFactoryVk->CreateDeviceAndContextsVk(EngineCI, &m_pDevice, ppContexts.data());
        m_pEngineFactory = pFactoryVk;
        if (!m_pSwapChain && hWnd != nullptr)
        {
            Diligent::Win32NativeWindow Window{ hWnd };
            pFactoryVk->CreateSwapChainVk(m_pDevice, ppContexts[0], SCDesc, Window, &m_pSwapChain);
        }
    }
    break;
#endif


    default:
        std::cerr << "Unknown/unsupported device type";
        return false;
        break;
    }

    m_pImmediateContext.Attach(ppContexts[0]);
    auto NumDeferredCtx = ppContexts.size() - 1;
    m_pDeferredContexts.resize(NumDeferredCtx);
    for (Diligent::Uint32 ctx = 0; ctx < NumDeferredCtx; ++ctx)
        m_pDeferredContexts[ctx].Attach(ppContexts[1 + ctx]);

    return true;
}

//---------------------------------------------------------------------------------------------------------

bool graphical_app::ProcessCommandLine(const char* CmdLine)
{
    const auto* Key = "-mode ";
    const auto* pos = strstr(CmdLine, Key);
    if (pos != nullptr)
    {
        pos += strlen(Key);
        if (_stricmp(pos, "D3D11") == 0)
        {
#if D3D11_SUPPORTED
            m_DeviceType = Diligent::RENDER_DEVICE_TYPE_D3D11;
#else
            std::cerr << "Direct3D11 is not supported. Please select another device type";
            return false;
#endif
        }
        else if (_stricmp(pos, "D3D12") == 0)
        {
#if D3D12_SUPPORTED
            m_DeviceType = Diligent::RENDER_DEVICE_TYPE_D3D12;
#else
            std::cerr << "Direct3D12 is not supported. Please select another device type";
            return false;
#endif
        }
        else if (_stricmp(pos, "GL") == 0)
        {
#if GL_SUPPORTED
            m_DeviceType = Diligent::RENDER_DEVICE_TYPE_GL;
#else
            std::cerr << "OpenGL is not supported. Please select another device type";
            return false;
#endif
        }
        else if (_stricmp(pos, "VK") == 0)
        {
#if VULKAN_SUPPORTED
            m_DeviceType = Diligent::RENDER_DEVICE_TYPE_VULKAN;
#else
            std::cerr << "Vulkan is not supported. Please select another device type";
            return false;
#endif
        }
        else
        {
            std::cerr << "Unknown device type. Only the following types are supported: D3D11, D3D12, GL, VK";
            return false;
        }
    }
    else
    {
#if D3D12_SUPPORTED
        m_DeviceType = Diligent::RENDER_DEVICE_TYPE_D3D12;
#elif VULKAN_SUPPORTED
        m_DeviceType = Diligent::RENDER_DEVICE_TYPE_VULKAN;
#elif D3D11_SUPPORTED
        m_DeviceType = Diligent::RENDER_DEVICE_TYPE_D3D11;
#elif GL_SUPPORTED
        m_DeviceType = Diligent::RENDER_DEVICE_TYPE_GL;
#endif
    }
    return true;
}

//---------------------------------------------------------------------------------------------------------

void graphical_app::BeginRender()
{
    XCORE_PERF_ZONE_SCOPED()

    if(m_bBeginRender) return;
    m_bBeginRender = true;

    // Set render targets before issuing any draw command.
    // Note that Present() unbinds the back buffer if it is set as render target.
    auto* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    auto* pDSV = m_pSwapChain->GetDepthBufferDSV();
    m_pImmediateContext->SetRenderTargets(1, &pRTV, pDSV, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Clear the back buffer
    const float ClearColor[] = { 0.350f, 0.350f, 0.350f, 1.0f };
    // Let the engine perform required state transitions
    m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(pDSV, Diligent::CLEAR_DEPTH_FLAG, 1.f, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

//---------------------------------------------------------------------------------------------------------

void graphical_app::Present()
{
    xassert(m_bBeginRender);

    XCORE_PERF_ZONE_SCOPED()
    m_pSwapChain->Present(false);

    m_bBeginRender = false;
}

//---------------------------------------------------------------------------------------------------------

void graphical_app::WindowResize(Diligent::Uint32 Width, Diligent::Uint32 Height)
{
    if (m_pSwapChain)
        m_pSwapChain->Resize(Width, Height);
}
