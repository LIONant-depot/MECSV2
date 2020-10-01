#ifndef MEGS_GRAPHICAL_APP_H
#define MEGS_GRAPHICAL_APP_H

#include "MEGS-Diligent.h"
#include "xcore.h"
#include "../DiligentTools/Imgui/interface/ImGuiImplDiligent.hpp"
#include "../DiligentTools/Imgui/interface/ImGuiImplWin32.hpp"
#include "InputController.hpp"

class graphical_app
{
public:
    graphical_app();
    ~graphical_app();

    struct event
    {
        using  post_page_flip = xcore::types::make_unique< xcore::event::type<>, struct post_page_flip_tag >;
        using  pre_page_flip  = xcore::types::make_unique< xcore::event::type<>, struct pre_page_flip_tag >;

        post_page_flip  m_prePageFlip;
        pre_page_flip   m_postPageFlip;
    };


    bool                            InitializeDiligentEngine       ( HWND hWnd );
    bool                            ProcessCommandLine             ( const char* CmdLine );
    void                            BeginRender                    ( void );
    void                            Present                        ( void );
    void                            WindowResize                   ( Diligent::Uint32 Width, Diligent::Uint32 Height );

    bool                                                            m_bBeginRender          = false;
    Diligent::RefCntAutoPtr<Diligent::IEngineFactory>               m_pEngineFactory        {};
    std::unique_ptr<Diligent::ImGuiImplDiligent>                    m_pImGui                {};
    std::vector<Diligent::RefCntAutoPtr<Diligent::IDeviceContext>>  m_pDeferredContexts     {};
    event                                                           m_Events                {};
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice>                m_pDevice               {};
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext>               m_pImmediateContext     {};
    Diligent::RefCntAutoPtr<Diligent::ISwapChain>                   m_pSwapChain            {};
    Diligent::RENDER_DEVICE_TYPE                                    m_DeviceType            = Diligent::RENDER_DEVICE_TYPE_D3D12;
    Diligent::InputController                                       m_InputController       {};

    static std::unique_ptr<graphical_app>                           s_pTheApp;
};

#endif