#include "MEGS-GraphicalApp.h"

#include "../DiligentSamples/SampleBase/src/Win32/InputControllerWin32.cpp"

using namespace Diligent;

LRESULT CALLBACK MessageProc(HWND, UINT, WPARAM, LPARAM);


//---------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------


static const char* pDebugRenderVSSource = R"(

// Vertex shader takes 3 inputs: vertex position, uvs and color.
// By convention, Diligent Engine expects vertex shader inputs to be 
// labeled 'ATTRIBn', where n is the attribute number.
struct VSInput 
{ 
    float3 Pos          : ATTRIB0;
    float2 TextureUV    : ATTRIB1;
    float4 Color        : ATTRIB2; 
};

cbuffer Constants
{
    float4x4 g_L2C;
};

struct PSInput 
{ 
    float4 Pos          : SV_POSITION;
    float4 Color        : COLOR; 
    float2 TextureUV    : TEXCOORD;
};

// Note that if separate shader objects are not supported (this is only the case for old GLES3.0 devices), vertex
// shader output variable name must match exactly the name of the pixel shader input variable.
// If the variable has structure type (like in this example), the structure declarations must also be indentical.
void main(in  VSInput VSIn,
          out PSInput PSIn) 
{
    PSIn.Pos   		= mul( g_L2C, float4( VSIn.Pos, 1.0f ) );
    PSIn.TextureUV 	= VSIn.TextureUV;
    PSIn.Color 		= VSIn.Color;
}
)";

// Pixel shader simply outputs interpolated vertex color
static const char* pDebugRenderPSSource = R"(
struct PSInput 
{ 
    float4 Pos          : SV_POSITION;
    float4 Color        : COLOR; 
    float2 TextureUV    : TEXCOORD;
};

// Note that if separate shader objects are not supported (this is only the case for old GLES3.0 devices), vertex
// shader output variable name must match exactly the name of the pixel shader input variable.
// If the variable has structure type (like in this example), the structure declarations must also be indentical.
float4 main(in PSInput PSIn) : SV_Target
{    
//    PSOut.Color = float4( PSIn.Color.xyz, 0.5 );
//    PSOut.Color = float4( 1.0, 1.0, 1.0, 0.5 );

    return PSIn.Color; //float4( PSIn.Color.xyz, 0.5 ); // PSIn.Color.xyz;
}
)";

namespace Diligent
{
    void DebugMessageCallbackD( Diligent::DEBUG_MESSAGE_SEVERITY Severity, const Char* Message, const char* Function, const char* File, int Line)
    {
        assert(false);
    }

    DebugMessageCallbackType DebugMessageCallback = DebugMessageCallbackD;
}

struct dbg_render
{
    static constexpr auto num_verts         = 300000;
    static constexpr auto num_indices       = 300000;
    static constexpr int  num_blend_states  = 5;
    
    struct vertex
    {
        xcore::vector3d     m_Position;
        xcore::vector2      m_UV;
        std::uint32_t       m_Color;
    };

    struct vertex_buffer
    {
        vertex_buffer*          m_pNext         { nullptr };
        RefCntAutoPtr<IBuffer>  m_VertexBuffer  { nullptr };

        vertex_buffer( dbg_render& DBRender )
        {
            BufferDesc InstBuffDesc{};
            InstBuffDesc.Name           = "dbg_render::vertex_buffer";
            InstBuffDesc.Usage          = USAGE_DYNAMIC;
            InstBuffDesc.BindFlags      = BIND_VERTEX_BUFFER;
            InstBuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
            InstBuffDesc.uiSizeInBytes  = sizeof(vertex) * num_verts;
            graphical_app::s_pTheApp->m_pDevice->CreateBuffer(InstBuffDesc, nullptr, &m_VertexBuffer );
            StateTransitionDesc Barrier(m_VertexBuffer, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_VERTEX_BUFFER, true);
            graphical_app::s_pTheApp->m_pImmediateContext->TransitionResourceStates(1, &Barrier);
        }
    };

    struct index_buffer
    {
        index_buffer*           m_pNext;
        RefCntAutoPtr<IBuffer>  m_IndexBuffer;

        index_buffer( dbg_render& DBRender )
        {
            BufferDesc InstBuffDesc;
            InstBuffDesc.Name           = "dbg_render::index_buffer";
            InstBuffDesc.Usage          = USAGE_DYNAMIC;
            InstBuffDesc.BindFlags      = BIND_INDEX_BUFFER;
            InstBuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
            InstBuffDesc.uiSizeInBytes  = sizeof(int) * num_indices;
            graphical_app::s_pTheApp->m_pDevice->CreateBuffer(InstBuffDesc, nullptr, &m_IndexBuffer );
            StateTransitionDesc Barrier(m_IndexBuffer, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_VERTEX_BUFFER, true);
            graphical_app::s_pTheApp->m_pImmediateContext->TransitionResourceStates(1, &Barrier);
        }
    };

