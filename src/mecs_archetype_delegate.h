namespace mecs::archetype::delegate
{
    using type_guid = xcore::guid::unit<64, struct mecs_archetype_delegate_type_guid_tag>;

    //---------------------------------------------------------------------------------
    // DELEGATE:: BASE CLASS
    //---------------------------------------------------------------------------------
    struct base
    {
        using guid = xcore::guid::unit<64, struct mecs_delegate_guid_tag>;

        virtual                        ~base                    (void) noexcept = default;
        virtual                 void    Disable                 (void) noexcept = 0;
        virtual                 void    Enable                  (void) noexcept = 0;
        virtual                 void    AttachToArchetype       (mecs::world::instance& World, mecs::archetype::instance& Archetype)   noexcept = 0;
    };

    //---------------------------------------------------------------------------------
    // DELEGATE:: INSTANCE OVERRIDE
    // This classes specify what the end user can override to extend the functionality
    // of the delegate.
    //---------------------------------------------------------------------------------
    struct archetype_instance_override : base
    {
        virtual                 void    InitializeQuery         (mecs::world::instance& World)  noexcept {}
        xforceinline            void    msgGraphInit            (mecs::world::instance& World)  noexcept {}
        xforceinline            void    msgFrameStart           (void)                          noexcept {}
        xforceinline            void    msgFrameDone            (void)                          noexcept {}
        // Can also override:   void    msgHandleEvents         ( Event Arguments )             noexcept {}

        constexpr static auto   type_guid_v         = mecs::archetype::delegate::type_guid{ nullptr };
        constexpr static auto   name_v              = xconst_universal_str("mecs::event::delegate(archetype_instance_base)");
        constexpr static auto   max_attachments_v   = 32;
        using                   query_t             = std::tuple<>;
    };

    //---------------------------------------------------------------------------------
    // DELEGATE:: DESCRIPTOR
    //---------------------------------------------------------------------------------
    struct descriptor
    {
        using fn_create = base*( void ) noexcept;
        type_guid                               m_Guid;
        const xcore::string::const_universal    m_Name;
        fn_create                               m_fnCreate;
    };

    namespace details
    {
        //---------------------------------------------------------------------------------
        // DELEGATE:: DETAILS:: DELEGATE CONTRACT
        //---------------------------------------------------------------------------------
        template< typename T_DELEGATE >
        constexpr bool DelegateContract() noexcept
        {
            /*
          //  static_assert( std::is_based_of_v< base, T_DELEGATE >
          //                 , "Make sure your delegate is derived from mecs::delegate::instance" );

            static_assert(mecs::event::details::template EventContract<T_DELEGATE::event_t>());

            static_assert(std::is_same_v<decltype(T_DELEGATE::guid_v),const delegate::guid >
                            ,"Make sure your delegate has defined a guid_v of the right type (mecs::delegate::guid)");

            static_assert(std::is_same_v<decltype(T_DELEGATE::name_v),const xcore::string::const_universal >
                            ,"Make sure your delegate has a defined a name_v of the right type (xcore::string::const_universal)");

            static_assert(T_DELEGATE::guid_v.isValid()
                            ,"Make sure your delegate has a valid guid_v. Did you know you can initialize them with a string?");
                            */
            return true;
        };

        //---------------------------------------------------------------------------------
        // DELEGATE:: DETAILS:: CALL FUNCTION
        //---------------------------------------------------------------------------------
        template< typename T_CUSTOM_INSTANCE, typename...T_FINAL_ARGS, typename...T_ADDITIONAL_ARGS >
        constexpr xforceinline static void CallFunction
        ( 
                T_CUSTOM_INSTANCE&              Instance
            ,   std::tuple<T_FINAL_ARGS...>* 
            ,   mecs::component::entity&        Entity
            ,   system::instance&               Sys
            ,   T_ADDITIONAL_ARGS&&...          ExtraArgs
        ) noexcept
        {
            if constexpr( (sizeof...(T_FINAL_ARGS) - sizeof...(ExtraArgs)) != 0 )
            {
                auto& MapEntry = Entity.getMapEntry();
                Instance
                ( 
                        Sys
                    ,   std::forward(ExtraArgs)...
                    ,   MapEntry->m_pPool->get<T_FINAL_ARGS>(MapEntry->m_Index) ... //TmpRef.getComponent<xcore::types::decay_full_t<T_FINAL_ARGS>>() ... 
                );
            }
            else
            {
                Instance( Sys, std::forward(ExtraArgs)... );
            }
        }

        //---------------------------------------------------------------------------------
        template< typename T >
        struct function_helper;

        template< typename...T_ARGS >
        struct function_helper< std::tuple<T_ARGS...> >
        {
            void operator() ( T_ARGS... ) {};
        };

        //---------------------------------------------------------------------------------
        // DELEGATE:: DETAILS:: MAKE CUSTOM INSTANCE
        //---------------------------------------------------------------------------------
        template< typename T_GRAPH, typename T_USER_DELEGATE, typename...T_ARGS >
        struct custom_instance;

        template< typename T_GRAPH, typename T_USER_DELEGATE, typename...T_ARGS >
        struct custom_instance< T_GRAPH, T_USER_DELEGATE, std::tuple<T_ARGS...> > final : T_USER_DELEGATE
        {
            using                   self_t              = custom_instance;
            using                   graph_instance_t    = T_GRAPH;
            using                   user_delegate_t     = T_USER_DELEGATE;
            using                   type_t              = mecs::archetype::event::details::base_event;
            using                   event_t             = typename user_delegate_t::event_t;
            constexpr static auto   query_v             = mecs::archetype::query::details::define<typename user_delegate_t::query_t>{};

            mecs::archetype::query::instance         m_GroupQuery;

            // Setting some defaults
            xforceinline            void    msgHandleEvents  (T_ARGS...) noexcept {}

            static_assert( mecs::archetype::event::details::template DelegateContract<T_USER_DELEGATE>() );

            virtual void    Disable (void) noexcept override 
            {
                // TODO: Missing implementation
                xassert(false);
            }

            virtual void    Enable  (void) noexcept override
            {
                // TODO: Missing implementation
                xassert(false);
            }

            virtual void InitializeQuery( T_GRAPH& Graph ) noexcept override
            {
                /*
                if constexpr( &user_delegate_t::InitializeQuery != &user_delegate_t::base_t::InitializeQuery)
                {
                    user_delegate_t::InitializeQuery( World );
                }
                else
                {
                    using args = xcore::types::tuple_delete_n_t< 1u, xcore::function::traits<self_t>::args_tuple >;
                    if constexpr ( std::is_same_v<args,std::tuple<>> )
                    {
                        World.InitializeQuery
                        < 
                            mecs::query::system_function<function_helper<std::tuple<const mecs::component::entity&>>>
                        >( user_delegate_t::m_GroupQuery, query_v );
                    }
                    else
                    {
                        World.InitializeQuery
                        < 
                            mecs::query::system_function<function_helper<args>>
                        >( user_delegate_t::m_GroupQuery, query_v );
                    }
                }
                */
            }

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
                    CallFunction
                    ( 
                            *this
                        ,   reinterpret_cast< xcore::types::tuple_delete_n_t< 1u, xcore::function::traits<self_t>::args_tuple >* >(nullptr) 
                        ,   Args...
                    );
                }
            }

            virtual void AttachToAchetype( mecs::graph::instance& Graph, mecs::archetype::instance& Archetype ) noexcept override
            {
                //
                // Add archetype to our list
                //
                {
                    xcore::lock::scope Lk(user_delegate_t::m_lArchetype);
                    user_delegate_t::m_lArchetype.get().push_back(Archetype.m_Guid);
                }

                //
                // Handle the different types of events
                //
                if      constexpr ( user_delegate_t::event_t::guid_v == mecs::archetype::event::moved_in<void>::guid_v )
                {
                    Archetype.m_Events.m_MovedInEntity.AddDelegate<&self_t::HandleMessage>( this );
                }
                else if constexpr( user_delegate_t::event_t::guid_v == mecs::archetype::event::create_entity::guid_v )
                {
                    Archetype.m_Events.m_CreatedEntity.AddDelegate<&self_t::HandleMessage>( this );
                }
                else if constexpr( user_delegate_t::event_t::guid_v == mecs::archetype::event::destroy_entity::guid_v )
                {
                    Archetype.m_Events.m_DeletedEntity.AddDelegate<&self_t::HandleMessage>( this );
                }
                else if constexpr( user_delegate_t::event_t::guid_v == mecs::archetype::event::moved_out<void>::guid_v )
                {
                    Archetype.m_Events.m_MovedOutEntity.AddDelegate<&self_t::HandleMessage>( this );
                }
                else if constexpr( user_delegate_t::event_t::guid_v == mecs::archetype::event::updated_component<void>::guid_v )
                {
                    Archetype.m_Events.m_UpdateComponent.m_Event.AddDelegate<&self_t::HandleMessage>( this );

                    static const tools::bits Bits{[&]
                    {
                        if( event_t::components_v.size() )
                        {
                            tools::bits Bits{nullptr};
                            for( auto k : event_t::components_v )
                                Bits.AddBit( static_cast<T_GRAPH&>(Graph).m_pUniverse->m_ComponentDescriptorsDB.m_mapDescriptors.get( k )->m_BitNumber );
                            return Bits;
                        }

                        return tools::bits{xcore::not_null};
                    }()};
                    
                    Archetype.m_Events.m_UpdateComponent.m_Bits.push_back(Bits);
                }
                else
                {
                    static_assert( xcore::types::always_false_v, "We are not covering all the cases!!" );
                }
            }

            typename graph_instance_t::events::graph_init::delegate     m_GraphInitDelegate     {       &self_t::msgGraphInit     };
            typename graph_instance_t::events::frame_start::delegate    m_FrameStartDelegate    {       &self_t::msgFrameStart    };
            typename graph_instance_t::events::frame_done::delegate     m_FrameDoneDelegate     {       &self_t::msgFrameDone     };
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
                T_DELEGATE::type_guid_v
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