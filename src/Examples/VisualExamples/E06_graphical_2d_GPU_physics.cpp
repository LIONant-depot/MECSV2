#include "../dependencies/DiligentEngine/DiligentTools/ThirdParty/imgui/imgui.h"

#define NOMINMAX
#include <windows.h>
#include <memoryapi.h>

#define PROPERTY_EDITOR
#include "mecs.h"
#include "SimpleEngine/MEGS-GraphicalApp.h"
#include <random>
#include "Graphics/GraphicsTools/interface/ShaderMacroHelper.hpp"
#include "../../dependencies/xcore/dependencies/properties/src/Examples/ImGuiExample/ImGuiPropertyInspector.h"
#include "Examples/VisualExamples/E06_graphical_2d_GPU_physics.h"

void PageFlip (void);
extern float s_ZoomFactor;

namespace mecs::examples::E06_graphical_2d_gpu_physics
{
    //-----------------------------------------------------------------------------------------
    // Example Menu
    //-----------------------------------------------------------------------------------------
    struct my_menu2 : xcore::property::base
    {
        int     m_EntitieCount;

        my_menu2() { reset(); }
        void reset()
        {
            m_EntitieCount     = 300000;
        }

        property_vtable()
    };
    static my_menu2 s_MyMenu;

    //-----------------------------------------------------------------------------------------
    // Basics structures
    //-----------------------------------------------------------------------------------------
    struct float2
    {
        float m_X, m_Y;
    };

    struct int2
    {
        std::int32_t m_X, m_Y;
    };

    struct particle_attribs
    {
        float2  m_Pos;
        float2  m_NewPos;
                
        float2  m_Speed;
        float2  m_NewSpeed;

        float   m_Size;
        int     m_iNumCollisions;
    };

    //-----------------------------------------------------------------------------------------
    // Black Board
    //-----------------------------------------------------------------------------------------
    struct physics_bundle : mecs::component::singleton
    {
        int                                                         m_NumParticles                  {};
        int                                                         m_ThreadGroupSize               = 256;

        Diligent::RefCntAutoPtr<Diligent::IPipelineState>           m_pRenderParticlePSO            {};
        Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding>   m_pRenderParticleSRB            {};
        Diligent::RefCntAutoPtr<Diligent::IPipelineState>           m_pResetParticleListsPSO        {};
        Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding>   m_pResetParticleListsSRB        {};
        Diligent::RefCntAutoPtr<Diligent::IPipelineState>           m_pMoveParticlesPSO             {};
        Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding>   m_pMoveParticlesSRB             {};
        Diligent::RefCntAutoPtr<Diligent::IPipelineState>           m_pCollideParticlesPSO          {};
        Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding>   m_pCollideParticlesSRB          {};
        Diligent::RefCntAutoPtr<Diligent::IPipelineState>           m_pUpdateParticleSpeedPSO       {};
        Diligent::RefCntAutoPtr<Diligent::IBuffer>                  m_Constants                     {};
        Diligent::RefCntAutoPtr<Diligent::IBuffer>                  m_pParticleAttribsBuffer        {};
        Diligent::RefCntAutoPtr<Diligent::IBuffer>                  m_pParticleListsBuffer          {};
        Diligent::RefCntAutoPtr<Diligent::IBuffer>                  m_pParticleListHeadsBuffer      {};
        Diligent::RefCntAutoPtr<Diligent::IResourceMapping>         m_pResMapping                   {};

        physics_bundle()
        {
            Initialize(
                graphical_app::s_pTheApp->m_pDevice
            ,   graphical_app::s_pTheApp->m_pSwapChain
            ,   graphical_app::s_pTheApp->m_pEngineFactory);
        }

        void CreateRenderParticlePSO(
            Diligent::RefCntAutoPtr<Diligent::IRenderDevice>&   pDevice
        ,   Diligent::RefCntAutoPtr<Diligent::ISwapChain>&      pSwapChain
        ,   Diligent::RefCntAutoPtr<Diligent::IEngineFactory>&  pEngineFactory );