    struct primitive
    {
        enum class type : std::uint8_t
        {
            NONE
            , LINE
            , TRIANGLE
        };

        type        m_NextPrimitive{primitive::type::NONE};
        int         m_iVB{0};
        int         m_iIB{0};
        int         m_nPrimitives{0};
        float       m_Order{0};
    };

    struct per_thread
    {
        primitive::type                             m_FirstPrimitive;
        primitive::type                             m_LastPrimitive;
        std::vector<primitive>                      m_TrianglePrim;
        std::vector<primitive>                      m_LinePrim;
        std::vector<vertex_buffer*>                 m_LineVB;
        std::vector<vertex_buffer*>                 m_lpTrianglesVB;
        std::vector<index_buffer*>                  m_lpTrianglesIB;
        Diligent::MapHelper<vertex>                 m_LineVBMap;
        Diligent::MapHelper<vertex>                 m_TriangleVBMap;
        Diligent::MapHelper<int>                    m_TriangleIBMap;
        int                                         m_TriangleiVertex;
        int                                         m_TriangleiIndex;
    };

    void Initialize(void)
    {
        CreatePipelineStates();
        m_MyDelegatePostPageFlip.Connect( graphical_app::s_pTheApp->m_Events.m_postPageFlip );
        m_MyDelegatePrePageFlip.Connect( graphical_app::s_pTheApp->m_Events.m_prePageFlip );

        m_PerFramePerThread.resize( graphical_app::s_pTheApp->m_pDeferredContexts.size() );
    }

    void CreatePipelineStates( void )
    {
        GraphicsPipelineStateCreateInfo                 PSOCreateInfo;
        std::array<BlendStateDesc, num_blend_states>    BlendState;
        GraphicsPipelineDesc&                           GraphicsPipeline = PSOCreateInfo.GraphicsPipeline;
        PipelineStateDesc&                              PSODesc          = PSOCreateInfo.PSODesc;

        /*
        BlendState[0].RenderTargets[0].BlendEnable = true;
        BlendState[0].RenderTargets[0].SrcBlend    = BLEND_FACTOR_SRC_ALPHA;
        BlendState[0].RenderTargets[0].DestBlend   = BLEND_FACTOR_INV_SRC_ALPHA;
        */
        BlendState[1].RenderTargets[0].BlendEnable = true;
        BlendState[1].RenderTargets[0].SrcBlend    = BLEND_FACTOR_SRC_ALPHA;
        BlendState[1].RenderTargets[0].DestBlend   = BLEND_FACTOR_INV_SRC_ALPHA;

        BlendState[2].RenderTargets[0].BlendEnable = true;
        BlendState[2].RenderTargets[0].SrcBlend    = BLEND_FACTOR_INV_SRC_ALPHA;
        BlendState[2].RenderTargets[0].DestBlend   = BLEND_FACTOR_SRC_ALPHA;

        BlendState[3].RenderTargets[0].BlendEnable = true;
        BlendState[3].RenderTargets[0].SrcBlend    = BLEND_FACTOR_SRC_COLOR;
        BlendState[3].RenderTargets[0].DestBlend   = BLEND_FACTOR_INV_SRC_COLOR;

        BlendState[4].RenderTargets[0].BlendEnable = true;
        BlendState[4].RenderTargets[0].SrcBlend    = BLEND_FACTOR_INV_SRC_COLOR;
        BlendState[4].RenderTargets[0].DestBlend   = BLEND_FACTOR_SRC_COLOR;

        // Pipeline state name is used by the engine to report issues.
        // It is always a good idea to give objects descriptive names.
        PSODesc.Name = "dbg_render::Pipelines";

        // This is a graphics pipeline
        PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

        // clang-format off
        // This tutorial will render to a single render target
        GraphicsPipeline.NumRenderTargets             = 1;
        // Set render target format which is the format of the swap chain's color buffer
        GraphicsPipeline.RTVFormats[0]                = graphical_app::s_pTheApp->m_pSwapChain->GetDesc().ColorBufferFormat;
        // Set depth buffer format which is the format of the swap chain's back buffer
        GraphicsPipeline.DSVFormat                    = graphical_app::s_pTheApp->m_pSwapChain->GetDesc().DepthBufferFormat;
        // Primitive topology defines what kind of primitives will be rendered by this pipeline state
        GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        // Disable back face culling
        GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE;
        // Disable depth testing
        GraphicsPipeline.DepthStencilDesc.DepthEnable = False;
        // clang-format on

        ShaderCreateInfo ShaderCI;
        // Tell the system that the shader source code is in HLSL.
        // For OpenGL, the engine will convert this into GLSL under the hood.
        ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

        // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
        ShaderCI.UseCombinedTextureSamplers = true;

        // Create a shader source stream factory to load shaders from files.
        //-- RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
        //-- m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
        //-- ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

        //
        // Create shaders
        //
        RefCntAutoPtr<IShader> pVS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
            ShaderCI.EntryPoint      = "main";
            ShaderCI.Desc.Name       = "DebugRender vertex shader";
            ShaderCI.Source          = pDebugRenderVSSource;
            graphical_app::s_pTheApp->m_pDevice->CreateShader(ShaderCI, &pVS);

            // Create dynamic uniform buffer that will store our transformation matrix
            // Dynamic buffers can be frequently updated by the CPU
            BufferDesc CBDesc;
            CBDesc.Name           = "VS constants CB";
            CBDesc.uiSizeInBytes  = sizeof(float4x4);
            CBDesc.Usage          = USAGE_DYNAMIC;
            CBDesc.BindFlags      = BIND_UNIFORM_BUFFER;
            CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
            graphical_app::s_pTheApp->m_pDevice->CreateBuffer(CBDesc, nullptr, &m_VSConstants);
        }

