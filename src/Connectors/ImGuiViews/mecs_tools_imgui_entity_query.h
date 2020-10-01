namespace mecs::tools::imgui::views
{
    namespace views = mecs::tools::imgui::views;

    struct entity_query
    {
        void Show( void )
        {
            if ( !ImGui::Begin( "Entity Query", &m_bWindowOpen ) )
            {
                ImGui::End();
                return;
            }

            if( ImGui::Button("[+]") )
            {
                
            }
            ImGui::SameLine();
            ImGui::Text("Must Have:");

            ImGui::Text("");
            ImGui::Separator();
            if( ImGui::Button("[+]") )
            {
                
            }
            ImGui::SameLine();
            ImGui::Text("May Have:");

            ImGui::Text("");
            ImGui::Separator();
            if( ImGui::Button("[+]") )
            {
                
            }
            ImGui::SameLine();
            ImGui::Text("Must Not Have:");

            ImGui::Text("");
            ImGui::Separator();

            ImGui::End();
        }

        bool m_bWindowOpen = true;
    };



}