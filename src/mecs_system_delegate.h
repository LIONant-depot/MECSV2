namespace mecs::system::delegate
{
    using type_guid = xcore::guid::unit<64, struct mecs_system_delegate_type_guid_tag>;

    //---------------------------------------------------------------------------------
    // DELEGATE:: BASE CLASS
    //---------------------------------------------------------------------------------
    struct base
    {
        using guid = xcore::guid::unit<64, struct mecs_delegate_guid_tag>;

        virtual                        ~base                    (void)                          noexcept = default;
        virtual                 void    Disable                 (void)                          noexcept = 0;
        virtual                 void    Enable                  (void)                          noexcept = 0;
        virtual                 void    AttachToEvent           (mecs::world::instance& World)  noexcept = 0;
    };

    //---------------------------------------------------------------------------------
    // DELEGATE:: INSTANCE OVERRIDE
    // This classes specify what the end user can override to extend the functionality
    // of the delegate.
    //---------------------------------------------------------------------------------
    struct custom_instance_override : base
    {
        xforceinline            void    msgGraphInit            (mecs::world::instance& World)  noexcept {}
        xforceinline            void    msgFrameStart           (void)                          noexcept {}
        xforceinline            void    msgFrameDone            (void)                          noexcept {}
        // Can also override:   void    msgHandleEvents         ( Event Arguments )             noexcept {}

        constexpr static auto   type_guid_v = type_guid{ nullptr };
        constexpr static auto   name_v      = xconst_universal_str("mecs::event::delegate(custom_instance_base)");
    };

    //---------------------------------------------------------------------------------
    // DELEGATE:: DESCRIPTOR
    //---------------------------------------------------------------------------------
    struct descriptor
    {
        using fn_create = base*( void ) noexcept;
        const type_guid                         m_Guid;
        const xcore::string::const_universal    m_Name;
        fn_create                               m_fnCreate;
    };

    namespace details
    {
        //---------------------------------------------------------------------------------
        // DELEGATE:: DETAILS:: CUSTOM INSTANCE BASE
        // 
        //---------------------------------------------------------------------------------
        template< typename T_GRAPH, typename T_USER_DELEGATE, typename...T_ARGS >
        struct custom_instance;

        template< typename T_GRAPH, typename T_USER_DELEGATE, typename...T_ARGS >
        struct custom_instance< T_GRAPH, T_USER_DELEGATE, std::tuple<T_ARGS...> > final : T_USER_DELEGATE
        {
            using   self_t              = custom_instance;
            using   graph_instance_t    = T_GRAPH;
            using   user_delegate_t     = T_USER_DELEGATE;
            using   type_t              = mecs::system::event::instance;

            virtual void    Disable (void) noexcept override 
            {
            }

            virtual void    Enable  (void) noexcept override
            {
            }

            xforceinline            void    msgHandleEvents  (T_ARGS...) noexcept {}

            void HandleMessage( T_ARGS... Args ) noexcept
            {
                if constexpr (&user_delegate_t::msgHandleSystemEvents != &user_delegate_t::base_t::msgHandleSystemEvents )
                {
                    user_delegate_t::msgHandleSystemEvents( std::forward<T_ARGS>(Args)... );
                }
                else
                {
                    static_assert( std::is_same_v< system::instance&, std::tuple_element_t< 0u, xcore::function::traits<self_t>::args_tuple >>,
                                   "Your delegate function should have <<<system::instance&>>> as your first parameter" );

                    (*this)( std::forward<T_ARGS>(Args) ... );
                }
            }

            virtual void AttachToEvent( mecs::graph::instance& Graph ) noexcept
            {
                auto& Tuple = static_cast<T_GRAPH&>(Graph).m_SystemEventDB.m_mapEvent.get( user_delegate_t::event_t::guid_v );

                xcore::types::tuple_visit( 
                  [&](std::vector<mecs::system::event::instance_data_base::system_with_event>& Vec )
                {
                    for( const auto& [G, pVoid] : Vec )
                    {
                        auto& Event = *reinterpret_cast<mecs::tools::event<T_ARGS...>*>(pVoid);
                        Event.AddDelegate<&self_t::HandleMessage>(this);
                    }
                }
                , [&]( mecs::tools::event<>& PlaceHolderEvent )
                {
                    auto& Event = *reinterpret_cast<mecs::tools::event<T_ARGS...>*>(&PlaceHolderEvent);
                    Event.AddDelegate<&self_t::HandleMessage>(this);
                }
                , Tuple );
            }

            typename graph_instance_t::events::graph_init::delegate     m_GraphInitDelegate     { &self_t::msgGraphInit  };
            typename graph_instance_t::events::frame_start::delegate    m_FrameStartDelegate    { &self_t::msgFrameStart };
            typename graph_instance_t::events::frame_done::delegate     m_FrameDoneDelegate     { &self_t::msgFrameDone  };
        };

        //---------------------------------------------------------------------------------

        template< typename T_USER_DELEGATE, typename T_WORLD = mecs::world::instance >
        using make_instance = custom_instance
        <
            T_WORLD
        ,   T_USER_DELEGATE
        ,   typename T_USER_DELEGATE::event_t::event_t::arguments_tuple_t
        >;

        //---------------------------------------------------------------------------------

        template< typename T_DELEGATE >
        constexpr auto MakeDescriptor(void) noexcept
        {
            return mecs::system::event::descriptor
            {
                T_DELEGATE::type_guid_v.isValid() ? T_DELEGATE::type_guid_v : type_guid{ __FUNCSIG__ }
            ,   T_DELEGATE::name_v
            ,   []( void ) noexcept { new make_instance<T_DELEGATE>; }
            };
        }
    }

    template< typename T_DELEGATE >
    static constexpr auto descriptor_v = details::MakeDescriptor<xcore::types::decay_full_t<T_DELEGATE>>();

    //---------------------------------------------------------------------------------
    // DELEGATE:: DESCRIPTOR DATA BASE
    //---------------------------------------------------------------------------------

    struct descriptors_data_base
    {
        using map_delegate_type_t = mecs::tools::fixed_map<mecs::system::delegate::type_guid, descriptor*, mecs::settings::max_event_types >;

        void Init(void) {}

        template< typename T_DELEGATE >
        void Register( void ) noexcept
        {
            m_mapDescriptors.alloc( T_DELEGATE::guid_v, [](descriptor*& pDescriptor )
            {
                pDescriptor = &mecs::system::delegate::descriptor_v<T_DELEGATE>;
            });
        }

        template< typename...T_DELEGATE >
        void TupleRegister( std::tuple<T_DELEGATE...>* ) noexcept
        {
            ( Register<T_DELEGATE>(), ... );
        }

        map_delegate_type_t     m_mapDescriptors;
    };

}