        // Create a pixel shader
        RefCntAutoPtr<IShader> pPS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
            ShaderCI.EntryPoint      = "main";
            ShaderCI.Desc.Name       = "DebugRender pixel shader";
            ShaderCI.Source          = pDebugRenderPSSource;
            graphical_app::s_pTheApp->m_pDevice->CreateShader(ShaderCI, &pPS);
        }

        // clang-format off
        // Define vertex shader input layout
        LayoutElement LayoutElems[] =
        {
            // Attribute 0 - vertex position
            LayoutElement{0, 0, 3, VT_FLOAT32, False},
            // Attribute 1 - vertex uv
            LayoutElement{1, 0, 2, VT_FLOAT32, False},
            // Attribute 2 - vertex color
            LayoutElement{2, 0, 4, VT_UINT8, True}
        };

        // clang-format on
        GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
        GraphicsPipeline.InputLayout.NumElements    = _countof(LayoutElems);

        PSOCreateInfo.pVS = pVS;
        PSOCreateInfo.pPS = pPS;


        // Define variable type that will be used by default
        PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

        for (int state = 0; state < num_blend_states; ++state)
        {
            GraphicsPipeline.BlendDesc = BlendState[state];
            graphical_app::s_pTheApp->m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pPSO[state]);

            if (state > 0)
                VERIFY(m_pPSO[state]->IsCompatibleWith(m_pPSO[0]), "PSOs are expected to be compatible");
        }

        // Since we did not explcitly specify the type for 'Constants' variable, default
        // type (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used. Static variables never
        // change and are bound directly through the pipeline state object.
        {
            int i = 0;
            for( auto pPSO : m_pPSO )
            {
                pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);

                // Create a shader resource binding object and bind all static resources in it
                pPSO->CreateShaderResourceBinding(&m_pSRB[i++], true);
            }
        }
    }

    vertex_buffer* AllocVB( void )
    {
        auto p = m_pFreeVertexB.load( std::memory_order_relaxed );
        do 
        {
            if( nullptr == p )
            {
                return new vertex_buffer{ *this };
            }
            else
            {
                if( m_pFreeVertexB.compare_exchange_weak( p, p->m_pNext ) ) return p;
            }

        } while(true);
    }

    index_buffer* AllocIB( void )
    {
        auto p = m_pFreeIndexB.load( std::memory_order_relaxed );
        do 
        {
            if( nullptr == p )
            {
                return new index_buffer{ *this };
            }
            else
            {
                if( m_pFreeIndexB.compare_exchange_weak( p, p->m_pNext ) ) return p;
            }

        } while(true);
    }

    primitive& AllocTriPrimitive( per_thread& PerThread, int ThreadID, float Order, int nIndices, int nVertex )
    {
        primitive* pPrimitive = nullptr;
        bool       bNewPrim   = false;

        if( PerThread.m_FirstPrimitive == primitive::type::NONE )
        {
            PerThread.m_FirstPrimitive = primitive::type::TRIANGLE;
        }
        else if( PerThread.m_LastPrimitive == primitive::type::LINE )
        {
            PerThread.m_LinePrim.back().m_NextPrimitive = primitive::type::TRIANGLE;
        }

        if( PerThread.m_TrianglePrim.size() == 0 )
        {
            PerThread.m_lpTrianglesVB.push_back( AllocVB() );
            PerThread.m_lpTrianglesIB.push_back( AllocIB() );
            PerThread.m_TrianglePrim.push_back({});
            pPrimitive = &PerThread.m_TrianglePrim.back();

        //    xcore::lock::scope Lk( m_GraphicsAPILock );
            PerThread.m_TriangleVBMap.Map( graphical_app::s_pTheApp->m_pDeferredContexts[ThreadID], PerThread.m_lpTrianglesVB[0]->m_VertexBuffer, MAP_WRITE, MAP_FLAG_DISCARD);
            PerThread.m_TriangleIBMap.Map( graphical_app::s_pTheApp->m_pDeferredContexts[ThreadID], PerThread.m_lpTrianglesIB[0]->m_IndexBuffer, MAP_WRITE, MAP_FLAG_DISCARD);
            bNewPrim = true;

            PerThread.m_TriangleiVertex = 0;
            PerThread.m_TriangleiIndex  = 0;
        }
        else
        {
            pPrimitive = &PerThread.m_TrianglePrim.back();
            if( PerThread.m_LastPrimitive != primitive::type::TRIANGLE )
            {
                assert( pPrimitive->m_NextPrimitive == primitive::type::LINE );
                PerThread.m_TrianglePrim.push_back(PerThread.m_TrianglePrim.back());
                pPrimitive = &PerThread.m_TrianglePrim.back();
                pPrimitive->m_nPrimitives = 0;
                pPrimitive->m_NextPrimitive = primitive::type::NONE;
                bNewPrim = true;
            }
        }

        if( (PerThread.m_TriangleiVertex + nVertex) >= num_verts )
        {
            if( bNewPrim == false )
            {
                pPrimitive->m_NextPrimitive = primitive::type::TRIANGLE;
                PerThread.m_TrianglePrim.push_back(*pPrimitive);
                pPrimitive = &PerThread.m_TrianglePrim.back();
                pPrimitive->m_nPrimitives = 0;
                pPrimitive->m_NextPrimitive = primitive::type::NONE;
                bNewPrim = true;
            }
            pPrimitive->m_iVB       = static_cast<int>(PerThread.m_lpTrianglesVB.size());
            PerThread.m_lpTrianglesVB.push_back( AllocVB() );
          //  xcore::lock::scope Lk( m_GraphicsAPILock );
            PerThread.m_TriangleVBMap.Unmap();
            PerThread.m_TriangleVBMap.Map( graphical_app::s_pTheApp->m_pDeferredContexts[ThreadID],PerThread.m_lpTrianglesVB[pPrimitive->m_iVB]->m_VertexBuffer, MAP_WRITE, MAP_FLAG_DISCARD);
            PerThread.m_TriangleiVertex = 0;
        }

        if( (PerThread.m_TriangleiIndex + nIndices) >= num_indices )
        {
            if( bNewPrim == false )
            {
                pPrimitive->m_NextPrimitive = primitive::type::TRIANGLE;
                PerThread.m_TrianglePrim.push_back(*pPrimitive);
                pPrimitive = &PerThread.m_TrianglePrim.back();
                pPrimitive->m_nPrimitives = 0;
                pPrimitive->m_NextPrimitive = primitive::type::NONE;
                bNewPrim = true;
            }

            pPrimitive->m_iIB      = static_cast<int>(PerThread.m_lpTrianglesIB.size());
            PerThread.m_lpTrianglesIB.push_back( AllocIB() );
           // xcore::lock::scope Lk( m_GraphicsAPILock );
            PerThread.m_TriangleIBMap.Unmap();
            PerThread.m_TriangleIBMap.Map( graphical_app::s_pTheApp->m_pDeferredContexts[ThreadID], PerThread.m_lpTrianglesIB[pPrimitive->m_iIB]->m_IndexBuffer, MAP_WRITE, MAP_FLAG_DISCARD);
            PerThread.m_TriangleiIndex  = 0;
        }

        PerThread.m_LastPrimitive    = primitive::type::TRIANGLE;
        pPrimitive->m_nPrimitives   += nIndices/3;
        PerThread.m_TriangleiVertex += nVertex;
        PerThread.m_TriangleiIndex  += nIndices;

        return *pPrimitive;
    }

    void Draw2DQuad( int ThreadID, float Order, const xcore::vector2& Center, const xcore::vector2& Entends, std::uint32_t Color = ~0 )
    {
        auto&       PerThread   = m_PerFramePerThread[ThreadID][m_iFrame];
        const auto& Prim        = AllocTriPrimitive( PerThread, ThreadID, Order, 6, 4 );
        const auto  iV          = PerThread.m_TriangleiVertex - 4;
        const auto  iI          = PerThread.m_TriangleiIndex  - 6;
        auto&       V1          = PerThread.m_TriangleVBMap[ iV + 0 ];
        auto&       V2          = PerThread.m_TriangleVBMap[ iV + 1 ];
        auto&       V3          = PerThread.m_TriangleVBMap[ iV + 2 ];
        auto&       V4          = PerThread.m_TriangleVBMap[ iV + 3 ];

        V1.m_Position.m_X = Center.m_X - Entends.m_X;
        V1.m_Position.m_Y = Center.m_Y + Entends.m_Y;

        V2.m_Position.m_X = Center.m_X + Entends.m_X;
        V2.m_Position.m_Y = Center.m_Y + Entends.m_Y;

        V3.m_Position.m_X = Center.m_X + Entends.m_X;
        V3.m_Position.m_Y = Center.m_Y - Entends.m_Y;

        V4.m_Position.m_X = Center.m_X - Entends.m_X;
        V4.m_Position.m_Y = Center.m_Y - Entends.m_Y;

        PerThread.m_TriangleIBMap[ iI + 0 ] = iV + 0;
        PerThread.m_TriangleIBMap[ iI + 1 ] = iV + 3;
        PerThread.m_TriangleIBMap[ iI + 2 ] = iV + 1;
        PerThread.m_TriangleIBMap[ iI + 3 ] = iV + 3;
        PerThread.m_TriangleIBMap[ iI + 4 ] = iV + 2;
        PerThread.m_TriangleIBMap[ iI + 5 ] = iV + 1;

        V1.m_Color = Color;
        V2.m_Color = Color;
        V3.m_Color = Color;
        V4.m_Color = Color;
    }

    void msgPostPageFlip( void )
    {
        XCORE_PERF_ZONE_SCOPED_N("Render::msgPostPageFlip")
        m_iFrame = (1 + m_iFrame)%m_PerFramePerThread[0].size();
        for( auto& DoubleBuffer : m_PerFramePerThread )
        {
            auto& PerThread = DoubleBuffer[m_iFrame];

            for( auto pLineVB : PerThread.m_LineVB )
            {
                pLineVB->m_pNext = m_pFreeVertexB.load(std::memory_order_relaxed); 
                m_pFreeVertexB.store( pLineVB, std::memory_order_relaxed );  
            }
            PerThread.m_LineVB.clear();

            for( auto pTriangleVB : PerThread.m_lpTrianglesVB )
            {
                pTriangleVB->m_pNext = m_pFreeVertexB.load(std::memory_order_relaxed); 
                m_pFreeVertexB.store( pTriangleVB, std::memory_order_relaxed );  
            }
            PerThread.m_lpTrianglesVB.clear();

            for( auto pTriangleIB : PerThread.m_lpTrianglesIB )
            {
                pTriangleIB->m_pNext = m_pFreeIndexB.load(std::memory_order_relaxed); 
                m_pFreeIndexB.store( pTriangleIB, std::memory_order_relaxed );  
            }
            PerThread.m_lpTrianglesIB.clear();

            PerThread.m_LinePrim.clear();
            PerThread.m_TrianglePrim.clear();
            PerThread.m_FirstPrimitive = PerThread.m_LastPrimitive = primitive::type::NONE;
        }
    }

    void msgPrePageFlip( void )
    {
        XCORE_PERF_ZONE_SCOPED_N("Render::msgPrePageFlip")
        int PipeLineMode = 1;


        std::array<RefCntAutoPtr<ICommandList>,64> lCmdList;
        std::atomic<bool> DoImGuid{true};
        xcore::scheduler::channel Channel( xconst_universal_str("msgPageFlipChannel"));
        Channel.setupPriority( xcore::scheduler::priority::ABOVE_NORMAL );
        for( int i=0; i<m_PerFramePerThread.size(); ++i )
        {
            auto& DoubleBuffer = m_PerFramePerThread[i];
            auto& PerThread    = DoubleBuffer[m_iFrame];

            if( PerThread.m_FirstPrimitive == primitive::type::NONE )
                continue;

            Channel.SubmitJob( [&DoubleBuffer,&PerThread,i,this,PipeLineMode,&lCmdList,&DoImGuid]
            {
                // Deferred contexts start in default state. We must bind everything to the context.
                // Render targets are set and transitioned to correct states by the main thread, here we only verify the states.
                auto* pRTV = graphical_app::s_pTheApp->m_pSwapChain->GetCurrentBackBufferRTV();
                graphical_app::s_pTheApp->m_pDeferredContexts[i]->SetRenderTargets(1, &pRTV, graphical_app::s_pTheApp->m_pSwapChain->GetDepthBufferDSV(), RESOURCE_STATE_TRANSITION_MODE_VERIFY);


                //
                // Unmap all the relevant buffers
                //
                {
                  //  xcore::lock::scope Lk( m_GraphicsAPILock );
                    if( false == PerThread.m_TrianglePrim.empty() )
                    {
                        PerThread.m_TriangleVBMap.Unmap();
                        PerThread.m_TriangleIBMap.Unmap();
                    }

                    if( false == PerThread.m_LinePrim.empty() )
                    {
                        PerThread.m_LineVBMap.Unmap();
                    }
                }

                {
                    // Set the right matrix
                    MapHelper<float4x4> CBConstants(graphical_app::s_pTheApp->m_pDeferredContexts[i], m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
                    auto M = float4x4::Ortho
                    ( 
                        static_cast<float>(graphical_app::s_pTheApp->m_pSwapChain->GetDesc().Width)
                    ,   static_cast<float>(graphical_app::s_pTheApp->m_pSwapChain->GetDesc().Height)
                    ,   0.1f
                    ,   100.0f
                    ,   false 
                    );
                    *CBConstants = M.Transpose();
                }


                primitive::type CurPrim         = PerThread.m_FirstPrimitive;
                int             iTriPrim        = 0;
                int             iLinePrim       = 0;
                do 
                {
                    if( CurPrim == primitive::type::TRIANGLE )
                    {
                        int iVertexCache    = -1;
                        int iIndexCache     = -1;

                        // Set the pipeline
                        graphical_app::s_pTheApp->m_pDeferredContexts[i]->SetPipelineState(m_pPSO[PipeLineMode]);

                        do 
                        {
                            auto& TriPrim = PerThread.m_TrianglePrim[iTriPrim++];
                            CurPrim       = TriPrim.m_NextPrimitive;

                            if( iVertexCache != TriPrim.m_iVB )
                            {
                                iVertexCache = TriPrim.m_iVB;
                                Uint32   pOffsets[] = { 0u, 0u };
                                IBuffer* pBuffs[]   = { PerThread.m_lpTrianglesVB[ TriPrim.m_iVB ]->m_VertexBuffer };

                                graphical_app::s_pTheApp->m_pDeferredContexts[i]->SetVertexBuffers
                                (
                                    0
                                    ,  _countof(pBuffs)
                                    , pBuffs
                                    , pOffsets
                                    , RESOURCE_STATE_TRANSITION_MODE_TRANSITION
                                    , SET_VERTEX_BUFFERS_FLAG_RESET
                                );
                            }

                            if( iIndexCache != TriPrim.m_iIB )
                            {
                                iIndexCache = TriPrim.m_iIB;
                                graphical_app::s_pTheApp->m_pDeferredContexts[i]->SetIndexBuffer
                                (
                                    PerThread.m_lpTrianglesIB[ TriPrim.m_iIB ]->m_IndexBuffer
                                    , 0
                                    , RESOURCE_STATE_TRANSITION_MODE_TRANSITION
                                );
                            }

                        // Commit shader resources. RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode
                        // makes sure that resources are transitioned to required states.
                        graphical_app::s_pTheApp->m_pDeferredContexts[i]->CommitShaderResources(m_pSRB[PipeLineMode], RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

                            DrawIndexedAttribs DrawAttrs;     // This is an indexed draw call
                            DrawAttrs.IndexType  = VT_UINT32; // Index type
                            DrawAttrs.NumIndices = TriPrim.m_nPrimitives * 3;
                            // Verify the state of vertex and index buffers
                            DrawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
                            graphical_app::s_pTheApp->m_pDeferredContexts[i]->DrawIndexed(DrawAttrs);



                        } while( CurPrim == primitive::type::TRIANGLE );
                    }

                } while ( CurPrim != primitive::type::NONE );

                //
                // Finish the command list
                //
                graphical_app::s_pTheApp->m_pDeferredContexts[i]->FinishCommandList(&lCmdList[i]);

                //
                // Handle ImGUi
                //
                if (graphical_app::s_pTheApp->m_pImGui )
                {
                    if( auto b = DoImGuid.load(std::memory_order_relaxed); b )
                    {
                        do if( DoImGuid.compare_exchange_strong(b,false) )
                        {
                            // No need to call EndFrame as ImGui::Render calls it automatically
                            ITextureView* pRTV = graphical_app::s_pTheApp->m_pSwapChain->GetCurrentBackBufferRTV();
                            ITextureView* pDSV = graphical_app::s_pTheApp->m_pSwapChain->GetDepthBufferDSV();
                            graphical_app::s_pTheApp->m_pDeferredContexts[i]->SetRenderTargets(1, &pRTV, graphical_app::s_pTheApp->m_pSwapChain->GetDepthBufferDSV(), RESOURCE_STATE_TRANSITION_MODE_VERIFY);
                            graphical_app::s_pTheApp->m_pImGui->Render(graphical_app::s_pTheApp->m_pDeferredContexts[i]);
                            graphical_app::s_pTheApp->m_pDeferredContexts[i]->FinishCommandList(&lCmdList[lCmdList.size()-1]);
                            break;
                        } while(b);
                    }
                }

            });
        }
        Channel.join();

        //
        // Handle ImGUi (Just in case there are not drawing of anything)
        //
        if (graphical_app::s_pTheApp->m_pImGui)
        {
            if( auto b = DoImGuid.load(std::memory_order_relaxed); b )
            {
                do if( DoImGuid.compare_exchange_strong(b,false) )
                {
                    // No need to call EndFrame as ImGui::Render calls it automatically
                    ITextureView* pRTV = graphical_app::s_pTheApp->m_pSwapChain->GetCurrentBackBufferRTV();
                    ITextureView* pDSV = graphical_app::s_pTheApp->m_pSwapChain->GetDepthBufferDSV();
                    graphical_app::s_pTheApp->m_pDeferredContexts[0]->SetRenderTargets(1, &pRTV, graphical_app::s_pTheApp->m_pSwapChain->GetDepthBufferDSV(), RESOURCE_STATE_TRANSITION_MODE_VERIFY);
                    graphical_app::s_pTheApp->m_pImGui->Render(graphical_app::s_pTheApp->m_pDeferredContexts[0]);
                    graphical_app::s_pTheApp->m_pDeferredContexts[0]->FinishCommandList(&lCmdList[lCmdList.size()-1]);
                    break;
                } while(b);
            }
        }

        for( auto& E : lCmdList )
        {
            if( E )
            {
                graphical_app::s_pTheApp->m_pImmediateContext->ExecuteCommandList(E);
                E.Release();
            }
        }

        for( int i=0; i<m_PerFramePerThread.size(); ++i )
        {
            // Call FinishFrame() to release dynamic resources allocated by deferred contexts
            // IMPORTANT: we must wait until the command lists are submitted for execution
            // because FinishFrame() invalidates all dynamic resources.
            graphical_app::s_pTheApp->m_pDeferredContexts[i]->FinishFrame();
        }
    }

//    xcore::lock::spin                               m_GraphicsAPILock;
    graphical_app::event::post_page_flip::delegate  m_MyDelegatePostPageFlip    { this, &dbg_render::msgPostPageFlip };
    graphical_app::event::pre_page_flip::delegate   m_MyDelegatePrePageFlip     { this, &dbg_render::msgPrePageFlip  };
    RefCntAutoPtr<IPipelineState>                   m_pPSO[num_blend_states];
    RefCntAutoPtr<IShaderResourceBinding>           m_pSRB[num_blend_states];
    RefCntAutoPtr<IBuffer>                          m_LineIndexBuffer;
    RefCntAutoPtr<IBuffer>                          m_VSConstants;
    std::atomic<vertex_buffer*>                     m_pFreeVertexB              { nullptr };
    std::atomic<index_buffer*>                      m_pFreeIndexB               { nullptr };
    std::vector<std::array<per_thread,2>>           m_PerFramePerThread;
    std::uint64_t                                   m_iFrame{0};
};

void DebugAssertionFailed(const Diligent::Char* Message, const char* Function, const char* File, int Line)
{
    assert(false);
}

//---------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------

static dbg_render g_DBRender;
float s_ZoomFactor = 0.34f;
void Draw2DQuad ( const xcore::vector2& Center, const xcore::vector2& Entends, std::uint32_t Color )
{
    g_DBRender.Draw2DQuad( xcore::get().m_Scheduler.getWorkerUID().m_Value, 0, Center*s_ZoomFactor, Entends*s_ZoomFactor, Color );    
}

void Draw2DWireFrameQuad( const xcore::vector2& Center, const xcore::vector2& Entends, std::uint32_t Color = ~0 )
{
    
}

//
// Main
//

void mainApp(void);

static bool bExit = false;
MSG msg = {0};
void PageFlip( void )
{
    XCORE_PERF_ZONE_SCOPED()

    //
    // Do the page flip
    //
    graphical_app::s_pTheApp->BeginRender();
    graphical_app::s_pTheApp->m_Events.m_prePageFlip.NotifyAll();

    //
    // Read input
    //
    graphical_app::s_pTheApp->m_InputController.ClearState();
    while( true )
    {
        if( WM_QUIT == msg.message )
        {
            bExit = true;
            break;
        }

        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            break;
        }
    }

    //
    // Post update
    //
    graphical_app::s_pTheApp->Present();
    graphical_app::s_pTheApp->m_Events.m_postPageFlip.NotifyAll();

    //
    // Start the new frame
    //
    if (graphical_app::s_pTheApp->m_pImGui)
    {
        const auto& SCDesc = graphical_app::s_pTheApp->m_pSwapChain->GetDesc();
        graphical_app::s_pTheApp->m_pImGui->NewFrame(SCDesc.Width, SCDesc.Height, SCDesc.PreTransform);
    }
}

