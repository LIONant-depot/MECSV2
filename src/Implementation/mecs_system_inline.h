namespace mecs::system
{
    //---------------------------------------------------------------------------------
    // SYSTEM:: DETAILS::CUSTOM_SYSTEM
    //---------------------------------------------------------------------------------
    namespace details
    {
        struct default_system : mecs::system::instance
        {
            using instance::instance;

            void msgUpdate() noexcept
            {
            }
        };

        template< bool T_IS_GLOBAL_V, typename...T_ARGS >
        auto GetRealEventsTuple( std::tuple<T_ARGS...>* )
        {
            if constexpr (sizeof...(T_ARGS) == 0) return std::tuple<>{};
            else if constexpr (T_IS_GLOBAL_V)     return std::tuple< typename T_ARGS::real_event_t* ... >{};
            else                                  return std::tuple< typename T_ARGS::real_event_t  ... >{};
        }

        template< typename event_type, typename...T_ARGS >
        auto FilterEventsTuple( std::tuple<T_ARGS...>* ) ->
            xcore::types::tuple_cat_t< std::conditional_t< std::is_base_of_v< event_type, T_ARGS>, std::tuple<T_ARGS>, std::tuple<> > ... >;

        template< typename...T_ARGS > xforceinline constexpr
        auto GetEventDescriptorArray(std::tuple<T_ARGS...>*) noexcept
        {
            if constexpr (!!sizeof...(T_ARGS))  return std::array{ &mecs::system::event::descriptor_v<T_ARGS> ... };
            else                                return std::array< const mecs::system::event::descriptor*, 0>{};
        }

        //---------------------------------------------------------------------------------
        // SYSTEM::CUSTOM SYSTEM 
        //---------------------------------------------------------------------------------
        template< typename T_SYSTEM >
        struct custom_system final : T_SYSTEM
        {
            using                           user_system_t                   = T_SYSTEM;
            inline static constexpr auto    query_v                         = mecs::archetype::query::details::define<typename user_system_t::query_t>{};
            using                           global_event_tuple_t            = decltype(FilterEventsTuple<system::event::global>(reinterpret_cast<typename user_system_t::events_t*>(nullptr)));
            using                           global_real_events_t            = decltype(GetRealEventsTuple<true>(reinterpret_cast<global_event_tuple_t*>(nullptr)));
            inline static constexpr auto    global_events_descriptors_v     = GetEventDescriptorArray(reinterpret_cast<global_event_tuple_t*>(nullptr));
            using                           exclusive_event_tuple_t         = decltype(FilterEventsTuple<system::event::exclusive>(reinterpret_cast<typename user_system_t::events_t*>(nullptr)));
            using                           exclusive_real_events_t         = decltype(GetRealEventsTuple<false>(reinterpret_cast<exclusive_event_tuple_t*>(nullptr)));
            inline static constexpr auto    exclusive_events_descriptors_v  = GetEventDescriptorArray(reinterpret_cast<exclusive_event_tuple_t*>(nullptr));
            
            exclusive_real_events_t     m_ExclusiveEvents   {};
            global_real_events_t        m_GlobalEvents      {};

            constexpr       custom_system               ( const typename T_SYSTEM::construct&& Settings )                   noexcept;
            virtual void    qt_onRun                    ( void )                                                            noexcept override;
            inline  void    msgSyncPointDone            ( mecs::sync_point::instance& Syncpoint )                           noexcept;
            virtual void*   DetailsGetExclusiveRealEvent( const system::event::type_guid )                                  noexcept;
            virtual const descriptor& getDescriptor     ( void ) const noexcept
            {
                return descriptor_v<T_SYSTEM>;
            }
        };

        template< typename T_CUSTOM_SYSTEM, typename...T_ARGS> constexpr xforceinline
        auto GetGlobalEventTuple( T_CUSTOM_SYSTEM& System, std::tuple<T_ARGS...>* ) noexcept
        {
            return typename T_CUSTOM_SYSTEM::global_real_events_t
            {
                &System.m_World.m_GraphDB.m_SystemGlobalEventDB.getOrCreate< xcore::types::decay_full_t<T_ARGS> >
                    (mecs::system::event::descriptor_v<xcore::types::decay_full_t<T_ARGS>>.m_Guid)
                ...
            };
        }

        template< typename T_SYSTEM > inline constexpr
        custom_system<T_SYSTEM>::custom_system( const typename T_SYSTEM::construct&& Settings ) noexcept
        : T_SYSTEM      { std::move(Settings) }
        , m_GlobalEvents{ GetGlobalEventTuple( *this, reinterpret_cast<typename custom_system::global_event_tuple_t*>(nullptr) ) }
        {
            instance::m_TypeGuid = descriptor_v<T_SYSTEM>.m_Guid;

            //
            // Register with World events
            //
            if constexpr( &custom_system::msgGraphInit != &overrides::msgGraphInit )
            {
                user_system_t::m_World.m_Graph.m_Events.m_GraphInit.AddDelegate<&custom_system::msgGraphInit>(*this);
            }

            if constexpr( &custom_system::msgFrameStart != &overrides::msgFrameStart )
            {
                user_system_t::m_World.m_Graph.m_Events.m_FrameStart.AddDelegate<&custom_system::msgFrameStart>(*this);
            }

            if constexpr( &custom_system::msgFrameDone != &overrides::msgFrameDone )
            {
                user_system_t::m_World.m_Graph.m_Events.m_FrameDone.AddDelegate<&custom_system::msgFrameDone>(*this);
            }
        }

        template< typename T_SYSTEM > inline
        void custom_system<T_SYSTEM>::qt_onRun( void ) noexcept
        {
            XCORE_PERF_ZONE_SCOPED_N(user_system_t::name_v.m_Str)
            if constexpr ( &user_system_t::msgUpdate != &system::overrides::msgUpdate )
            {
                user_system_t::msgUpdate();
            }
            else
            {
                user_system_t::m_World.m_ArchetypeDB.template DoQuery< user_system_t, query_v >(user_system_t::m_Query);
                user_system_t::ForEach( user_system_t::m_Query, *this, user_system_t::entities_per_job_v );
            }
        }

        template< typename T_SYSTEM > inline
        void custom_system<T_SYSTEM>::msgSyncPointDone( mecs::sync_point::instance& Syncpoint ) noexcept
        {
            // Call the user function if he overwrote it
            if constexpr ( &user_system_t::msgSyncPointDone != &system::overrides::msgSyncPointDone )
            {
                user_system_t::msgSyncPointDone(Syncpoint);
            }

            //
            // unlock all the groups that we need to work with so that no one tries to add/remove entities
            //
            mecs::archetype::details::safety::UnlockQueryComponents(*this, user_system_t::m_Query);

            //
            // unlock and sync groups from the cache
            //
            /*
            for( const auto& E : user_system_t::m_Cache.m_Lines )
            {
                bool bFound = false;
                std::as_const(E.m_pGroup->m_SemaphoreLock).unlock();

                for( const auto& Q : user_system_t::m_Query.m_lResults )
                {
                    if( E.m_pGroup == Q.m_pGroup )
                    {
                        bFound = true;
                        break;
                    }
                }

                if( bFound == false ) E.m_pGroup->MemoryBarrierSync( Syncpoint );
            }
            */

            //
            // Let the archetypes that we used do their memory barriers
            //
            for( auto& R : user_system_t::m_Query.m_lResults )
            {
                R.m_pArchetype->MemoryBarrierSync( Syncpoint );
            }

            //user_system_t::m_Cache.m_Lines.clear();
            user_system_t::m_Query.m_lResults.clear();
        }

        template< typename T_SYSTEM > inline
        void* custom_system<T_SYSTEM>::DetailsGetExclusiveRealEvent( const system::event::type_guid Guid ) noexcept
        {
            for( int Index = 0, end = static_cast<int>(exclusive_events_descriptors_v.size()); Index != end ; ++Index )
            {
                if(exclusive_events_descriptors_v[Index]->m_Guid == Guid )
                {
                    auto* p = reinterpret_cast<mecs::tools::event<>*>(&m_ExclusiveEvents);
                    return &p[Index];
                }
            }
            return nullptr;
        }

        template< typename T_CALLBACK >
        struct process_resuts_params
        {
            using call_back_t = T_CALLBACK;

            xcore::scheduler::channel&              m_Channel;
            const T_CALLBACK&&                      m_Callback;
            const int                               m_nEntriesPerJob;
        };

        template< typename T, typename...T_ARGS> constexpr xforceinline
        auto ProcessInitArray( std::tuple<T_ARGS...>*, int iStart, int Index, archetype::query::result_entry& R, T& Params ) noexcept
        {
            using       func_tuple  = std::tuple<T_ARGS...>;
            auto&       Specialized = R.m_pArchetype->m_MainPool.getComponentByIndex<mecs::archetype::specialized_pool>(Index, 0);
            const auto  Decide      = [&]( int iArg, auto Arg ) constexpr noexcept
            {
                using       t = xcore::types::decay_full_t<decltype(Arg)>;
                const auto& I = R.m_lFunctionToArchetype[iArg];

                if( I.m_Index == mecs::archetype::query::result_entry::invalid_index) return reinterpret_cast<std::byte*>(nullptr);

                if( I.m_isShared ) return reinterpret_cast<std::byte*>(&R.m_pArchetype->m_MainPool.getComponentByIndex<t>( Index, I.m_Index ));

                return reinterpret_cast<std::byte*>(&Specialized.m_EntityPool.getComponentByIndex<t>( iStart, I.m_Index ));
            };
            
            return std::array<std::byte*, sizeof...(T_ARGS)>
            {
                Decide( xcore::types::tuple_t2i_v<T_ARGS, func_tuple>, reinterpret_cast<xcore::types::decay_full_t<T_ARGS>*>(nullptr) ) ...
            };
        }

        template< typename T, typename...T_ARGS> constexpr xforceinline
        void ProcesssCall(std::tuple<T_ARGS...>*, std::span<std::byte*> Span, T& Params, int iStart, const int iEnd ) noexcept
        {
            using func_tuple    = std::tuple<T_ARGS...>;

            xassert(iStart != iEnd);

            // Call the system
            do
            {
                Params.m_Callback
                (
                    ([&]() constexpr noexcept -> T_ARGS
                    {
                        auto& p         = Span[xcore::types::tuple_t2i_v<T_ARGS, func_tuple>];
                        auto  pBackup   = p;
                        if constexpr (mecs::component::descriptor_v<T_ARGS>.m_Type != mecs::component::type::SHARE)
                        {
                            if constexpr (std::is_pointer_v<T_ARGS>) { if (p) p += sizeof(xcore::types::decay_full_t<T_ARGS>); }
                            else                                              p += sizeof(xcore::types::decay_full_t<T_ARGS>);
                        }

                        if constexpr (std::is_pointer_v<T_ARGS>) return reinterpret_cast<T_ARGS>(pBackup);
                        else                                     return reinterpret_cast<T_ARGS>(*pBackup);
                    }())...
                );
            } while( ++iStart != iEnd );
        }
    }

    //----------------------------------------------------------------------------
    // SYSTEM:: INSTANCE
    //----------------------------------------------------------------------------

    xforceinline
    instance::instance( const construct&& Settings) noexcept
        : overrides{ Settings.m_JobDefinition | xcore::scheduler::triggers::DONT_CLEAN_COUNT }
        , m_World{ Settings.m_World }
        , m_Guid{ Settings.m_Guid }
    { setupName(Settings.m_Name); }

    template< typename T_CALLBACK > constexpr xforceinline
    void instance::ForEach(mecs::archetype::query::instance& QueryI, T_CALLBACK&& Functor, int nEntitiesPerJob) noexcept
    {
        // If there is nothing to do then lets bail out
        if (QueryI.m_lResults.size() == 0)
            return;

        //
        // Check for graph errors by Locking the relevant components in the archetypes
        //
        if (mecs::archetype::details::safety::LockQueryComponents(*this, QueryI))
        {
            // we got an error here
            xassert(false);
        }

        //
        // Loop through all the results and lets the system deal with the components
        //
        xcore::scheduler::channel       Channel(getName());
        using                           params_t = details::process_resuts_params<T_CALLBACK>;
        params_t                        Params
        {
            Channel
            , std::forward<T_CALLBACK>(Functor)
            , nEntitiesPerJob
        };

        for (auto& R : QueryI.m_lResults)
        {
            auto& MainPool = R.m_pArchetype->m_MainPool;
            xassert(MainPool.size());
            for (int end = static_cast<int>(MainPool.size()), i = 0; i < end; ++i)
            {
                ProcessResult(Params, R, MainPool, i);
            }
        }

        Channel.join();
    }

    template< typename...T_SHARE_COMPONENTS > xforceinline
    mecs::archetype::entity_creation instance::createEntities(mecs::archetype::instance::guid gArchetype, int nEntities, std::span<entity::guid> gEntitySpan, T_SHARE_COMPONENTS&&... ShareComponents) noexcept
    {
        auto& Archetype = m_World.m_ArchetypeDB.getArchetype( gArchetype );
        return Archetype.CreateEntities( *this, nEntities, gEntitySpan, std::forward<T_SHARE_COMPONENTS>(ShareComponents)...);
    }

    inline
    void instance::deleteEntity(mecs::component::entity& Entity ) noexcept
    {
        XCORE_PERF_ZONE_SCOPED()
        if (Entity.isZombie()) return;
        Entity.getReference().m_pPool->m_pArchetypeInstance->deleteEntity(*this, Entity);
    }

    template< typename T_ADD_COMPONENTS_AND_TAGS, typename T_REMOVE_COMPONENTS_AND_TAGS, typename T_CALLBACK > constexpr xforceinline
    void instance::getArchetypeBy(mecs::component::entity& Entity, T_CALLBACK&& Callback) noexcept
    {
        auto& OldArchetype  = *Entity.getReference().m_pPool->m_pArchetypeInstance;

        // Ok we need to fill the cache
        auto& NewArchetype = m_World.m_ArchetypeDB.getOrCreateGroupBy(
              OldArchetype
            , (T_ADD_COMPONENTS_AND_TAGS*)nullptr
            , (T_REMOVE_COMPONENTS_AND_TAGS*)nullptr);

        // Handle the callback
        Callback(NewArchetype);
    }

    //----------------------------------------------------------------------------------------------------

    template< typename T_CALLBACK, typename...T_SHARE_COMPONENTS > constexpr xforceinline
    void instance::moveEntityToArchetype(mecs::component::entity& Entity, mecs::archetype::instance& ToNewArchetype, T_CALLBACK&& Callback, T_SHARE_COMPONENTS&&... ShareComponents) noexcept
    {
        m_World.m_ArchetypeDB.moveEntityToArchetype(*this, Entity, ToNewArchetype, std::forward<T_CALLBACK>(Callback), std::forward<T_SHARE_COMPONENTS>(ShareComponents) ...);
    }

    //----------------------------------------------------------------------------------------------------
    const time& instance::getTime(void) const noexcept
    {
        return m_World.m_GraphDB.m_Time;
    }

    //----------------------------------------------------------------------------------------------------
    template< typename T_PARAMS > constexpr xforceinline
    void instance::ProcessResult( T_PARAMS& Params, mecs::archetype::query::result_entry& R, mecs::entity_pool::instance& MainPool, const int Index ) noexcept
    {
        auto&  SpecializedPool = MainPool.getComponentByIndex<mecs::archetype::specialized_pool>(Index, 0);
        for( int end=static_cast<int>(SpecializedPool.m_EntityPool.size()), i=0; i<end; )
        {
            const int MyEnd = std::min<int>(i+Params.m_nEntriesPerJob, end);
            Params.m_Channel.SubmitJob(
            [
                iStart  = i
            ,   iEnd    = MyEnd
            ,   &R
            ,   &Params
            ,   Index
            ] () constexpr noexcept
            {
                using function_arg_tuple = typename xcore::function::traits<T_PARAMS::call_back_t>::args_tuple;
                auto  Pointers           = details::ProcessInitArray(reinterpret_cast<function_arg_tuple*>(nullptr), iStart, Index, R, Params);

                details::ProcesssCall
                (
                    reinterpret_cast<function_arg_tuple*>(nullptr)
                ,   Pointers
                ,   Params
                ,   iStart
                ,   iEnd );
            });
            i = MyEnd;
        }
    }

    //----------------------------------------------------------------------------------------------------
    template< typename T_EVENT, typename T_SYSTEM, typename...T_ARGS > xforceinline constexpr
    void instance::EventNotify( T_ARGS&&...Args ) noexcept
    {
        static_assert(std::is_same_v< T_EVENT::system_t, void> == std::is_base_of_v<mecs::system::event::global, T_EVENT>
            , "Global events should not override system_t.");
        static_assert(std::is_base_of_v<mecs::system::event::exclusive, T_EVENT> || std::is_base_of_v<mecs::system::event::global, T_EVENT>
            , "You are using an event which is not mecs::system::event::global or mecs::system::event::local derived.");

        using self = details::custom_system<T_SYSTEM>;
        auto& This = static_cast<self&>(*this);

        if constexpr ( std::is_base_of_v<mecs::system::event::exclusive, T_EVENT> )
        {
            static_assert( !!std::tuple_size_v<self::exclusive_real_events_t> );
            auto& Event = std::get< typename T_EVENT::real_event_t >( This.m_ExclusiveEvents );
            Event.NotifyAll( *this, std::forward<T_ARGS>(Args)... );
        }
        else
        {
            auto& Event = *std::get< typename T_EVENT::real_event_t* >( This.m_GlobalEvents );
            Event.NotifyAll( *this, std::forward<T_ARGS>(Args)... );
        }
    }

    //---------------------------------------------------------------------------------
    // SYSTEM:: DESCRIPTOR DATA BASE
    //---------------------------------------------------------------------------------

    namespace details
    {
        template< typename T_SYSTEM >
        constexpr auto MakeDescriptor(void) noexcept
        {
            using sys = mecs::system::details::custom_system<T_SYSTEM>;
            
            return mecs::system::descriptor
            {
                T_SYSTEM::type_guid_v.isValid() ? T_SYSTEM::type_guid_v : type_guid{ __FUNCSIG__ }
                , [](const mecs::system::overrides::construct&& C) noexcept
                {
                    auto p = new sys{ std::move(C) };
                    return std::unique_ptr<mecs::system::instance>{ static_cast<mecs::system::instance*>(p) };
                }
                , T_SYSTEM::name_v
                , sys::exclusive_events_descriptors_v
                , sys::global_events_descriptors_v
            };
        }
    }

    //---------------------------------------------------------------------------------
    // SYSTEM:: DESCRIPTOR DATA BASE
    //---------------------------------------------------------------------------------
    template< typename...T_SYSTEMS >
    void descriptors_data_base::Register( mecs::system::event::descriptors_data_base& EventDescriptionDataBase ) noexcept
    {
        static_assert(!!sizeof...(T_SYSTEMS));

        if((m_mapDescriptors.alloc(descriptor_v<T_SYSTEMS>.m_Guid, [&](const descriptor*& Ptr)
        {
            m_lDescriptors.push_back(Ptr = &descriptor_v<T_SYSTEMS>);

            // Register all the events types
            using sys = details::custom_system<T_SYSTEMS>;
            EventDescriptionDataBase.TupleRegister( reinterpret_cast<typename sys::exclusive_event_tuple_t*>(nullptr) );
            EventDescriptionDataBase.TupleRegister(reinterpret_cast<typename sys::global_event_tuple_t*>(nullptr));

        }) || ...))
        {
            // Make sure that someone did not forget the set the right guid
            xassert( ((m_mapDescriptors.get(descriptor_v<T_SYSTEMS>.m_Guid) == &descriptor_v<T_SYSTEMS>) && ... ) );
        }
    }

    //---------------------------------------------------------------------------------
    // SYSTEM:: INSTANCE DATA BASE
    //---------------------------------------------------------------------------------
    inline
    instance& instance_data_base::Create( const mecs::system::descriptor& Descriptor, mecs::system::instance::guid Guid ) noexcept
    {
        instance* p = nullptr;
        mecs::system::instance::construct Construct
        {
            Guid
        ,   m_World
        ,   Descriptor.m_Name
        ,   { xcore::scheduler::definition::definition::Flags
                (
                    xcore::scheduler::lifetime::DONT_DELETE_WHEN_DONE
                    , xcore::scheduler::jobtype::NORMAL
                    , xcore::scheduler::affinity::NORMAL
                    , xcore::scheduler::priority::NORMAL
                    , xcore::scheduler::triggers::DONT_CLEAN_COUNT
                )
            }
        };

        m_mapInstances.getOrCreate( Descriptor.m_Guid, [&](map_systems::value& E ) noexcept
        {
            p = E.get();
        }
        , [&]( map_systems::value& E ) noexcept
        {
            E = Descriptor.m_fnCreate(std::move(Construct));
            p = E.get();

            m_lInstance.push_back(p);

            if (m_Event.m_CreatedSystem.hasSubscribers())
                m_Event.m_CreatedSystem.NotifyAll(*p);
        });

        xassert(p);
        return *p;
    }

}