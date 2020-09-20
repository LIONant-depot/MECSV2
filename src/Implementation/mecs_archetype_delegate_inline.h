namespace mecs::archetype::delegate
{
    namespace details
    {
        template< typename T_TUPLE>
        struct function_helper;

        template< typename...T_ARGS >
        struct function_helper< std::tuple<T_ARGS...> >
        {
            void operator()(T_ARGS...);
        };

        template< typename T_USER_DELEGATE > constexpr xforceinline
        custom_instance< T_USER_DELEGATE >::custom_instance(mecs::world::instance& World) noexcept
            : m_World{ World }
        {
            //
            // Register to Graph messages
            //
            if constexpr( &custom_instance::msgGraphInit != &overrides::msgGraphInit )
            {
                m_World.m_GraphDB.m_Events.m_GraphInit.AddDelegate<&custom_instance::msgGraphInit>(*this);
            }

            if constexpr( &custom_instance::msgFrameStart != &overrides::msgFrameStart )
            {
                m_World.m_GraphDB.m_Events.m_FrameStart.AddDelegate<&custom_instance::msgFrameStart>(*this);
            }

            if constexpr( &custom_instance::msgFrameDone != &overrides::msgFrameDone )
            {
                m_World.m_GraphDB.m_Events.m_FrameDone.AddDelegate<&custom_instance::msgFrameDone>(*this);
            }

            //
            // Register to ArchetypeDB messages
            //
            m_World.m_ArchetypeDB.m_Event.m_CreatedArchetype.AddDelegate<&custom_instance::eventAddNewArchetype>(*this);
            m_World.m_ArchetypeDB.m_Event.m_DeletedArchetype.AddDelegate<&custom_instance::eventDeleteArchetype>(*this);

            //
            // Do query to collect all relevant archetypes
            //
            if constexpr( &custom_instance::msgInitializeQuery != &overrides::msgInitializeQuery )
            {
                custom_instance::msgInitializeQuery();
            }

            {
                using args = xcore::types::tuple_delete_n_t< 1u, xcore::function::traits<self_t>::args_tuple >;
                if constexpr ( std::is_same_v< args, std::tuple<> > )
                {
                    World.m_ArchetypeDB. template DoQuery
                    < 
                        function_helper<std::tuple<const overrides::entity&>>
                    ,   typename user_delegate_t::query_t
                    >( m_Query );
                }
                else
                {
                    World.m_ArchetypeDB. template DoQuery
                    < 
                        function_helper<args>
                    ,   typename user_delegate_t::query_t
                    >( m_Query );
                }
            }

            //
            // For the archetypes found add to relevant events
            //
            for( auto& R : m_Query.m_lResults )
            {
                AttachToArchetype( *R.m_pArchetype );
            }
        }

        template< typename T_USER_DELEGATE > inline
        void custom_instance< T_USER_DELEGATE >::eventAddNewArchetype(archetype::instance& Archetype) noexcept
        {
            if( m_Query.TryAppendArchetype( Archetype ) )
            {
                AttachToArchetype( Archetype );
            }
        }

        template< typename T_USER_DELEGATE > inline
        void custom_instance< T_USER_DELEGATE >::eventDeleteArchetype( archetype::instance& Archetype ) noexcept
        {
            for (auto& R : m_Query.m_lResults )
            {
                if( R.m_pArchetype == &Archetype )
                {
                    m_Query.m_lResults.eraseSwap(m_Query.m_lResults.getIndexByEntry(R));
                    break;
                }
            }
        }

        template< typename T_USER_DELEGATE > inline
        void custom_instance< T_USER_DELEGATE >::AttachToArchetype( archetype::instance& Archetype ) noexcept
        {
            //
            // Handle the different types of events
            //
            if      constexpr ( user_delegate_t::event_t::type_guid_v == mecs::archetype::event::moved_in::type_guid_v )
            {
                Archetype.m_Events.m_MovedInEntity.AddDelegate<&custom_instance::HandleEvents>(*this );
            }
            else if constexpr( user_delegate_t::event_t::type_guid_v == mecs::archetype::event::create_entity::type_guid_v )
            {
                Archetype.m_Events.m_CreatedEntity.AddDelegate<&custom_instance::HandleEvents>(*this );
            }
            else if constexpr (user_delegate_t::event_t::type_guid_v == mecs::archetype::event::create_pool::type_guid_v)
            {
                Archetype.m_Events.m_CreatedPool.AddDelegate<&custom_instance::HandlePoolEvents>(*this);
            }
            else if constexpr (user_delegate_t::event_t::type_guid_v == mecs::archetype::event::destroy_pool::type_guid_v)
            {
                Archetype.m_Events.m_DestroyPool.AddDelegate<&custom_instance::HandlePoolEvents>(*this);
            }
            else if constexpr( user_delegate_t::event_t::type_guid_v == mecs::archetype::event::destroy_entity::type_guid_v )
            {
                Archetype.m_Events.m_DestroyEntity.AddDelegate<&custom_instance::HandleEvents>(*this );
            }
            else if constexpr( user_delegate_t::event_t::type_guid_v == mecs::archetype::event::moved_out::type_guid_v )
            {
                Archetype.m_Events.m_MovedOutEntity.AddDelegate<&custom_instance::HandleEvents>(*this );
            }
            else if constexpr( user_delegate_t::event_t::type_guid_v == mecs::archetype::event::updated_component<>::type_guid_v )
            {
                static const tools::bits Bits{[&]()
                {
                    if constexpr ( event_t::components_v.size() )
                    {
                        tools::bits Bits{nullptr};
                        for( auto k : event_t::components_v )
                            Bits.AddBit( k->m_BitNumber );
                        return Bits;
                    }
                    else return tools::bits{xcore::not_null};
                }()};
                
                Archetype.m_Events.m_UpdateComponent.m_Event.AddDelegate<&custom_instance::HandleEvents>(*this);
                Archetype.m_Events.m_UpdateComponent.m_Bits.push_back(Bits);
            }
            else
            {
                static_assert( xcore::types::always_false_v, "We are not covering all the cases!!" );
            }
        }

        template< typename T_USER_DELEGATE > inline
        void custom_instance< T_USER_DELEGATE >::HandleEvents(mecs::component::entity& Entity, mecs::system::instance& System) noexcept
        {
            if constexpr (std::is_same_v< event_t, mecs::archetype::event::details::base_event::event_t> == false )
            {
                // Do nothing if we are not an regular event
                xassert(false);
            }
            else if constexpr ( &custom_instance::msgHandleEvents != &overrides::msgHandleEvents )
            {
                custom_instance::msgHandleEvents(Entity, System);
            }
            else
            {
                using params_tuple = typename xcore::function::traits<custom_instance>::args_tuple;

                if constexpr( std::tuple_size_v<params_tuple> == 1 )
                {
                    (*this)(System);
                }
                else
                {
                    using componets_tuple = xcore::types::tuple_delete_n_t< 1u, params_tuple >;

                    mecs::archetype::details::CallFunction
                    (           
                        Entity.m_pInstance->m_Value
                        , *this
                        , reinterpret_cast<componets_tuple*>(nullptr)
                        , System
                    );
                }
            }
        }

        template< typename T_USER_DELEGATE > inline
        void custom_instance< T_USER_DELEGATE >::HandlePoolEvents( mecs::system::instance& System, mecs::archetype::specialized_pool& Pool ) noexcept
        {
            if constexpr ( std::is_same_v< event_t, mecs::archetype::event::details::base_event::event_t> )
            {
                // Do nothing if we are not an event pool
                xassert(false);
            }
            else if constexpr ( &custom_instance::msgHandlePoolEvents != &overrides::msgHandlePoolEvents )
            {
                custom_instance::msgHandlePoolEvents(System, Pool);
            }
            else
            {
                using params_tuple = typename xcore::function::traits<custom_instance>::args_tuple;
                static_assert( std::tuple_size_v<params_tuple> >= 2 );
                if constexpr( std::tuple_size_v<params_tuple> == 2 )
                {
                    (*this)(System, Pool);
                }
                else
                {
                    using componets_tuple = xcore::types::tuple_delete_n_t< 2u, params_tuple >;

                    mecs::archetype::details::CallPoolFunction
                    (
                          Pool
                        , *this
                        , reinterpret_cast<componets_tuple*>(nullptr)
                        , System
                    );
                }
            }
        }

    }

    template< typename T_ARCHETYPE_DELEGATE >
    T_ARCHETYPE_DELEGATE& instance_data_base::Create(overrides::guid Guid) noexcept
    {
        m_World.m_Universe.registerTypes<T_ARCHETYPE_DELEGATE>();
        return static_cast<T_ARCHETYPE_DELEGATE&>(Create(descriptor_v<T_ARCHETYPE_DELEGATE>.m_Guid, Guid));
    }

    inline
    overrides& instance_data_base::Create( type_guid gTypeGuid, overrides::guid InstanceGuid ) noexcept
    {
        overrides* p;
        m_mapInstance.alloc(InstanceGuid, [&](map_delegate_instance_t::value& Entry )
        {
            auto& Delegate = m_World.m_Universe.m_ArchetypeDelegateDescriptorsDB.Create( m_World, gTypeGuid);

            Delegate.m_Guid = InstanceGuid;
            Entry = std::unique_ptr<overrides>{ p = &Delegate };
        });

        return *p;
    }
}