        void CreateUpdateParticlePSO(
            Diligent::RefCntAutoPtr<Diligent::IRenderDevice>&   pDevice
        ,   Diligent::RefCntAutoPtr<Diligent::IEngineFactory>&  pEngineFactory );

        void CreateParticleBuffers(
            Diligent::RefCntAutoPtr<Diligent::IRenderDevice>&   pDevice );

        void CreateConsantBuffer(
            Diligent::RefCntAutoPtr<Diligent::IRenderDevice>&   pDevice );

        void Initialize(
            Diligent::RefCntAutoPtr<Diligent::IRenderDevice>&   pDevice
        ,   Diligent::RefCntAutoPtr<Diligent::ISwapChain>&      pSwapChain
        ,   Diligent::RefCntAutoPtr<Diligent::IEngineFactory>&  pEngineFactory );

        void Render(
            float                                               DeltaTime
        ,   Diligent::RefCntAutoPtr<Diligent::IRenderDevice>&   pDevice
        ,   Diligent::RefCntAutoPtr<Diligent::ISwapChain>&      pSwapChain
        ,   Diligent::RefCntAutoPtr<Diligent::IDeviceContext>&  pImmediateContext
        );

    }; 

    //-----------------------------------------------------------------------------------------

    void physics_bundle::Initialize(
        Diligent::RefCntAutoPtr<Diligent::IRenderDevice>&       pDevice
    ,   Diligent::RefCntAutoPtr<Diligent::ISwapChain>&          pSwapChain
    ,   Diligent::RefCntAutoPtr<Diligent::IEngineFactory>&      pEngineFactory )
    {
        const auto& deviceCaps = pDevice->GetDeviceCaps();
        if (!deviceCaps.Features.ComputeShaders)
        {
            throw std::runtime_error("Compute shaders are required to run this tutorial");
        }

        m_NumParticles = s_MyMenu.m_EntitieCount;

        CreateConsantBuffer(pDevice);
        CreateRenderParticlePSO(pDevice, pSwapChain, pEngineFactory);
        CreateUpdateParticlePSO(pDevice, pEngineFactory);
        CreateParticleBuffers(pDevice);
    }

    //-----------------------------------------------------------------------------------------

