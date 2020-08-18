namespace mecs::system
{
    using type_guid = xcore::guid::unit<64, struct type_tag>;

    namespace details
    {
        struct cache
        {
            struct line
            {
                //component::group::guid          m_Guid;
                //component::group::instance* m_pGroup;
            };

            xcore::lock::semaphore  m_Lock;
            std::vector<line>       m_Lines;
        };
    }

    //----------------------------------------------------------------------------
    // SYSTEM:: INSTANCE OVERWRITES
    //----------------------------------------------------------------------------

    struct overrites : xcore::scheduler::job<mecs::settings::max_syncpoints_per_system>
    {
        using job_t     = xcore::scheduler::job<mecs::settings::max_syncpoints_per_system>;
        using guid      = xcore::guid::unit<64, struct mecs_system_tag>;
        using type_guid = mecs::system::type_guid;

        using job_t::job_t;

        // Structure used for the construction of the hierarchy 
        struct construct
        {
            guid                            m_Guid;
            mecs::world::instance&          m_World;
            xcore::string::const_universal  m_Str;
            xcore::scheduler::definition    m_Def;
        };

        // Defaults that can be overwritten by the user
        constexpr static auto                   entities_per_job_v  = 5000;
        constexpr static auto                   type_guid_v         = mecs::system::type_guid{ nullptr };
        constexpr static auto                   name_v              = xconst_universal_str("mecs::System(Unnamed)");
        using                                   query_t             = std::tuple<>;
        using                                   events_t            = std::tuple<>;

        // Messages that we can handle, these can be overwritten by the user
        inline void msgSyncPointDone    (mecs::sync_point::instance&)   noexcept {}
        inline void msgSyncPointStart   (mecs::sync_point::instance&)   noexcept {}
        inline void msgGraphInit        (mecs::world::instance&)        noexcept {}
        inline void msgFrameStart       (void)                          noexcept {}
        inline void msgFrameDone        (void)                          noexcept {}
        inline void msgUpdate           (void)                          noexcept {}
    };

    //----------------------------------------------------------------------------
    // SYSTEM:: INSTANCE
    //----------------------------------------------------------------------------
    namespace details
    {
        template< typename T_CALLBACK >
        struct process_resuts_params
        {
            using call_back_t = T_CALLBACK;

            xcore::scheduler::channel&              m_Channel;
            T_CALLBACK&&                            m_Callback;
            int                                     m_nEntriesPerJob;
        };

        template< typename T, typename...T_ARGS> constexpr xforceinline
        auto ProcessInitArray( std::tuple<T_ARGS...>*, int iStart, int Index, archetype::query::result_entry& R, T& Params ) noexcept
        {
            using       func_tuple  = std::tuple<T_ARGS...>;
            auto&       Specialized = R.m_pArchetype->m_MainPool.getComponentByIndex<mecs::archetype::specialized_pool>(Index, 0);
            const auto  Decide      = [&]( int iArg, auto Arg ) constexpr
            {
                using       t = xcore::types::decay_full_t<decltype(Arg)>;
                const auto& I = R.m_lFunctionToArchetype[iArg];

                if( I.m_Index == mecs::archetype::query::result_entry::invalid_index) return reinterpret_cast<std::byte*>(nullptr);

                if( I.m_isShared )
                {
                    return reinterpret_cast<std::byte*>(&R.m_pArchetype->m_MainPool.getComponentByIndex<t>( Index, I.m_Index ));
                }
                else
                {
                    return reinterpret_cast<std::byte*>(&Specialized.m_EntityPool.getComponentByIndex<t>(iStart, I.m_Index));
                }
            };
            
            return std::array<std::byte*, sizeof...(T_ARGS)>
            {
                Decide( xcore::types::tuple_t2i_v<T_ARGS, func_tuple>, reinterpret_cast<xcore::types::decay_full_t<T_ARGS>*>(nullptr) ) ...
            };
        }

        template< typename T, typename...T_ARGS> constexpr xforceinline
        void ProcesssCall(std::tuple<T_ARGS...>*, std::span<std::byte*> Span, T& Params) noexcept
        {
            using func_tuple    = std::tuple<T_ARGS...>;

            // Call the system
            Params.m_Callback
            (
                ([&]() constexpr noexcept 
                {
                    auto& p  = Span[xcore::types::tuple_t2i_v<T_ARGS, func_tuple>];
                    auto  pp = p;
                    if constexpr (mecs::component::descriptor_v<T_ARGS>.m_Type != mecs::component::type::SHARE)
                    {
                        if constexpr (std::is_pointer_v<T_ARGS>) { if (p) p += sizeof(xcore::types::decay_full_t<T_ARGS>); }
                        else                                              p += sizeof(xcore::types::decay_full_t<T_ARGS>);
                    }

                    if constexpr (std::is_pointer_v<T_ARGS>) return reinterpret_cast<T_ARGS>(pp);
                    else                                     return reinterpret_cast<T_ARGS>(*pp);
                }())...
            );
        }
    }

    struct instance : overrites
    {
        instance(const construct&& Settings) noexcept
            : overrites{ Settings.m_Def | xcore::scheduler::triggers::DONT_CLEAN_COUNT }
            , m_World            { Settings.m_World }
            , m_Guid             { Settings.m_Guid }
        { setupName(Settings.m_Str); }

        template< typename T_CALLBACK > constexpr xforceinline
        void ForEach( mecs::archetype::query::instance& QueryI, T_CALLBACK&& Functor, int nEntitiesPerJob ) noexcept
        {
            // If there is nothing to do then lets bail out
            if( QueryI.m_lResults.size() == 0 )
                return;

            xcore::scheduler::channel       Channel(getName());
            using                           params_t = details::process_resuts_params<T_CALLBACK>;
            params_t                        Params
            {
                Channel
            ,   std::forward<T_CALLBACK>(Functor)
            ,   nEntitiesPerJob
            };

            for( auto& R : QueryI.m_lResults )
            {
                auto& MainPool = R.m_pArchetype->m_MainPool;
                xassert(MainPool.size());
                for( int end = static_cast<int>(MainPool.size()), i=0; i<end; ++i )
                {
                    ProcessResult( Params, R, MainPool, i);
                }
            }

            Channel.join();
        }

        template< typename T_PARAMS > constexpr xforceinline
        void ProcessResult( T_PARAMS& Params, archetype::query::result_entry& R, mecs::entity_pool::instance& MainPool, int Index ) noexcept
        {
            auto&  SpecializedPool = MainPool.getComponentByIndex<mecs::archetype::specialized_pool>(Index, 0);
            for( int end=static_cast<int>(SpecializedPool.m_EntityPool.size()), i=0; i<end; )
            {
                int MyEnd = std::min<int>(i+Params.m_nEntriesPerJob, end);
                Params.m_Channel.SubmitJob(
                [
                    iStart  = i
                ,   iEnd    = MyEnd
                ,   &R
                ,   &Params
                ,   Index
                ]
                {
                    using function_arg_tuple = typename xcore::function::traits<T_PARAMS::call_back_t>::args_tuple;
                    auto Pointers = details::ProcessInitArray( reinterpret_cast<function_arg_tuple*>(nullptr), iStart, Index, R, Params );

                    for( int i=iStart; i!=iEnd; ++i )
                    {
                        details::ProcesssCall( reinterpret_cast<function_arg_tuple*>(nullptr), Pointers, Params);
                    }
                });
                i = MyEnd;
            }
        }

        // TODO: This will be private
        mecs::world::instance&              m_World;
        system::instance::guid              m_Guid;
        mecs::archetype::query::instance    m_Query{};

        // query::instance     m_Query;
        // details::cache      m_Cache;
    };

    //---------------------------------------------------------------------------------
    // SYSTEM:: DETAILS::CUSTOM_SYSTEM
    //---------------------------------------------------------------------------------
    namespace details
    {
        //---------------------------------------------------------------------------------

        template< typename...T_ARGS >
        std::tuple< xcore::types::make_unique< typename T_ARGS::event_t, T_ARGS >... > GetEventTuple(std::tuple<T_ARGS...>*);

        //---------------------------------------------------------------------------------

        template< typename...T_ARGS > constexpr 
        auto GetEventGuids(std::tuple<T_ARGS...>*) noexcept
        {
            if constexpr ( !!sizeof...(T_ARGS))
            { 
                return std::array{ T_ARGS::type_guid_v ... };
            }
            else
            {
                return std::array<mecs::system::event::type_guid,0>{};
            }
        }

        //---------------------------------------------------------------------------------
        // SYSTEM::CUSTOM SYSTEM 
        //---------------------------------------------------------------------------------
        template< typename T_SYSTEM >
        struct custom_system final : T_SYSTEM
        {
            using                   user_system_t       = T_SYSTEM;
            using                   world_instance_t    = std::decay_t< decltype(user_system_t::m_World) >;
            constexpr static auto   query_v             = mecs::archetype::query::details::define<typename user_system_t::query_t>{};
            using                   events_tuple        = decltype(GetEventTuple(reinterpret_cast<typename user_system_t::events_t*>(nullptr)));
            static constexpr auto   events_guids        = std::array{ GetEventGuids(reinterpret_cast<typename user_system_t::events_t*>(nullptr)) };

            using user_system_t::user_system_t;

            events_tuple m_Events{};

            virtual void qt_onRun(void) noexcept override
            {
                XCORE_PERF_ZONE_SCOPED_N(user_system_t::name_v.m_Str)
                if constexpr ( &user_system_t::msgUpdate != &system::overrites::msgUpdate )
                {
                    user_system_t::msgUpdate();
                }
                else
                {
                    user_system_t::m_World.m_ArchetypeDB.template DoQuery< user_system_t, query_v >(user_system_t::m_Query);
                    user_system_t::ForEach( user_system_t::m_Query, *this, user_system_t::entities_per_job_v );
                }
            }
        };
    }

    //---------------------------------------------------------------------------------
    // SYSTEM:: descriptor
    //---------------------------------------------------------------------------------
    struct descriptor
    {
        using fn_create = std::unique_ptr<mecs::system::instance>( mecs::system::instance::construct&& C ) noexcept;

        mecs::system::type_guid                         m_Guid;
        fn_create*                                      m_fnCreate;
        xcore::string::const_universal                  m_Name;
        std::uint32_t                                   m_EventOffset;
        std::span<const mecs::system::event::type_guid> m_EventTypeGuids;
    };
    
    namespace details
    {
        template< typename T_SYSTEM >
        constexpr auto MakeDescriptor( void ) noexcept
        {
            using sys = mecs::system::details::custom_system<T_SYSTEM>;
            static_assert(T_SYSTEM::type_guid_v.isValid());
            static_assert( std::is_same_v<decltype(T_SYSTEM::type_guid_v), const mecs::system::type_guid> );

            return mecs::system::descriptor
            {
                T_SYSTEM::type_guid_v
            ,   []( mecs::system::overrites::construct&& C ) noexcept
                {
                    auto p = new sys{ std::move(C) };
                    return std::unique_ptr<mecs::system::instance>{ static_cast<mecs::system::instance*>(p) };
                }
            ,   T_SYSTEM::name_v
            ,   static_cast<std::uint32_t>(offsetof( sys, m_Events ))
            ,   sys::events_guids
            };
        }
    }

    template< typename T_SYSTEM >
    static constexpr auto descriptor_v = details::MakeDescriptor<T_SYSTEM>();

    //---------------------------------------------------------------------------------
    // SYSTEM:: DESCRIPTOR DATA BASE
    //---------------------------------------------------------------------------------

    struct descriptors_data_base
    {
        using map_system_types = mecs::tools::fixed_map< mecs::system::type_guid, const descriptor*, mecs::settings::max_systems >;

        void Init( void ) noexcept {}

        template< typename...T_SYSTEMS >
        void Register( mecs::system::event::descriptors_data_base& EventDescriptionDataBase ) noexcept
        {
            static_assert(!!sizeof...(T_SYSTEMS));

            if((m_mapDescriptors.alloc(descriptor_v<T_SYSTEMS>.m_Guid, [&](const descriptor*& Ptr)
            {
                m_lDescriptors.push_back(Ptr = &descriptor_v<T_SYSTEMS>);

                // Register all the events types
                using sys = details::custom_system<T_SYSTEMS>;
                EventDescriptionDataBase.TupleRegister( reinterpret_cast<typename sys::events_tuple*>(nullptr) );

            }) || ...))
            {
                // Make sure that someone did not forget the set the right guid
                xassert( ((m_mapDescriptors.get(descriptor_v<T_SYSTEMS>.m_Guid) == &descriptor_v<T_SYSTEMS>) && ... ) );
            }
        }

        std::vector<const descriptor*>      m_lDescriptors;
        map_system_types                    m_mapDescriptors;
    };
    
    //---------------------------------------------------------------------------------
    // SYSTEM:: INSTANCE DATA BASE
    //---------------------------------------------------------------------------------
    struct instance_data_base
    {
        using map_systems = mecs::tools::fixed_map< mecs::system::type_guid, std::vector<std::unique_ptr<mecs::system::instance>>, mecs::settings::max_systems >;

        instance_data_base( mecs::world::instance& World ) : m_World(World) {}

        void Init( void ) noexcept {}

        instance& Create( const mecs::system::descriptor& Descriptor, mecs::system::instance::guid Guid ) noexcept
        {
            instance* p = nullptr;
            mecs::system::instance::construct Construct
            {
                Guid
            ,   m_World
            ,   Descriptor.m_Name
            ,   {}
            };

            auto AddIt = [&](map_systems::value& Vec) noexcept
            {
                xassert_block_basic()
                {
                    for( const auto& E : Vec ) xassert( E->m_Guid == Guid );
                }

                // Create the entry
                {
                    std::unique_ptr<mecs::system::instance> I = Descriptor.m_fnCreate(std::move(Construct));
                    p = I.get();
                    Vec.push_back(std::move(I));
                }

                p->m_Guid = Guid;
                m_lInstance.push_back(p);

                for( auto& E : Descriptor.m_EventTypeGuids )
                {
                }
            };

            m_mapInstances.getOrCreate( Descriptor.m_Guid, AddIt, AddIt );

            xassert(p);
            return *p;
        }

        template< typename T_SYSTEM >
        instance& Create(instance::guid Guid)
        {
            return Create(descriptor_v<T_SYSTEM>, Guid);
        }

        mecs::world::instance&              m_World;
        std::vector<instance*>              m_lInstance;
        map_systems                         m_mapInstances;
    };
}