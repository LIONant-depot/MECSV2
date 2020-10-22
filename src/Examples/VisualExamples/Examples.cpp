#include "../DiligentTools/ThirdParty/imgui/imgui.h"
#include <variant>

#define NOMINMAX
#include <windows.h>
#include <memoryapi.h>

#define PROPERTY_EDITOR
#include "mecs.h"
#include "../../dependencies/xcore/dependencies/properties/src/Examples/ImGuiExample/ImGuiPropertyInspector.h"
#include "../../dependencies/xcore/dependencies/properties/src/Examples/ImGuiExample/ImGuiPropertyInspector.cpp"

#include "../../src/Connectors/ImGuiViews/mecs_tools_imgui_group_view.h"
#include "../../src/Connectors/ImGuiViews/mecs_tools_imgui_entity_query.h"

void PageFlip           ( void );
void Draw2DQuad         ( const xcore::vector2& Center, const xcore::vector2& Entends, std::uint32_t Color = ~0 );

#include "Examples/VisualExamples/E01_graphical_2d_basic_physics.h"
#include "Examples/VisualExamples/E02_graphical_2d_basic_physics.h"
#include "Examples/VisualExamples/E03_graphical_2d_basic_physics.h"
#include "Examples/VisualExamples/E04_graphical_2d_basic_physics.h"
/*
#include "Examples/GraphicsExamples/E04_graphical_2d_boids.h"
#include "Examples/GraphicsExamples/E05_graphical_2d_virus_spread.h"
#include "Examples/GraphicsExamples/E06_graphical_2d_GPU_physics.h"
#include "Examples/GraphicsExamples/E01_graphical_gridbase_pathfinding.h"
*/

bool isAppReadyToExit( void );
extern float s_ZoomFactor;
static float s_ExpectedZoom = s_ZoomFactor;

//----------------------------------------------------------------------------------------------------
// Menu system
//----------------------------------------------------------------------------------------------------
namespace menu
{
    struct global_settings
    {
    };

    struct entry
    {
        using fn = void();
        constexpr entry( fn* p, const char* n ) : m_pFunction{p}, m_pName{n}{}
        fn*         m_pFunction;
        const char* m_pName;
    };

    struct category
    {
        const char*             m_pName;
        std::span<const entry>  m_lEntries;
    };

    static global_settings                          s_GlobalSettings    {};
    static const entry*                             s_pActive           = nullptr;
    static property::inspector                      s_Inspector         {"Inspector"};
    static mecs::tools::imgui::views::group_list    s_GroupListView     (false);
    static mecs::tools::imgui::views::group_details s_GroupDetailView   {};
    static bool                                     s_bImGuiDemo        = false;
    static property::inspector                      s_PropertySettings  {"Property Settings", false };

