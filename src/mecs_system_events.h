namespace mecs::system::event
{
    //---------------------------------------------------------------------------------
    // There are 3 kinds of events
    //---------------------------------------------------------------------------------
    //
    // User Defined System Events
    //      These are sent only by the system specified in system_t
    // User Defined Global Events
    //      These are sent by system. system_t there for should be left to void;
    // archetype events
    //      These events are sent by the archetype and they are special because unlike the other two
    //      the delegates which register to these events types are filter by the query_t
    //
    //---------------------------------------------------------------------------------
    using type_guid = xcore::guid::unit<64, struct mecs_event_tag>;

    //---------------------------------------------------------------------------------
    // EVENT:: TYPE
    //---------------------------------------------------------------------------------

    //---------------------------------------------------------------------------------
    // EVENT:: DESCRIPTORS
    //---------------------------------------------------------------------------------
    struct descriptor
    {
        using fn_emplace = void(mecs::tools::event<>&);
        type_guid                               m_Guid;
        const xcore::string::const_universal    m_Name;
        fn_emplace                              m_fnEmplace;
    };

    namespace details
    {
        template< typename T_EVENT >
        constexpr auto MakeDescriptor(void) noexcept
        {
            return mecs::system::event::descriptor
            {
                T_EVENT::type_guid_v
            ,   T_EVENT::name_v
            ,   [](mecs::tools::event<>& Type ) noexcept { new(&Type) typename T_EVENT::event_t; }
            };
        }
    }

    template< typename T_EVENT >
    static constexpr auto descriptor_v = details::MakeDescriptor<xcore::types::decay_full_t<T_EVENT>>();

    //---------------------------------------------------------------------------------
    // EVENT:: 
    //---------------------------------------------------------------------------------
    namespace details
    {
        /*
        template< typename T_EVENT >
        constexpr bool EventContract() noexcept
        {
            static_assert(xcore::types::is_specialized_v<type, typename T_EVENT::event_t >
                , "Please use xcore::event::type<...> to define your event");

            static_assert(std::is_same_v<decltype(T_EVENT::guid_v), const event::guid >
                , "Make sure your event has a defined a guid_v of the right type (mecs::event::guid)");

            static_assert(std::is_same_v<decltype(T_EVENT::name_v), const xcore::string::const_universal >
                , "Make sure your event has a defined a name_v of the right type (xcore::string::const_universal)");

            static_assert(T_EVENT::guid_v.isValid()
                , "Make sure your event has a valid guid_v. Did you know you can initialize them with a string?");

            return true;
        };

        */

        /*
        namespace detail
        {
            template< std::size_t T_SIZE, typename...T_COMPONENTS >
            struct choose
            {
                static_assert(T_SIZE > 0);
                constexpr static auto       components_v = std::array{ mecs::component::descriptor_v<T_COMPONENTS>.m_Guid ... };
            };

            template<>
            struct choose<1, void>
            {
                constexpr static auto       components_v = std::span<mecs::component::type_guid>{ nullptr, std::size_t{ 0u } };
            };

            template<>
            struct choose<0>
            {
                constexpr static auto       components_v = std::span<mecs::component::type_guid>{ nullptr, std::size_t{ 0u } };
            };
        }

        template< typename...T_COMPONENTS >
        static constexpr auto choose_v = detail::choose<sizeof...(T_COMPONENTS), T_COMPONENTS...>::components_v;
        */
    }

    struct instance
    {
        constexpr static auto       type_guid_v = mecs::component::type_guid{ "mecs::event::system_event" };
    };


    struct descriptors_data_base
    {
        using map_event_type_t = mecs::tools::fixed_map<mecs::system::event::type_guid, descriptor*, mecs::settings::max_event_types >;

        void Init(void){}

        template< typename T_EVENT >
        void Register( void ) noexcept
        {
            m_mapDescriptors.alloc(T_EVENT::guid_v, [](descriptor*& pDescriptor )
            {
                pDescriptor = &mecs::system::event::descriptor_v<T_EVENT>;
            });
        }

        template< typename...T_EVENT >
        void TupleRegister( std::tuple<T_EVENT...>* ) noexcept
        {
            ( Register<T_EVENT>(), ... );
        }

        map_event_type_t    m_mapDescriptors;
    };

    struct instance_data_base
    {
        using system_instance_guid  = std::uint64_t;
        using system_with_event     = std::tuple< system_instance_guid, void* >;
        using type                  = std::variant< int, std::vector<system_with_event>, mecs::tools::event<> >;
        using map_event_t           = mecs::tools::fixed_map< mecs::system::event::type_guid, type, mecs::settings::max_event_types >;

        void AddSystemEvent(system_instance_guid SystemGuid, mecs::system::event::type_guid EventTypeGuid, void* pEvent ) noexcept
        {
            m_mapEvent.getOrCreate( EventTypeGuid
            , [&](map_event_t::value& Type) noexcept
            {
                auto& Vec = Type.emplace<std::vector<system_with_event>>();
                Vec.push_back( system_with_event{ SystemGuid, pEvent } );
            }
            , [&](map_event_t::value& Type ) noexcept
            {
                auto& Vec = std::get<std::vector<system_with_event>>(Type);
                Vec.push_back(system_with_event{ SystemGuid, pEvent });
            });
        }

        void AddGlobalEvent( mecs::system::event::type_guid EventTypeGuid )
        {
            m_mapEvent.getOrCreate(EventTypeGuid
            , [&](map_event_t::value& Type) noexcept
            {
                auto& Event = Type.emplace<mecs::tools::event<>>();
                m_pDescriptorDataBase->m_mapDescriptors.get(EventTypeGuid)->m_fnEmplace(Event);
            }
            , [&](map_event_t::value& Type) noexcept
            {
                auto& Event = std::get<mecs::tools::event<>>(Type);
                m_pDescriptorDataBase->m_mapDescriptors.get(EventTypeGuid)->m_fnEmplace(Event);
            });
        }

        template< typename T_SYSTEM_EVENT >
        typename T_SYSTEM_EVENT::event_t* find( std::uint64_t SystemGuid, mecs::system::event::type_guid EventTypeGuid )
        {
            auto& Tuple = m_mapEvent.get( EventTypeGuid );
            auto& Vec   = std::get<std::vector<system_with_event>>(Tuple);

            for( const auto& [G, pVoid ] : Vec )
            {
                if( G.m_Value == SystemGuid ) return pVoid;
            }

            return nullptr;
        }

        map_event_t             m_mapEvent;
        descriptors_data_base*  m_pDescriptorDataBase;
    };
}