bool isAppReadyToExit( void )
{
    return bExit;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int cmdShow)
{
    xcore::Init("MEGS-VisualExamples");
    xcore::function::scope CleanUp{ []{ xcore::Kill(); } };

#if defined(_DEBUG) || defined(DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    graphical_app::s_pTheApp.reset(new graphical_app);

    const auto* cmdLine = GetCommandLineA();
    if (!graphical_app::s_pTheApp->ProcessCommandLine(cmdLine))
        return -1;

    std::wstring Title(L"MECS: Win32");
    switch (graphical_app::s_pTheApp->m_DeviceType)
    {
        case RENDER_DEVICE_TYPE_D3D11:  Title.append(L" (D3D11)"); break;
        case RENDER_DEVICE_TYPE_D3D12:  Title.append(L" (D3D12)"); break;
        case RENDER_DEVICE_TYPE_GL:     Title.append(L" (GL)"); break;
        case RENDER_DEVICE_TYPE_VULKAN: Title.append(L" (VK)"); break;
    }
    // Register our window class
    WNDCLASSEX wcex = {sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, MessageProc,
                       0L, 0L, instance, NULL, NULL, NULL, NULL, L"SampleApp", NULL};
    RegisterClassEx(&wcex);

    // Create a window
    LONG WindowWidth  = 1280;
    LONG WindowHeight = 1024;
    RECT rc           = {0, 0, WindowWidth, WindowHeight};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    HWND wnd = CreateWindow(L"SampleApp", Title.c_str(),
                            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                            rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, instance, NULL);
    if (!wnd)
    {
        MessageBox(NULL, L"Cannot create window", L"Error", MB_OK | MB_ICONERROR);
        return 0;
    }
    ShowWindow(wnd, cmdShow);
    UpdateWindow(wnd);

    if (!graphical_app::s_pTheApp->InitializeDiligentEngine(wnd))
        return -1;

    // Initialize Dear ImGUI
    {
        const auto& SCDesc = graphical_app::s_pTheApp->m_pSwapChain->GetDesc();
        graphical_app::s_pTheApp->m_pImGui.reset(new ImGuiImplWin32(wnd, graphical_app::s_pTheApp->m_pDevice, SCDesc.ColorBufferFormat, SCDesc.DepthBufferFormat));
        graphical_app::s_pTheApp->m_pImGui->NewFrame(SCDesc.Width, SCDesc.Height, SCDesc.PreTransform);
    }

//    graphical_app::s_pTheApp->CreateResources();

    g_DBRender.Initialize();

    // let the client application do stuff
    mainApp();

    graphical_app::s_pTheApp.reset();

    return (int)msg.wParam;
}