    void physics_bundle::Render(
        float                                               DeltaTime
    ,   Diligent::RefCntAutoPtr<Diligent::IRenderDevice>&   pDevice
    ,   Diligent::RefCntAutoPtr<Diligent::ISwapChain>&      pSwapChain
    ,   Diligent::RefCntAutoPtr<Diligent::IDeviceContext>&  pImmediateContext
    )
    {
        {
            struct constants
            {
                Diligent::float4x4  m_CL2;
                std::uint32_t       m_NumParticles;
                float               m_DeltaTime;
                float               m_Dummy0;
                float               m_Dummy1;

                float2              m_Scale;
                int2                m_ParticleGridSize;
            };

            Diligent::MapHelper<constants> ConstData(pImmediateContext, m_Constants, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);

            ConstData->m_CL2 = Diligent::float4x4::Ortho
            (
                  static_cast<float>(graphical_app::s_pTheApp->m_pSwapChain->GetDesc().Width)
                , static_cast<float>(graphical_app::s_pTheApp->m_pSwapChain->GetDesc().Height)
                , 0.1f
                , 100.0f
                , false
            );

            float s = s_ZoomFactor + 0.6f;
            ConstData->m_CL2 = ConstData->m_CL2.Scale(s, s, 1.0f);
            ConstData->m_CL2 = ConstData->m_CL2.Transpose();

            // Map the buffer and write current world-view-projection matrix
            ConstData->m_NumParticles = static_cast<std::uint32_t>(m_NumParticles);
            ConstData->m_DeltaTime    = DeltaTime;

            float  AspectRatio = static_cast<float>(pSwapChain->GetDesc().Width) / static_cast<float>(pSwapChain->GetDesc().Height);
            float2 f2Scale = float2{std::sqrt(1.f / AspectRatio), std::sqrt(AspectRatio)};
            ConstData->m_Scale = f2Scale;

            int iParticleGridWidth = static_cast<int>(std::sqrt(static_cast<float>(m_NumParticles)) / f2Scale.m_X);
            ConstData->m_ParticleGridSize.m_X = iParticleGridWidth;
            ConstData->m_ParticleGridSize.m_Y = m_NumParticles / iParticleGridWidth;
        }

        Diligent::DispatchComputeAttribs DispatAttribs;
        DispatAttribs.ThreadGroupCountX = (m_NumParticles + m_ThreadGroupSize - 1) / m_ThreadGroupSize;

        //
        // Compute new position/velocity
        //
        pImmediateContext->SetPipelineState(m_pResetParticleListsPSO);
        pImmediateContext->CommitShaderResources(m_pResetParticleListsSRB, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        pImmediateContext->DispatchCompute(DispatAttribs);

        pImmediateContext->SetPipelineState(m_pMoveParticlesPSO);
        pImmediateContext->CommitShaderResources(m_pMoveParticlesSRB, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        pImmediateContext->DispatchCompute(DispatAttribs);

        pImmediateContext->SetPipelineState(m_pCollideParticlesPSO);
        pImmediateContext->CommitShaderResources(m_pCollideParticlesSRB, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        pImmediateContext->DispatchCompute(DispatAttribs);

        pImmediateContext->SetPipelineState(m_pUpdateParticleSpeedPSO);
        // Use the same SRB
        pImmediateContext->CommitShaderResources(m_pCollideParticlesSRB, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        pImmediateContext->DispatchCompute(DispatAttribs);

        //
        // Render 
        //
        pImmediateContext->SetPipelineState(m_pRenderParticlePSO);
        pImmediateContext->CommitShaderResources(m_pRenderParticleSRB, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        Diligent::DrawAttribs drawAttrs;
        drawAttrs.NumVertices = 4;
        drawAttrs.NumInstances = static_cast<std::uint32_t>(m_NumParticles);
        pImmediateContext->Draw(drawAttrs);
    }

    //-----------------------------------------------------------------------------------------

    void physics_bundle::CreateConsantBuffer(Diligent::RefCntAutoPtr<Diligent::IRenderDevice>& pDevice)
    {
        Diligent::BufferDesc BuffDesc;
        BuffDesc.Name                   = "Constants buffer";
        BuffDesc.Usage                  = Diligent::USAGE_DYNAMIC;
        BuffDesc.BindFlags              = Diligent::BIND_UNIFORM_BUFFER;
        BuffDesc.CPUAccessFlags         = Diligent::CPU_ACCESS_WRITE;
        BuffDesc.uiSizeInBytes          = sizeof(Diligent::float4) * 2;
        pDevice->CreateBuffer(BuffDesc, nullptr, &m_Constants);
    }

    //-----------------------------------------------------------------------------------------

    void physics_bundle::CreateRenderParticlePSO(
            Diligent::RefCntAutoPtr<Diligent::IRenderDevice>&   pDevice
        ,   Diligent::RefCntAutoPtr<Diligent::ISwapChain>&      pSwapChain
        ,   Diligent::RefCntAutoPtr<Diligent::IEngineFactory>&  pEngineFactory
        )
    {
        Diligent::PipelineStateCreateInfo PSOCreateInfo;
        Diligent::PipelineStateDesc& PSODesc = PSOCreateInfo.PSODesc;

        // Pipeline state name is used by the engine to report issues.
        PSODesc.Name                                            = "Render particles PSO";

        // This is a graphics pipeline
        PSODesc.PipelineType                                    = Diligent::PIPELINE_TYPE::PIPELINE_TYPE_GRAPHICS;

        // clang-format off
        // This tutorial will render to a single render target
        PSODesc.GraphicsPipeline.NumRenderTargets               = 1;
        // Set render target format which is the format of the swap chain's color buffer
        PSODesc.GraphicsPipeline.RTVFormats[0]                  = pSwapChain->GetDesc().ColorBufferFormat;
        // Set depth buffer format which is the format of the swap chain's back buffer
        PSODesc.GraphicsPipeline.DSVFormat                      = pSwapChain->GetDesc().DepthBufferFormat;
        // Primitive topology defines what kind of primitives will be rendered by this pipeline state
        PSODesc.GraphicsPipeline.PrimitiveTopology              = Diligent::PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        // Disable back face culling
        PSODesc.GraphicsPipeline.RasterizerDesc.CullMode        = Diligent::CULL_MODE_NONE;
        // Disable depth testing
        PSODesc.GraphicsPipeline.DepthStencilDesc.DepthEnable   = false;
        // clang-format on

        auto& BlendDesc = PSODesc.GraphicsPipeline.BlendDesc;

        BlendDesc.RenderTargets[0].BlendEnable                  = true;
        BlendDesc.RenderTargets[0].SrcBlend                     = Diligent::BLEND_FACTOR_SRC_ALPHA;
        BlendDesc.RenderTargets[0].DestBlend                    = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;

        Diligent::ShaderCreateInfo ShaderCI;
        // Tell the system that the shader source code is in HLSL.
        // For OpenGL, the engine will convert this into GLSL under the hood.
        ShaderCI.SourceLanguage                                 = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;

        // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
        ShaderCI.UseCombinedTextureSamplers                     = true;

        // Create a shader source stream factory to load shaders from files.
        Diligent::RefCntAutoPtr<Diligent::IShaderSourceInputStreamFactory> pShaderSourceFactory;
        pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
        ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
        // Create particle vertex shader
        Diligent::RefCntAutoPtr<Diligent::IShader> pVS;
        {
            ShaderCI.Desc.ShaderType    = Diligent::SHADER_TYPE_VERTEX;
            ShaderCI.EntryPoint         = "main";
            ShaderCI.Desc.Name          = "Particle VS";
            ShaderCI.FilePath           = "assets/E06_graphical_2d_GPU_physics/particle.vsh";
            pDevice->CreateShader(ShaderCI, &pVS);
            xassert(pVS);
        }

        // Create particle pixel shader
        Diligent::RefCntAutoPtr<Diligent::IShader> pPS;
        {
            ShaderCI.Desc.ShaderType    = Diligent::SHADER_TYPE_PIXEL;
            ShaderCI.EntryPoint         = "main";
            ShaderCI.Desc.Name          = "Particle PS";
            ShaderCI.FilePath           = "assets/E06_graphical_2d_GPU_physics/particle.psh";
            pDevice->CreateShader(ShaderCI, &pPS);
            xassert(pPS);
        }

        PSODesc.GraphicsPipeline.pVS    = pVS;
        PSODesc.GraphicsPipeline.pPS    = pPS;

        // Define variable type that will be used by default
        PSODesc.ResourceLayout.DefaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

        // clang-format off
        // Shader variables should typically be mutable, which means they are expected
        // to change on a per-instance basis
        Diligent::ShaderResourceVariableDesc Vars[] =
        {
            { Diligent::SHADER_TYPE_VERTEX, "g_Particles", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE }
        };
        // clang-format on
        PSODesc.ResourceLayout.Variables        = Vars;
        PSODesc.ResourceLayout.NumVariables     = _countof(Vars);

        pDevice->CreatePipelineState(PSOCreateInfo, &m_pRenderParticlePSO);
        m_pRenderParticlePSO->GetStaticVariableByName(Diligent::SHADER_TYPE_VERTEX, "Constants")->Set(m_Constants);
    }

    //-----------------------------------------------------------------------------------------

    void physics_bundle::CreateUpdateParticlePSO(
            Diligent::RefCntAutoPtr<Diligent::IRenderDevice>&   pDevice
        ,   Diligent::RefCntAutoPtr<Diligent::IEngineFactory>&  pEngineFactory
        )
    {
        Diligent::ShaderCreateInfo ShaderCI;
        // Tell the system that the shader source code is in HLSL.
        // For OpenGL, the engine will convert this into GLSL under the hood.
        ShaderCI.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;

        // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
        ShaderCI.UseCombinedTextureSamplers = true;

        // Create a shader source stream factory to load shaders from files.
        Diligent::RefCntAutoPtr<Diligent::IShaderSourceInputStreamFactory> pShaderSourceFactory;
        pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
        ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

        Diligent::ShaderMacroHelper Macros;
        Macros.AddShaderMacro("THREAD_GROUP_SIZE", m_ThreadGroupSize);
        Macros.Finalize();

        Diligent::RefCntAutoPtr<Diligent::IShader> pResetParticleListsCS;
        {
            ShaderCI.Desc.ShaderType    = Diligent::SHADER_TYPE_COMPUTE;
            ShaderCI.EntryPoint         = "main";
            ShaderCI.Desc.Name          = "Reset particle lists CS";
            ShaderCI.FilePath           = "assets/E06_graphical_2d_GPU_physics/reset_particle_lists.csh";
            ShaderCI.Macros             = Macros;
            pDevice->CreateShader(ShaderCI, &pResetParticleListsCS);
            xassert(pResetParticleListsCS);
        }

        Diligent::RefCntAutoPtr<Diligent::IShader> pMoveParticlesCS;
        {
            ShaderCI.Desc.ShaderType    = Diligent::SHADER_TYPE_COMPUTE;
            ShaderCI.EntryPoint         = "main";
            ShaderCI.Desc.Name          = "Move particles CS";
            ShaderCI.FilePath           = "assets/E06_graphical_2d_GPU_physics/move_particles.csh";
            ShaderCI.Macros             = Macros;
            pDevice->CreateShader(ShaderCI, &pMoveParticlesCS);
            xassert(pMoveParticlesCS);
        }

        Diligent::RefCntAutoPtr<Diligent::IShader> pCollideParticlesCS;
        {
            ShaderCI.Desc.ShaderType    = Diligent::SHADER_TYPE_COMPUTE;
            ShaderCI.EntryPoint         = "main";
            ShaderCI.Desc.Name          = "Collide particles CS";
            ShaderCI.FilePath           = "assets/E06_graphical_2d_GPU_physics/collide_particles.csh";
            ShaderCI.Macros             = Macros;
            pDevice->CreateShader(ShaderCI, &pCollideParticlesCS);
            xassert(pCollideParticlesCS);
        }

        Diligent::RefCntAutoPtr<Diligent::IShader> pUpdatedSpeedCS;
        {
            ShaderCI.Desc.ShaderType    = Diligent::SHADER_TYPE_COMPUTE;
            ShaderCI.EntryPoint         = "main";
            ShaderCI.Desc.Name          = "Update particle speed CS";
            ShaderCI.FilePath           = "assets/E06_graphical_2d_GPU_physics/collide_particles.csh";
            Macros.AddShaderMacro("UPDATE_SPEED", 1);
            ShaderCI.Macros = Macros;
            pDevice->CreateShader(ShaderCI, &pUpdatedSpeedCS);
            xassert(pUpdatedSpeedCS);
        }

        Diligent::PipelineStateCreateInfo PSOCreateInfo;
        Diligent::PipelineStateDesc& PSODesc = PSOCreateInfo.PSODesc;

        // This is a compute pipeline
        PSODesc.PipelineType = Diligent::PIPELINE_TYPE::PIPELINE_TYPE_COMPUTE;


        PSODesc.ResourceLayout.DefaultVariableType  = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;
        // clang-format off
        Diligent::ShaderResourceVariableDesc Vars[] =
        {
            { Diligent::SHADER_TYPE_COMPUTE, "Constants", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC }
        };
        // clang-format on
        PSODesc.ResourceLayout.Variables    = Vars;
        PSODesc.ResourceLayout.NumVariables = _countof(Vars);

        PSODesc.Name = "Reset particle lists PSO";
        PSODesc.ComputePipeline.pCS = pResetParticleListsCS;
        pDevice->CreatePipelineState(PSOCreateInfo, &m_pResetParticleListsPSO);
        m_pResetParticleListsPSO->GetStaticVariableByName(Diligent::SHADER_TYPE_COMPUTE, "Constants")->Set(m_Constants);

        PSODesc.Name = "Move particles PSO";
        PSODesc.ComputePipeline.pCS = pMoveParticlesCS;
        pDevice->CreatePipelineState(PSOCreateInfo, &m_pMoveParticlesPSO);
        m_pMoveParticlesPSO->GetStaticVariableByName(Diligent::SHADER_TYPE_COMPUTE, "Constants")->Set(m_Constants);

        PSODesc.Name = "Collidse particles PSO";
        PSODesc.ComputePipeline.pCS = pCollideParticlesCS;
        pDevice->CreatePipelineState(PSOCreateInfo, &m_pCollideParticlesPSO);
        m_pCollideParticlesPSO->GetStaticVariableByName(Diligent::SHADER_TYPE_COMPUTE, "Constants")->Set(m_Constants);

        PSODesc.Name = "Update particle speed PSO";
        PSODesc.ComputePipeline.pCS = pUpdatedSpeedCS;
        pDevice->CreatePipelineState(PSOCreateInfo, &m_pUpdateParticleSpeedPSO);
        m_pUpdateParticleSpeedPSO->GetStaticVariableByName(Diligent::SHADER_TYPE_COMPUTE, "Constants")->Set(m_Constants);
    }

    //-----------------------------------------------------------------------------------------

    void physics_bundle::CreateParticleBuffers(
        Diligent::RefCntAutoPtr<Diligent::IRenderDevice>& pDevice 
        )
    {
        m_pParticleAttribsBuffer.Release();
        m_pParticleListHeadsBuffer.Release();
        m_pParticleListsBuffer.Release();

        Diligent::BufferDesc BuffDesc;
        BuffDesc.Name               = "Particle attribs buffer";
        BuffDesc.Usage              = Diligent::USAGE_DEFAULT;
        BuffDesc.BindFlags          = Diligent::BIND_SHADER_RESOURCE | Diligent::BIND_UNORDERED_ACCESS;
        BuffDesc.Mode               = Diligent::BUFFER_MODE_STRUCTURED;
        BuffDesc.ElementByteStride  = sizeof(particle_attribs);
        BuffDesc.uiSizeInBytes      = sizeof(particle_attribs) * m_NumParticles;
        BuffDesc.CPUAccessFlags     = Diligent::CPU_ACCESS_READ; // Tomas

        std::vector<particle_attribs> ParticleData(m_NumParticles);

        std::mt19937 gen; // Standard mersenne_twister_engine. Use default seed
                          // to generate consistent distribution.

        std::uniform_real_distribution<float> pos_distr(-1.f, +1.f);
        std::uniform_real_distribution<float> size_distr(0.5f, 1.f);

        constexpr float fMaxParticleSize = 0.05f;
        float           fSize = 0.7f / std::sqrt(static_cast<float>(m_NumParticles));
        fSize = std::min(fMaxParticleSize, fSize);
        for (auto& particle : ParticleData)
        {
            particle.m_NewPos.m_X   = pos_distr(gen);
            particle.m_NewPos.m_Y   = pos_distr(gen);
            particle.m_NewSpeed.m_X = pos_distr(gen) * fSize * 5.f;
            particle.m_NewSpeed.m_Y = pos_distr(gen) * fSize * 5.f;
            particle.m_Size         = fSize * size_distr(gen);
        }

        Diligent::BufferData VBData;
        VBData.pData = ParticleData.data();
        VBData.DataSize = sizeof(particle_attribs) * static_cast<Diligent::Uint32>(ParticleData.size());
        pDevice->CreateBuffer(BuffDesc, &VBData, &m_pParticleAttribsBuffer);
        Diligent::IBufferView* pParticleAttribsBufferSRV = m_pParticleAttribsBuffer->GetDefaultView(Diligent::BUFFER_VIEW_SHADER_RESOURCE);
        Diligent::IBufferView* pParticleAttribsBufferUAV = m_pParticleAttribsBuffer->GetDefaultView(Diligent::BUFFER_VIEW_UNORDERED_ACCESS);

        BuffDesc.ElementByteStride      = sizeof(int);
        BuffDesc.Mode                   = Diligent::BUFFER_MODE_FORMATTED;
        BuffDesc.uiSizeInBytes          = BuffDesc.ElementByteStride * static_cast<Diligent::Uint32>(m_NumParticles);
        BuffDesc.BindFlags              = Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE;
        pDevice->CreateBuffer(BuffDesc, nullptr, &m_pParticleListHeadsBuffer);
        pDevice->CreateBuffer(BuffDesc, nullptr, &m_pParticleListsBuffer);
        Diligent::RefCntAutoPtr<Diligent::IBufferView> pParticleListHeadsBufferUAV;
        Diligent::RefCntAutoPtr<Diligent::IBufferView> pParticleListsBufferUAV;
        Diligent::RefCntAutoPtr<Diligent::IBufferView> pParticleListHeadsBufferSRV;
        Diligent::RefCntAutoPtr<Diligent::IBufferView> pParticleListsBufferSRV;
        {
            Diligent::BufferViewDesc ViewDesc;
            ViewDesc.ViewType               = Diligent::BUFFER_VIEW_UNORDERED_ACCESS;
            ViewDesc.Format.ValueType       = Diligent::VT_INT32;
            ViewDesc.Format.NumComponents   = 1;
            m_pParticleListHeadsBuffer->CreateView(ViewDesc, &pParticleListHeadsBufferUAV);
            m_pParticleListsBuffer->CreateView(ViewDesc, &pParticleListsBufferUAV);

            ViewDesc.ViewType = Diligent::BUFFER_VIEW_SHADER_RESOURCE;
            m_pParticleListHeadsBuffer->CreateView(ViewDesc, &pParticleListHeadsBufferSRV);
            m_pParticleListsBuffer->CreateView(ViewDesc, &pParticleListsBufferSRV);
        }

        m_pResetParticleListsSRB.Release();
        m_pResetParticleListsPSO->CreateShaderResourceBinding(&m_pResetParticleListsSRB, true);
        m_pResetParticleListsSRB->GetVariableByName(Diligent::SHADER_TYPE_COMPUTE, "g_ParticleListHead")->Set(pParticleListHeadsBufferUAV);

        m_pRenderParticleSRB.Release();
        m_pRenderParticlePSO->CreateShaderResourceBinding(&m_pRenderParticleSRB, true);
        m_pRenderParticleSRB->GetVariableByName(Diligent::SHADER_TYPE_VERTEX, "g_Particles")->Set(pParticleAttribsBufferSRV);

        m_pMoveParticlesSRB.Release();
        m_pMoveParticlesPSO->CreateShaderResourceBinding(&m_pMoveParticlesSRB, true);
        m_pMoveParticlesSRB->GetVariableByName(Diligent::SHADER_TYPE_COMPUTE, "g_Particles")->Set(pParticleAttribsBufferUAV);
        m_pMoveParticlesSRB->GetVariableByName(Diligent::SHADER_TYPE_COMPUTE, "g_ParticleListHead")->Set(pParticleListHeadsBufferUAV);
        m_pMoveParticlesSRB->GetVariableByName(Diligent::SHADER_TYPE_COMPUTE, "g_ParticleLists")->Set(pParticleListsBufferUAV);

        m_pCollideParticlesSRB.Release();
        m_pCollideParticlesPSO->CreateShaderResourceBinding(&m_pCollideParticlesSRB, true);
        m_pCollideParticlesSRB->GetVariableByName(Diligent::SHADER_TYPE_COMPUTE, "g_Particles")->Set(pParticleAttribsBufferUAV);
        m_pCollideParticlesSRB->GetVariableByName(Diligent::SHADER_TYPE_COMPUTE, "g_ParticleListHead")->Set(pParticleListHeadsBufferSRV);
        m_pCollideParticlesSRB->GetVariableByName(Diligent::SHADER_TYPE_COMPUTE, "g_ParticleLists")->Set(pParticleListsBufferSRV);
    }

    //-----------------------------------------------------------------------------------------
    // System
    //-----------------------------------------------------------------------------------------
    namespace system
    {
        //----------------------------------------------------------------------------------------
        // FULL GPU PHYSICS
        //----------------------------------------------------------------------------------------
        struct full_gpu_physics : mecs::system::instance
        {
            using mecs::system::instance::instance;

            void operator() (physics_bundle& Bundle) noexcept
            {
                graphical_app::s_pTheApp->BeginRender();
                Bundle.Render
                (
                    getTime().m_DeltaTime
                    , graphical_app::s_pTheApp->m_pDevice
                    , graphical_app::s_pTheApp->m_pSwapChain
                    , graphical_app::s_pTheApp->m_pImmediateContext
                );
            }
        };

        //----------------------------------------------------------------------------------------
        // SYSTEM:: RENDER_PAGEFLIP
        //----------------------------------------------------------------------------------------
        struct render_pageflip : mecs::system::instance
        {
            // Make sure we are schegule first to let our system have the best chance to finish early
            render_pageflip( const instance::construct&& Construct)
                : mecs::system::instance{ [&] { auto C = Construct; C.m_JobDefinition.m_Priority = xcore::scheduler::priority::ABOVE_NORMAL; return C; }() }
            {
                s_bContinue = true;
            }

            inline
            void msgUpdate(void) noexcept
            {
                s_bContinue = m_Menu();
                PageFlip();
            }

            xcore::func<bool(void)> m_Menu;
            static inline bool s_bContinue;
        };
    }

    //-----------------------------------------------------------------------------------------
    // Test
    //-----------------------------------------------------------------------------------------
    void Test(bool(*Menu)(mecs::world::instance&, xcore::property::base&))
    {
        printf("--------------------------------------------------------------------------------\n");
        printf("E06_graphical_2d_gpu_physics\n");
        printf("--------------------------------------------------------------------------------\n");

        auto upUniverse = std::make_unique<mecs::universe::instance>();

        //------------------------------------------------------------------------------------------
        // Registration
        //------------------------------------------------------------------------------------------

        //
        // Register component
        //
        upUniverse->registerTypes<physics_bundle>();


        //
        // Register the game graph.
        // 
        auto& DefaultWorld  = *upUniverse->m_WorldDB[0];
        auto& SyncPhysics   = DefaultWorld.m_GraphDB.CreateSyncPoint();
        auto& System        = DefaultWorld.m_GraphDB.CreateGraphConnection<system::full_gpu_physics>  (DefaultWorld.m_GraphDB.m_StartSyncPoint, SyncPhysics);
                              DefaultWorld.m_GraphDB.CreateGraphConnection<system::render_pageflip>   (SyncPhysics,                             DefaultWorld.m_GraphDB.m_EndSyncPoint).m_Menu = [&] { return Menu(DefaultWorld, s_MyMenu); };

        //------------------------------------------------------------------------------------------
        // Initialization
        //------------------------------------------------------------------------------------------

        //
        // Create an entity group
        //
        auto& Archetype = DefaultWorld.m_ArchetypeDB.getOrCreateArchitype<physics_bundle>();

        Archetype.CreateEntity(System);

        //------------------------------------------------------------------------------------------
        // Running
        //------------------------------------------------------------------------------------------

        //
        // Start executing the world
        //
        DefaultWorld.Start();

        //
        // run 100 frames
        //
        while (system::render_pageflip::s_bContinue)
        {
            DefaultWorld.Resume();
        }
    }
}

property_begin_name(mecs::examples::E06_graphical_2d_gpu_physics::my_menu2, "Settings")
{
    property_var(m_EntitieCount)
        .Help("This specifies how many entities are going to get spawn.\n"
            "You must use the upper button call 'Use Settings' to \n"
            "activate it.")
    , property_var_fnbegin("Settings", string_t)
    {
        if (isRead) xcore::string::Copy(InOut, "Reset To Defaults");
        else         mecs::examples::E06_graphical_2d_gpu_physics::s_MyMenu.reset();
    }
    property_var_fnend()
        .EDStyle(property::edstyle<string_t>::Button())
        .Help("Will reset these settings menu back to the original values. \n"
            "Please note that to respawn the entities you need to use \n"
            "the 'Use Settings' button at the top.")
}
property_vend_h(mecs::examples::E06_graphical_2d_gpu_physics::my_menu2);

