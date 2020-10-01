
namespace mecs::tools::imgui::views
{
    namespace views = mecs::tools::imgui::views;

    struct group_details
    {
        property::inspector                 m_GroupDetailsInspector {"Group Details"};
        //mecs::component::group::instance*   m_pGroup                {nullptr};

        group_details()
        {
            m_GroupDetailsInspector.setupWindowSize( 220, 350 );
        }

        void Show( mecs::archetype::instance& Instance, mecs::world::instance& World )
        {
            /*
            if( m_pGroup != &Instance )
            {
                m_pGroup = &Instance;
                m_GroupDetailsInspector.clear();
                m_GroupDetailsInspector.AppendEntity();
                m_GroupDetailsInspector.AppendEntityComponent(xcore::property::getTable(Instance),&Instance);
            }
            m_GroupDetailsInspector.Show([&]
            {
                const auto StartX = ImGui::GetCursorPos().x;
                const auto EndX   = StartX + ImGui::GetContentRegionAvail().x*2;
                auto       X      = StartX;
                
                ImGui::PushStyleColor( ImGuiCol_Text,   xcore::endian::Convert( 0xFF ) );
                for( int i=0; i<Instance.m_GroupDescriptor.m_nComponents; ++i  )
                {
                    const auto& C     = Instance.m_GroupDescriptor.m_lComponentTypeGuids[i];
                    const auto& Entry = *World.m_mapComponentDescriptor.get(C);
                    const auto  Color = [&]()->std::uint32_t
                    {
                        switch(Instance.m_GroupDescriptor.m_lComponentDescriptors[i]->m_Type)
                        {
                            default: xassume(false);
                            case mecs::component::type::QUANTUM_DOUBLE_BUFFER: return xcore::endian::Convert( 0xA89B48FF );
                            case mecs::component::type::DOUBLE_BUFFER:         return xcore::endian::Convert( 0x5F8952FF );
                            case mecs::component::type::QUANTUM:               return xcore::endian::Convert( 0xD6D384FF );
                            case mecs::component::type::LINEAR:                return xcore::endian::Convert( 0x90D17DFF );
                            case mecs::component::type::TAG:                   return xcore::endian::Convert( 0xA6DCEAFF );
                        }
                    }();
                    const auto  pSymble = [&]()->const char*
                    {
                        switch(Instance.m_GroupDescriptor.m_lComponentDescriptors[i]->m_Type)
                        {
                            default: xassume(false);
                            case mecs::component::type::QUANTUM_DOUBLE_BUFFER: return "[DBQ]";
                            case mecs::component::type::DOUBLE_BUFFER:         return "[DB]";
                            case mecs::component::type::QUANTUM:               return "[Q]";
                            case mecs::component::type::LINEAR:                return "[L]";
                            case mecs::component::type::TAG:                   return "#";
                        }
                    }();
                    const auto  T     = xcore::string::Fmt( " %s%s ", pSymble, Entry.m_Name.m_Str.m_pValue );
                    const auto  l     = xcore::string::Length(T).m_Value * 16;

                    if( Instance.m_GroupDescriptor.m_lComponentDescriptors[i]->m_Type == mecs::component::type::QUANTUM_DOUBLE_BUFFER 
                    ||  Instance.m_GroupDescriptor.m_lComponentDescriptors[i]->m_Type == mecs::component::type::DOUBLE_BUFFER )
                    {
                        i++;
                    }

                    if( (X + l) > EndX )
                    {
                        X = StartX;
                        //Make it go to the next line
                        ImGui::Text("");    
                    }

                    ImGui::PushStyleColor( ImGuiCol_Button, Color );
                    ImGui::Button( T );
                    ImGui::PopStyleColor(1);
                    ImGui::SameLine();
                    X += l;
                }
                ImGui::Text("");
                ImGui::PopStyleColor(1);
            });
            */
        }
    };

    struct select_components
    {
        xcore::vector<mecs::component::type_guid>   m_Selected   {};
        xcore::vector<mecs::component::type_guid>   m_lExclude   {};
        bool                                        m_isOpen     = true;
        bool                                        m_isCancel   = false;
        void clear()
        {
            m_Selected.clear();
            m_lExclude.clear();
            m_isCancel = false;
        }

        bool isClose()
        {
            return !m_isOpen;
        }

        bool isCancel()
        {
            return m_isCancel;
        }

