namespace mecs::sync_point
{
    using type_guid = xcore::guid::unit<64, struct type_tag>;

    struct overrites
    {
        constexpr static auto       type_guid_v = type_guid{ nullptr };
        constexpr static auto       name_v      = xconst_universal_str("unnamed sync-point type");
    };

    namespace details
    {
        struct events
        {
            using done  = xcore::types::make_unique< mecs::tools::event<mecs::sync_point::instance&>, struct done_tag >;
            using start = xcore::types::make_unique< mecs::tools::event<mecs::sync_point::instance&>, struct start_tag >;

            start   m_Start   {};
            done    m_Done    {};
        };
    }

    struct instance : xcore::scheduler::trigger<mecs::settings::max_graph_connections>, overrites
    {
        using                       parent_t        = xcore::scheduler::trigger<mecs::settings::max_graph_connections>;
        using                       guid            = xcore::guid::unit<64, struct sync_point_tag >;
        using                       type_guid       = mecs::sync_point::type_guid;
        constexpr static auto       type_guid_v     = type_guid{"sync point instance"};
        constexpr static auto       name_v          = xconst_universal_str("sync point instance");

        bool                        m_bDisable      {false};
        std::uint32_t               m_Tick          {0};
        guid                        m_Guid          {};
        details::events             m_Events        {};
        xcore::string::ref<char>    m_Name          {};

        xforceinline instance( void ) noexcept : parent_t{ xcore::scheduler::lifetime::DONT_DELETE_WHEN_DONE, xcore::scheduler::triggers::DONT_CLEAN_COUNT } {}

        virtual void qt_onTriggered (void) noexcept override
        {
            m_Events.m_Done.NotifyAll(*this);
            m_Tick++;
            if( m_bDisable == false )
            {
                m_Events.m_Start.NotifyAll(*this);
                parent_t::qt_onTriggered();
            }
        }
    };


    struct descriptor
    {
        using fn_create = std::unique_ptr<mecs::sync_point::instance>(void) noexcept;

        const type_guid                         m_Guid;
        const xcore::string::const_universal    m_Name;
        fn_create*                              m_fnCreate;
    };

    namespace details
    {
        template< typename T_SYNC_POINT >
        constexpr auto MakeDescriptor( void ) noexcept
        {
            static_assert( T_SYNC_POINT::type_guid_v.isValid() );
            static_assert(std::is_same_v<decltype(T_SYNC_POINT::type_guid_v), const mecs::sync_point::type_guid>);

            return mecs::sync_point::descriptor
            {
                T_SYNC_POINT::type_guid_v
            ,   T_SYNC_POINT::name_v
            ,   []() noexcept -> std::unique_ptr<mecs::sync_point::instance>
                {
                    auto p = static_cast<mecs::sync_point::instance*>(new T_SYNC_POINT);
                    return std::unique_ptr<mecs::sync_point::instance>{ p };
                }
            };
        }
    }

    template< typename T_SYNC_POINT >
    static constexpr auto descriptor_v = details::MakeDescriptor<xcore::types::decay_full_t<T_SYNC_POINT>>();

    struct descriptors_data_base
    {
        using map_sync_point_descriptor  = mecs::tools::fixed_map<mecs::sync_point::type_guid,       const descriptor*,                              mecs::settings::max_sync_points>;

        void Init(void)
        {
            Register<instance>();
        }

        template< typename...T_SYNC_POINT >
        void Register( void ) noexcept
        {
            static_assert(!!sizeof...(T_SYNC_POINT));
            if((m_mapDescriptors.alloc(descriptor_v<T_SYNC_POINT>.m_Guid, [&](const descriptor*& Ptr)
            {
                m_lDescriptors.push_back( Ptr = &descriptor_v<T_SYNC_POINT> );
            } ) || ... ))
            {
                // Make sure that someone did not forget the set the right guid
                xassert( ((m_mapDescriptors.get(descriptor_v<T_SYNC_POINT>.m_Guid) == &descriptor_v<T_SYNC_POINT>) && ...) );
            }
        }

        std::vector<const descriptor*>      m_lDescriptors;
        map_sync_point_descriptor           m_mapDescriptors;
    };

    struct instance_data_base
    {
        using map_sync_point        = mecs::tools::fixed_map<mecs::sync_point::instance::guid,  std::unique_ptr<mecs::sync_point::instance>,    mecs::settings::max_sync_points>;

        void Init(void){}

        template< typename T_SYNC_POINT >
        instance& Create( instance::guid Guid )
        {
            instance* p = nullptr;
            m_mapInstances.alloc(Guid, [&](std::unique_ptr<mecs::sync_point::instance>& I)
            {
                m_lInstance.push_back(p = I = std::make_unique<T_SYNC_POINT>());
                I.m_Guid = Guid;
            });
            xassert(p);
            return *p;
        }

        std::vector<instance*>              m_lInstance;
        map_sync_point                      m_mapInstances;
    };
}