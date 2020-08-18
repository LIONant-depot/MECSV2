namespace mecs::entity_pool
{
    //---------------------------------------------------------------------------------
    // POOLS::EVENTS
    //---------------------------------------------------------------------------------
    class instance
    {
    public:

        constexpr static auto           page_size_v     = 4096;
        constexpr static auto           page_size_pow_v = xcore::bits::Log2Int(page_size_v);
        using                           self            = instance;

        /*
        struct iterator
        {
            self&       m_This;
            std::size_t m_I;

            inline      auto operator   ++  (void)                    noexcept { m_I++; return *this; }
            constexpr   bool operator   !=  (std::nullptr_t)  const   noexcept { return m_This.m_Count != m_I; }
            constexpr   auto operator   *   (void)            const   noexcept { return std::tuple<const T_TYPES&...> { std::get<T_TYPES*>(m_This.m_Pointers)[m_I]... }; }
            constexpr   auto operator   *   (void)                    noexcept { return std::tuple<T_TYPES&...>       { std::get<T_TYPES*>(m_This.m_Pointers)[m_I]... }; }
        };
        */

        /*
        static bool b = true;
        if (b)
        {
            SYSTEM_INFO sSysInfo;
            GetSystemInfo(&sSysInfo);
            b = false;
            xassert(page_size_v <= sSysInfo.dwPageSize);
        }
        */

    public:

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
        template< typename T_COMPONENT >
        inline      T_COMPONENT&        getComponentByIndex     ( const index Index, int iComponent )                               noexcept;
        xforceinline auto&              getDescriptors          ( void ) const                                                      noexcept {return m_Descriptors; }

    protected:

        inline      void                DeletePendingEntries    ( void )                                                            noexcept;

    protected:

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

    protected:

        std::atomic<counter>                                    m_NewCount              {{0}};
        index                                                   m_Count                 {0};
        ptr_array                                               m_Pointers              {};
        std::span<const mecs::component::descriptor* const>     m_Descriptors           {};
        index                                                   m_MaxEntries            {0};
        mecs::tools::qvstp<std::uint32_t>                       m_DeleteList            {};
        mecs::archetype::instance*                              m_pArchetype            {nullptr};
    };
}