    template< auto& T_CATEGORIES_V > static
    bool Menu( mecs::world::instance& WI, xcore::property::base& Base ) noexcept
    {
        static bool     b       = true;
        const entry*    pActive = nullptr;

        // Handle the camera zoom
        s_ZoomFactor += (s_ExpectedZoom - s_ZoomFactor)
                     * xcore::Min( 2.0f, xcore::Sqr(s_ExpectedZoom - s_ZoomFactor)+1.8f )
                     * WI.m_GraphDB.m_Time.m_DeltaTime;

        if( s_GroupListView.isWindowOpen() )
        {
            ImGui::SetNextWindowSize( ImVec2( 220, 350 ), ImGuiCond_FirstUseEver );
            ImGui::SetNextWindowPos(  ImVec2( static_cast<float>(400), static_cast<float>(300) ), ImGuiCond_FirstUseEver );
            s_GroupListView.Show(WI);

            if(s_GroupListView.m_pSelected)
            {
                s_GroupDetailView.Show(*s_GroupListView.m_pSelected,WI);
            }
        }


        if(s_bImGuiDemo) ImGui::ShowDemoWindow(&s_bImGuiDemo);

        s_PropertySettings.Show([]{});

        s_Inspector.Show([&]
        {
            if (ImGui::Button(" Example Menu "))
                ImGui::OpenPopup("exampleMenu");

            ImGui::SameLine();
            if( ImGui::Button(" Use Settings ") )
            {
                pActive = s_pActive;
            }

            if (ImGui::BeginPopup("exampleMenu", ImGuiWindowFlags_AlwaysAutoResize))
            {
                for( auto& C : T_CATEGORIES_V ) if( ImGui::BeginMenu( C.m_pName ) )
                {
                    for( auto& E : C.m_lEntries ) if( ImGui::MenuItem( E.m_pName, nullptr) )
                    {
                        pActive = &E;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndMenu();
                }

                if( ImGui::MenuItem( "Group DBase", nullptr) )
                {
                    s_GroupListView.m_bWindowOpen = true;
                }

                if( ImGui::MenuItem( "ImGui Demo", nullptr) )
                {
                    s_bImGuiDemo = true;
                }

                if( ImGui::MenuItem( "Property Settings", nullptr) )
                {
                    s_PropertySettings.setOpenWindow(true);
                    s_PropertySettings.clear();
                    s_PropertySettings.AppendEntity();
                    s_PropertySettings.AppendEntityComponent(xcore::property::getTable(s_Inspector),&s_Inspector);
                }

                ImGui::EndPopup();
            }


            ImGui::TextDisabled( "");
            ImGui::TextDisabled( " %s, FPS: %3.2f", s_pActive->m_pName, WI.m_GraphDB.m_Time.getAvgFPS() );
            ImGui::TextDisabled( "");

            ImGui::Separator();
        });

        // Let the user know if things have change
        if( pActive )
        {
            s_Inspector.clear();
            s_GroupListView.clear();
            s_pActive = pActive;
            return false;
        }

        // If we dont have any property sets let make sure we get some
        if( s_Inspector.isValid() == false )
        {
            s_Inspector.AppendEntity            ();
            s_Inspector.AppendEntityComponent   ( property::getTable(Base), &Base );
            s_Inspector.AppendEntityComponent   ( property::getTable(s_GlobalSettings), &s_GlobalSettings );
        }

        // if the app wants us to exit or the inspector window is close
        // then exit the app gracefully
        if( isAppReadyToExit() || b == false )
        {
            s_pActive = nullptr;
            return false;
        }
        
        return true;
    }
}

//----------------------------------------------------------------------------------------------------
// Main App
//----------------------------------------------------------------------------------------------------

extern const std::span<const menu::category> s_Categories;

static constexpr std::array s_SimplePhysicsDemos
{
    menu::entry{ []{mecs::examples::E01_graphical_2d_basic_physics::Test(menu::Menu<s_Categories>);},  "Standard Grid"       }
,   menu::entry{ []{mecs::examples::E02_graphical_2d_basic_physics::Test(menu::Menu<s_Categories>);},  "Minimal Grid"        }
,   menu::entry{ []{mecs::examples::E03_graphical_2d_basic_physics::Test(menu::Menu<s_Categories>);},  "Minimal & Compact"   }
,   menu::entry{ []{mecs::examples::E04_graphical_2d_basic_physics::Test(menu::Menu<s_Categories>);}, "Singleton"           }
/*
,   menu::entry{ []{mecs::examples::E04_graphical_2d_boids::        Test(menu::Menu<s_Categories>);}, "Boids example"       }
,   menu::entry{ []{mecs::examples::E05_graphical_2d_virus_spread ::Test(menu::Menu<s_Categories>);}, "Virus example"       }
,   menu::entry{ []{mecs::examples::E06_graphical_2d_gpu_physics  ::Test(menu::Menu<s_Categories>);}, "GPU Physics"         }
*/
};

static constexpr std::array s_CategoriesStorage
{
    menu::category{ "Simple Physics", s_SimplePhysicsDemos }
};

static const std::span<const menu::category> s_Categories = s_CategoriesStorage;

void mainApp()
{
    // Set the default example
    menu::s_pActive = &s_Categories[0].m_lEntries[3];

    // Run until the app wants us to quit
    menu::s_Inspector.setupWindowSize( 220, 450 );
    for( ; menu::s_pActive; menu::s_pActive->m_pFunction());
}

property_begin_name( menu::global_settings, "Global" )
{
    property_var_fnbegin( "Zoom", float )
    {
        static constexpr float Min = 0.34f;
        static constexpr float Max = 10.0f;
        if( isRead )
        {
            if( s_ExpectedZoom >= 1.0f )  InOut = 50 + (s_ExpectedZoom/Max)*50.0f;
            else                          InOut = (((s_ExpectedZoom- Min)/(1-Min)))*50.0f;
        }
        else
        {
            if( InOut >= 50.0f ) s_ExpectedZoom = 1   + ((InOut - 50.0f)/50.0f)*(Max-1);
            else                 s_ExpectedZoom = Min + (InOut/50.0f)*(1-Min);
        }
    }
    property_var_fnend()
        .EDStyle(property::edstyle<float>::ScrollBar( 0, 100.0f, "%3.0f%%" ))
        .Help(  "Changes the camera to zoom in/out.\n"
                "This option is done in real time.")
}
property_end();


