namespace mecs::system::delegate
{
    using type_guid = xcore::guid::unit<64, struct mecs_system_delegate_type_guid_tag>;

    //---------------------------------------------------------------------------------
    // DELEGATE:: INSTANCE OVERRIDE
    // This classes specify what the end user can override to extend the functionality
    // of the delegate.
    //---------------------------------------------------------------------------------
    struct overrides
    {
        using                           type_guid               = mecs::system::delegate::type_guid;
        using                           entity                  = mecs::component::entity;
        using                           guid                    = xcore::guid::unit<64, struct mecs_delegate_guid_tag>;
        using                           system                  = mecs::system::instance;

        constexpr static auto           type_guid_v             = type_guid{ nullptr };
        constexpr static auto           type_name_v             = xconst_universal_str("mecs::event::delegate(custom_instance_base)");

        virtual                        ~overrides               (void)                          noexcept = default;
        virtual                 void    Disable                 (void)                          noexcept = 0;
        virtual                 void    Enable                  (void)                          noexcept = 0;

        xforceinline            void    msgGraphInit            (mecs::world::instance& World)  noexcept {}
        xforceinline            void    msgFrameStart           (void)                          noexcept {}
        xforceinline            void    msgFrameDone            (void)                          noexcept {}
        // Can also override:   void    msgHandleEvents         ( Event Arguments )             noexcept {}

        guid                            m_Guid                  { nullptr };
    };


    template< typename T_EVENT >
    struct instance : overrides
    {
        using event_t = T_EVENT;
    };

    //---------------------------------------------------------------------------------
    // DELEGATE:: DESCRIPTOR
    //---------------------------------------------------------------------------------
    struct descriptor
    {
        using fn_create = overrides&(mecs::world::instance&, overrides::guid) noexcept;
        const type_guid                         m_Guid;
        const xcore::string::const_universal    m_Name;
        fn_create*                              m_fnCreate;
    };

    namespace details
    {
        //---------------------------------------------------------------------------------
        // DELEGATE:: DETAILS:: CUSTOM INSTANCE BASE
        // 
        //---------------------------------------------------------------------------------
        template< typename T_USER_DELEGATE, typename...T_ARGS >
        struct custom_instance;

        template< typename T_USER_DELEGATE, typename...T_ARGS >
        struct custom_instance< T_USER_DELEGATE, std::tuple<T_ARGS...> > final : T_USER_DELEGATE
        {
            using                           self_t                  = custom_instance;
            using                           user_delegate_t         = T_USER_DELEGATE;
            using                           type_t                  = mecs::system::event::instance;
            using                           guid                    = typename user_delegate_t::guid;
            static constexpr auto           is_exclusive_v          = std::is_base_of_v<mecs::system::event::exclusive, user_delegate_t::event_t>;

            static_assert( std::is_same_v< user_delegate_t::event_t::system_t, void>               == std::is_base_of_v<mecs::system::event::global, user_delegate_t::event_t> 
                         , "Global events should not override system_t." );
            static_assert( std::is_base_of_v<mecs::system::event::exclusive, user_delegate_t::event_t> || std::is_base_of_v<mecs::system::event::global, user_delegate_t::event_t> 
                         , "You are using an event which is not mecs::system::event::global or mecs::system::event::local derived.");

            mecs::world::instance& m_World;

            constexpr           custom_instance         ( mecs::world::instance& World, guid Guid )    noexcept;
            virtual     void    Disable                 ( void )                                       noexcept override { xassert(false); }
            virtual     void    Enable                  ( void )                                       noexcept override { xassert(false); }
                        void    msgHandleSystemEvents   ( system::instance& System, T_ARGS... Args )   noexcept;

//            inline      void    eventAddNewSystem       ( system::instance& System )                   noexcept;
//            inline      void    eventDeleteSystem       ( system::instance& System )                   noexcept;
        };

        //---------------------------------------------------------------------------------

        template< typename T_SYSTEM_DELEGATE >
        constexpr auto MakeDescriptor(void) noexcept
        {
            using custom_t = custom_instance
            <
                T_SYSTEM_DELEGATE
                , xcore::types::tuple_delete_n_t< 1u, typename T_SYSTEM_DELEGATE::event_t::real_event_t::arguments_tuple_t >
            >;

            return mecs::system::delegate::descriptor
            {
                T_SYSTEM_DELEGATE::type_guid_v.isValid() ? T_SYSTEM_DELEGATE::type_guid_v : type_guid{ __FUNCSIG__ }
            ,   T_SYSTEM_DELEGATE::type_name_v
            ,   [](mecs::world::instance& World, overrides::guid Guid ) constexpr noexcept -> overrides& 
                { return *new custom_t{ World, Guid }; }
            };
        }
    }

    template< typename T_DELEGATE >
    inline static constexpr auto descriptor_v = details::MakeDescriptor<xcore::types::decay_full_t<T_DELEGATE>>();

    //---------------------------------------------------------------------------------
    // DELEGATE:: DESCRIPTOR DATA BASE
    //---------------------------------------------------------------------------------
    struct descriptors_data_base
    {
        using map_delegate_type_t = mecs::tools::fixed_map<type_guid, const descriptor*, mecs::settings::max_event_types >;

        void Init(void) {}

        template< typename T_DELEGATE >
        void Register(void) noexcept
        {
            m_mapDescriptors.alloc(descriptor_v<T_DELEGATE>.m_Guid, [](const descriptor*& pDescriptor)
            {
                pDescriptor = &descriptor_v<T_DELEGATE>;
            });
        }

        template< typename...T_DELEGATE >
        void TupleRegister(std::tuple<T_DELEGATE...>*) noexcept
        {
            (Register<T_DELEGATE>(), ...);
        }

        overrides& Create( mecs::world::instance& World, type_guid gTypeGuid, overrides::guid InstanceGuid ) noexcept
        {
            return m_mapDescriptors.get(gTypeGuid)->m_fnCreate(World, InstanceGuid);
        }

        map_delegate_type_t     m_mapDescriptors;
    };

    struct instance_data_base
    {
        using map_delegate_instance_t = mecs::tools::fixed_map<overrides::guid, std::unique_ptr<overrides>, mecs::settings::max_sytem_delegates >;

        template< typename T_ARCHETYPE_DELEGATE >
        T_ARCHETYPE_DELEGATE& Create(overrides::guid Guid) noexcept;

        constexpr   instance_data_base          ( mecs::world::instance& World ) : m_World{ World } {}
        overrides&  Create                      ( type_guid gTypeGuid, overrides::guid InstanceGuid ) noexcept;

        mecs::world::instance&  m_World;
        map_delegate_instance_t m_mapInstance{};
    };
}