// Called every time the NativeNativeAppBase receives a message
LRESULT CALLBACK MessageProc(const HWND wnd, const UINT message, const WPARAM wParam, const LPARAM lParam)
{
    bool bHandled = false;
    switch (message)
    {
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            BeginPaint(wnd, &ps);
            EndPaint(wnd, &ps);
            bHandled = true;
            break;
        }
        case WM_SIZE: // Window size has been changed
            if (graphical_app::s_pTheApp)
            {
                graphical_app::s_pTheApp->WindowResize(LOWORD(lParam), HIWORD(lParam));
            }
            bHandled = true;
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            bHandled = true;
            break;

        case WM_GETMINMAXINFO:
        {
            LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;

            lpMMI->ptMinTrackSize.x = 320;
            lpMMI->ptMinTrackSize.y = 240;
            bHandled = true;
            break;
        }
    }

    if (graphical_app::s_pTheApp->m_pImGui)
    {
        bHandled = bHandled || static_cast<ImGuiImplWin32*>(graphical_app::s_pTheApp->m_pImGui.get())->Win32_ProcHandler(wnd, message, wParam, lParam);
    }

    struct WindowsMessageData
    {
        HWND   hWnd;
        UINT   message;
        WPARAM wParam;
        LPARAM lParam;
    } MsgData = {wnd, message, wParam, lParam};
    graphical_app::s_pTheApp->m_InputController.HandleNativeMessage(&MsgData);

    if(bHandled) return 0;
    return DefWindowProc(wnd, message, wParam, lParam);
}


