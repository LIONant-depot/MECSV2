namespace mecs::archetype::query
{

    //---------------------------------------------------------------------------------
    // QUERY:: INSTANCE
    //---------------------------------------------------------------------------------
    template
    < typename  T_SYSTEM_FUNCTION
    , auto&     T_DEFINED_V
    >
    xforceinline
    void instance::Initialize( void ) noexcept
    {
        static_assert(std::is_convertible_v< decltype(T_DEFINED_V), query::details::define_data >);

        if( m_isInitialized ) return;
        m_isInitialized = true;

        static constexpr query::details::define<T_SYSTEM_FUNCTION> function_define_data_v{};

        //
        // Handle the Component Side
        //
        for( auto E : T_DEFINED_V.m_ComponentQuery.m_All )
            m_ComponentQuery.m_All.AddBit( std::get<0>(E).m_BitNumber );

        for (auto E : T_DEFINED_V.m_ComponentQuery.m_Any)
            m_ComponentQuery.m_Any.AddBit( std::get<0>(E).m_BitNumber );

        for (auto E : T_DEFINED_V.m_ComponentQuery.m_None)
            m_ComponentQuery.m_None.AddBit( std::get<0>(E).m_BitNumber );

        //
        // Add the contribution from the function parameters
        //
        if constexpr (function_define_data_v.m_ComponentQuery.m_All.size()) for( auto& E : function_define_data_v.m_ComponentQuery.m_All )
        {
            // DEBUG WARNING: Make sure all components are register!!!
            const auto& Descriptor = std::get<0>(E);
            m_ComponentQuery.m_All.AddBit(Descriptor.m_BitNumber);
            if( std::get<1>(E) ) m_Write.AddBit(Descriptor.m_BitNumber);
        }

        if constexpr (function_define_data_v.m_ComponentQuery.m_Any.size()) for (auto& E : function_define_data_v.m_ComponentQuery.m_Any )
        {
            // DEBUG WARNING: Make sure all components are register!!!
            const auto& Descriptor = std::get<0>(E);
            m_ComponentQuery.m_Any.AddBit(Descriptor.m_BitNumber);
            if( std::get<1>(E) ) m_Write.AddBit(Descriptor.m_BitNumber);
        }

        //
        // Handle the tag side
        //
        for (auto E : T_DEFINED_V.m_TagQuery.m_All)
            m_TagQuery.m_All.AddBit(std::get<0>(E).m_BitNumber);

        for (auto E : T_DEFINED_V.m_TagQuery.m_Any)
            m_TagQuery.m_Any.AddBit(std::get<0>(E).m_BitNumber);

        for (auto E : T_DEFINED_V.m_TagQuery.m_None)
            m_TagQuery.m_None.AddBit(std::get<0>(E).m_BitNumber);
    }

    //---------------------------------------------------------------------------------

    inline
    bool instance::TryAppendArchetype(mecs::archetype::instance& Archetype) noexcept
    {
        if (false == Archetype.m_ArchitypeBits.Query(m_ComponentQuery.m_All, m_ComponentQuery.m_Any, m_ComponentQuery.m_None)) return false;
        if (false == Archetype.m_ArchitypeBits.Query(m_TagQuery.m_All,       m_TagQuery.m_Any,       m_TagQuery.m_None)      ) return false;

        auto& R = m_lResults.append();
        R.m_pArchetype  = &Archetype;
        R.m_nParameters = 0;
        return true;
    }
}