namespace mecs::entity_pool
{
    //---------------------------------------------------------------------------------
    // POOLS::EVENTS
    //---------------------------------------------------------------------------------
    struct instance
    {
        constexpr static auto           page_size_v     = 4096;
        constexpr static auto           page_size_pow_v = xcore::bits::Log2Int(page_size_v);
        using                           self            = instance;

        inline                         ~instance                ( void )                                                            noexcept;
        inline      void                Init                    ( mecs::archetype::instance& Archetype, const std::span<const mecs::component::descriptor* const> Descriptors, index MaxEntries )          noexcept;
        inline      void                Kill                    ( void )                                                            noexcept;
        inline      void                clear                   ( void )                                                            noexcept;
        inline      std::uint32_t       append                  ( int nEntries = 1 )                                                noexcept;
        inline      void                MemoryBarrier           ( void )                                                            noexcept;
        inline      void                deleteBySwap            ( const index Index )                                               noexcept;
        template< typename T_COMPONENT >
        inline      T_COMPONENT&        get                     ( const index Index )                                               noexcept;
        template< typename T_COMPONENT >
        constexpr   const T_COMPONENT&  get                     ( const index Index ) const                                         noexcept;
        constexpr   auto                size                    ( void ) const                                                      noexcept{ return m_Count;       }
        constexpr   auto                capacity                ( void ) const                                                      noexcept{ return m_MaxEntries;  }
        template< typename T_COMPONENT, bool T_DO_CONVERSIONS_V = true >
        inline      T_COMPONENT&        getComponentByIndex     ( const index Index, int iComponent )                               noexcept;
        template< typename T_COMPONENT, bool T_DO_CONVERSIONS_V = true >
        inline      const T_COMPONENT&  getComponentByIndex     ( const index Index, int iComponent )                               const noexcept;
        inline      std::byte*          getComponentByIndexRaw  ( const index Index, int iComponent )                               noexcept;
        xforceinline auto&              getDescriptors          ( void ) const                                                      noexcept {return m_Descriptors; }


        inline      void                DeletePendingEntries    ( void )                                                            noexcept;
        inline      auto                FreeCount               ( void )                                                            const noexcept { return m_MaxEntries - m_Count; }
        inline      void                UpdateCount             ( void )                                                            noexcept       { m_Count = m_NewCount.load().m_Count; }

        union counter
        {
            index                               m_FullValue;
            struct
            {
                index                           m_Count:31
                            ,                   m_Lock:1;
            };
        };

        using ptr_array = std::array<std::byte*, mecs::settings::max_data_components_per_entity>;


        std::atomic<counter>                                    m_NewCount              {{0}};
        index                                                   m_Count                 {0};
        ptr_array                                               m_Pointers              {};
        std::span<const mecs::component::descriptor* const>     m_Descriptors           {};
        index                                                   m_MaxEntries            {0};
        mecs::tools::qvstp<std::uint32_t>                       m_DeleteList            {};
        mecs::archetype::instance*                              m_pArchetype            {nullptr};
    };
}