        bool Show( mecs::world::instance& World )
        {
            m_isOpen = ImGui::BeginPopup("Select Components", ImGuiWindowFlags_AlwaysAutoResize);
            if ( m_isOpen == false )
                return false;

            const auto StartX = ImGui::GetCursorPos().x;
            const auto EndX   = StartX + ImGui::GetContentRegionAvail().x*2;
            auto       X      = StartX;
            ImGui::PushStyleColor( ImGuiCol_Text,   xcore::endian::Convert( 0xFF ) );
            /*
            for( const auto& E : World.m_mapComponentDescriptor.m_Map )
            {
                if( E.m_pEntry == nullptr ) continue;

                const auto Key = E.m_pEntry->m_Key;

                // Check for exclusion
                {
                    bool Exclude = false;
                    for( const auto X : m_lExclude )
                    {
                        if( Key == X )
                        {
                            Exclude = true;
                            break;
                        }
                    }
                    if(Exclude) continue;
                }

                //
                // Render Components
                //
                {
                    const auto& Entry = *World.m_mapComponentDescriptor.get(Key);
                    const auto  Color = [&]()->std::uint32_t
                    {
                        switch(Entry.m_Type)
                        {
                            default: xassume(false);
                            case mecs::component::type::QUANTUM_DOUBLE_BUFFER: return xcore::endian::Convert( 0xA89B48FF );
                            case mecs::component::type::DOUBLE_BUFFER:         return xcore::endian::Convert( 0x5F8952FF );
                            case mecs::component::type::QUANTUM:               return xcore::endian::Convert( 0xD6D384FF );
                            case mecs::component::type::LINEAR:                return xcore::endian::Convert( 0x90D17DFF );
                            case mecs::component::type::TAG:                   return xcore::endian::Convert( 0xA6DCEAFF );
                        }
                    }();
                    const auto  pSymble = [&]()->const char*
                    {
                        switch(Entry.m_Type)
                        {
                            default: xassume(false);
                            case mecs::component::type::QUANTUM_DOUBLE_BUFFER: return "[QDB]";
                            case mecs::component::type::DOUBLE_BUFFER:         return "[DB]";
                            case mecs::component::type::QUANTUM:               return "[Q]";
                            case mecs::component::type::LINEAR:                return "[L]";
                            case mecs::component::type::TAG:                   return "#";
                        }
                    }();
                    const auto  T     = xcore::string::Fmt( "%s%s", pSymble, Entry.m_Name.m_Str.m_pValue );
                    const auto  l     = xcore::string::Length(T).m_Value * 16;

                    if( (X + l) > EndX )
                    {
                        X = StartX;
                        //Make it go to the next line
                        ImGui::Text("");    
                    }

                    ImGui::PushStyleColor( ImGuiCol_Button, Color );

                    int Index = -1;
                    for( auto& E : m_Selected )
                    {
                        if( E == Key )
                        {
                            Index = m_Selected.getIndexByEntry<int>(E);
                            break;
                        }
                    }
                    if( Index == -1 )
                    {
                        if( ImGui::Button( T ) )
                        {
                            m_Selected.append(Key);
                        }
                    }
                    else
                    {
                        ImGui::Bullet();
                        if( ImGui::Button( T ) )
                        {
                            m_Selected.eraseCollapse( Index );
                        }
                    }
                    ImGui::PopStyleColor(1);
                    ImGui::SameLine();
                    X += l;
                }
            }
            */

            ImGui::Text("");
            if( ImGui::Button( "OK" ) )
            {
                m_isCancel = false;
                m_isOpen   = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if( ImGui::Button( "Cancel" ) )
            {
                m_isCancel = true;
                m_isOpen   = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if( ImGui::Button( "Clear" ) )
            {
                m_Selected.clear();
            }

            ImGui::PopStyleColor(1);
            ImGui::EndPopup();

            return m_isOpen == false;
        }
    };

    struct group_list
    {
        enum class query_type
        {
            NONE, MUST, MAY, CANT
        };

        query_type                                  m_ActiveQuery           = query_type::NONE;
        xcore::vector<mecs::component::type_guid>   m_Must                  {};
        xcore::vector<mecs::component::type_guid>   m_May                   {};
        xcore::vector<mecs::component::type_guid>   m_Cant                  {};
        xcore::vector<mecs::component::type_guid>   m_Exclude               {};
        select_components                           m_SelectComponentsView  {};

        bool                                        m_bWindowOpen           = true;
        mecs::archetype::instance*                  m_pSelected             = nullptr;

        /*
        mecs::query::instance::component_query      m_ComponentQuery        {};
        mecs::query::instance::tag_query            m_TagQuery              {};
        */

        constexpr group_list() = default;

        constexpr group_list( bool bOpen ) noexcept : m_bWindowOpen(bOpen)
        {
        }

        void clear()
        {
            m_pSelected = nullptr;
            /*
            m_ComponentQuery.m_All.clear();
            m_ComponentQuery.m_Any.clear();
            m_ComponentQuery.m_None.clear();

            m_TagQuery.m_All.clear();
            m_TagQuery.m_Any.clear();
            m_TagQuery.m_None.clear();
            */
        }

        void RebuildQueries( mecs::world::instance& World )
        {
            clear();
            /*
            for( const auto& E : m_Must )
            {
                const auto& Entry = *World.m_mapComponentDescriptor.get(E);
                if( Entry.m_Type == mecs::component::type::TAG )    m_TagQuery.m_All.AddBit( Entry.m_BitNumber );
                else                                                m_ComponentQuery.m_All.AddBit( Entry.m_BitNumber );
            }

            for( const auto& E : m_May )
            {
                const auto& Entry = *World.m_mapComponentDescriptor.get(E);
                if( Entry.m_Type == mecs::component::type::TAG )    m_TagQuery.m_Any.AddBit( Entry.m_BitNumber );
                else                                                m_ComponentQuery.m_Any.AddBit( Entry.m_BitNumber );
            }

            for( const auto& E : m_Cant )
            {
                const auto& Entry = *World.m_mapComponentDescriptor.get(E);
                if( Entry.m_Type == mecs::component::type::TAG )    m_TagQuery.m_None.AddBit( Entry.m_BitNumber );
                else                                                m_ComponentQuery.m_None.AddBit( Entry.m_BitNumber );
            }
            */
        }

        void DisplayComponents( xcore::vector<mecs::component::type_guid>& List, mecs::world::instance& World )
        {
            const auto StartX = ImGui::GetCursorPos().x;
            const auto EndX   = StartX + ImGui::GetContentRegionAvail().x*2;
            auto       X      = StartX;
            ImGui::PushStyleColor( ImGuiCol_Text,   xcore::endian::Convert( 0xFF ) );
            /*
            for( int i=0; i<List.size(); ++i)
            {
                const auto& Key = List[i];

                //
                // Render Components
                //
                {
                    const auto& Entry = *World.m_mapComponentDescriptor.get(Key);
                    const auto  Color = [&]()->std::uint32_t
                    {
                        switch(Entry.m_Type)
                        {
                            default: xassume(false);
                            case mecs::component::type::QUANTUM_DOUBLE_BUFFER: return xcore::endian::Convert( 0xA89B48FF );
                            case mecs::component::type::DOUBLE_BUFFER:         return xcore::endian::Convert( 0x5F8952FF );
                            case mecs::component::type::QUANTUM:               return xcore::endian::Convert( 0xD6D384FF );
                            case mecs::component::type::LINEAR:                return xcore::endian::Convert( 0x90D17DFF );
                            case mecs::component::type::TAG:                   return xcore::endian::Convert( 0xA6DCEAFF );
                        }
                    }();
                    const auto  pSymble = [&]()->const char*
                    {
                        switch(Entry.m_Type)
                        {
                            default: xassume(false);
                            case mecs::component::type::QUANTUM_DOUBLE_BUFFER: return "[QDB]";
                            case mecs::component::type::DOUBLE_BUFFER:         return "[DB]";
                            case mecs::component::type::QUANTUM:               return "[Q]";
                            case mecs::component::type::LINEAR:                return "[L]";
                            case mecs::component::type::TAG:                   return "#";
                        }
                    }();
                    const auto  T     = xcore::string::Fmt( "%s%s", pSymble, Entry.m_Name.m_Str.m_pValue );
                    const auto  l     = xcore::string::Length(T).m_Value * 16;

                    if( (X + l) > EndX )
                    {
                        X = StartX;
                        //Make it go to the next line
                        ImGui::Text("");    
                    }

                    ImGui::PushStyleColor( ImGuiCol_Button, Color );
                    if( ImGui::Button( T ) )
                    {
                        List.eraseCollapse(List.getIndexByEntry( Key ));
                        RebuildQueries(World);
                        --i;
                    }
                    ImGui::PopStyleColor(1);
                    ImGui::SameLine();
                    X += l;
                }
            }
            */
            ImGui::Text("");
            ImGui::PopStyleColor(1);
        }

        constexpr bool isWindowOpen() const noexcept
        {
            return m_bWindowOpen;
        }

        void Show( mecs::world::instance& World )
        {
            /*
            mecs::component::group::instance* pSelected = nullptr;
            if ( !ImGui::Begin( "Group DataBase", &m_bWindowOpen ) )
            {
                ImGui::End();
                return;
            }

            if( ImGui::Button(" Must ") )
            {
                m_ActiveQuery = query_type::MUST;
            }

            ImGui::SameLine();
            if(false == m_Must.empty()) DisplayComponents(m_Must,World);
            else if(false == m_May.empty())    ImGui::Text("");

            if( ImGui::Button(" May ") )
            {
                m_ActiveQuery = query_type::MAY;
            }
            ImGui::SameLine();
            if(false == m_May.empty())          DisplayComponents(m_May,World);
            else if(false == m_Cant.empty())    ImGui::Text("");

            if( ImGui::Button(" Can't ") )
            {
                m_ActiveQuery = query_type::CANT;
            }
            ImGui::SameLine();
            if(false == m_Cant.empty()) DisplayComponents(m_Cant,World);
            else ImGui::Text("");

            ImGui::Separator();

            for( auto i= 0u; i<World.m_TagDBase.m_nTagEntries; ++i )
            {
                auto& TagEntries = World.m_TagDBase.m_lTagEntries[i];

                if( false == TagEntries.m_TagBits.Query( m_TagQuery.m_All, m_TagQuery.m_Any, m_TagQuery.m_None ) )
                    continue;

                xcore::string::fixed<char,512>  Name;
                if( TagEntries.m_nTags == 0 )
                {
                    xcore::string::sprintf( Name.getView(), "No Tags" );
                }
                else
                {
                    int iCurrent=0;
                    for( auto t=0u; t<TagEntries.m_nTags; ++t )
                    {
                        auto& Entry = *World.m_mapComponentDescriptor.get(TagEntries.m_lTagGuids[t]);
                        iCurrent += xcore::string::sprintf( Name.getViewFrom(iCurrent), "#%s ", Entry.m_Name.m_Str.m_pValue ).m_Value;
                    }
                }

                if( ImGui::TreeNodeEx(Name, ImGuiTreeNodeFlags_DefaultOpen ) )
                {
                    for( auto g=0u; g<TagEntries.m_nGroups; ++g )
                    {
                        if( false == TagEntries.m_lGroups[g].m_GroupBits.Query( m_ComponentQuery.m_All, m_ComponentQuery.m_Any, m_ComponentQuery.m_None ) )
                            continue;

                        auto& Group = *TagEntries.m_lGroups[g].m_pGroup;

                        if(1)
                        {
                            int iName = 0;
                            if(Group.m_GroupDescriptor.m_nComponents<=1)
                            {
                                xcore::string::Copy( Name, "entity" );
                            }
                            else
                            {
                                for( int i=1, end=Group.m_GroupDescriptor.m_nComponents; i<end; ++i )
                                {
                                    if(i>1) Name[iName++] = ',';
                                    auto& Entry = *World.m_mapComponentDescriptor.get(Group.m_GroupDescriptor.m_lComponentTypeGuids[i]);
                                    iName += xcore::string::Copy( Name.getViewFrom(iName), Entry.m_Name.m_Str ).m_Value;
                                    if(Entry.m_Type == mecs::component::type::QUANTUM_DOUBLE_BUFFER 
                                     ||Entry.m_Type == mecs::component::type::DOUBLE_BUFFER ) i++;
                                }
                            }
                        }
                        else
                        {
                            Group.m_Guid.getStringHex(Name.getView());
                        }

                        auto Flags = m_pSelected == &Group;
                        ImGui::Selectable(Name, Flags);
                        if (ImGui::IsItemClicked())
                        {
                            pSelected = &Group;
                        }
                        else if( pSelected == nullptr && Flags )
                        {
                            pSelected = m_pSelected;
                        }
                    }
                    ImGui::TreePop();
                }
            }

            ImGui::End();

            m_pSelected = pSelected;

            if( m_ActiveQuery != query_type::NONE && m_SelectComponentsView.isClose() )
            {
                m_SelectComponentsView.clear();
                switch(m_ActiveQuery)
                {
                    case query_type::CANT:  m_SelectComponentsView.m_Selected = m_Cant;
                                            m_SelectComponentsView.m_lExclude.appendList(m_May);
                                            m_SelectComponentsView.m_lExclude.appendList(m_Must);
                                            break;
                    case query_type::MAY:   m_SelectComponentsView.m_Selected = m_May;  
                                            m_SelectComponentsView.m_lExclude.appendList(m_Cant);
                                            m_SelectComponentsView.m_lExclude.appendList(m_Must);
                                            break;
                    case query_type::MUST:  m_SelectComponentsView.m_Selected = m_Must; 
                                            m_SelectComponentsView.m_lExclude.appendList(m_Cant);
                                            m_SelectComponentsView.m_lExclude.appendList(m_May);
                                            break;
                }

                
                ImGui::OpenPopup("Select Components");
            }

            if( m_SelectComponentsView.Show( World ) )
            {
                if( m_SelectComponentsView.isCancel() == false )
                {
                    switch(m_ActiveQuery)
                    {
                        case query_type::CANT: m_Cant = std::move(m_SelectComponentsView.m_Selected); break;
                        case query_type::MAY:  m_May  = std::move(m_SelectComponentsView.m_Selected); break;
                        case query_type::MUST: m_Must = std::move(m_SelectComponentsView.m_Selected); break;
                    }

                    RebuildQueries(World);
                }
                m_ActiveQuery = query_type::NONE;
            }
            */
        }
    };
}
