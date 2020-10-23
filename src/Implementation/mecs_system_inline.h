namespace mecs::system
{
    //---------------------------------------------------------------------------------
    // SYSTEM:: DETAILS::CUSTOM_SYSTEM
    //---------------------------------------------------------------------------------
    namespace details
    {
        //---------------------------------------------------------------------------------

        template< bool T_IS_GLOBAL_V, typename...T_ARGS >
        auto GetRealEventsTuple( std::tuple<T_ARGS...>* )
        {
            if constexpr (sizeof...(T_ARGS) == 0) return std::tuple<>{};
            else if constexpr (T_IS_GLOBAL_V)     return std::tuple< typename T_ARGS::real_event_t* ... >{};
            else                                  return std::tuple< typename T_ARGS::real_event_t  ... >{};
        }

        //---------------------------------------------------------------------------------

        template< typename event_type, typename...T_ARGS >
        auto FilterEventsTuple( std::tuple<T_ARGS...>* ) ->
            xcore::types::tuple_cat_t< std::conditional_t< std::is_base_of_v< event_type, T_ARGS>, std::tuple<T_ARGS>, std::tuple<> > ... >;

        //---------------------------------------------------------------------------------

        template< typename...T_ARGS > xforceinline constexpr
        auto GetEventDescriptorArray(std::tuple<T_ARGS...>*) noexcept
        {
            if constexpr (!!sizeof...(T_ARGS))  return std::array{ &mecs::system::event::descriptor_v<T_ARGS> ... };
            else                                return std::array< const mecs::system::event::descriptor*, 0>{};
        }

        //---------------------------------------------------------------------------------

        template< typename...T_ARGS, typename T_CALLBACK > constexpr
        auto tuple_type_apply(std::tuple<T_ARGS...>*, T_CALLBACK&& Callback ) noexcept
        {
            // Types must be wrapped around a tuple in case of references
            return Callback( static_cast<std::tuple<T_ARGS>*>(nullptr) ... );
        }

        //---------------------------------------------------------------------------------

        template< typename...T_ARGS > constexpr
        auto ComputeUpdateBits( std::tuple<T_ARGS...>* ) noexcept
        {
            tools::bits Bits{ nullptr };

            (( [&]()
            {
                if constexpr ( std::is_const_v< std::remove_pointer_t<std::remove_reference_t<T_ARGS>> > == false )
                {
                    Bits.AddBit( mecs::component::descriptor_v<T_ARGS>.m_BitNumber );
                }
            }()), ... );
            
            return Bits;
        };

        //---------------------------------------------------------------------------------

        template< typename... T_PARAMS >
        xforceinline constexpr
        std::uint64_t getFunctionGUID( std::tuple<T_PARAMS...>* ) noexcept
        {
            std::uint64_t Val1 = mecs::tools::hash::combine( mecs::component::descriptor_v<T_PARAMS>.m_Guid.m_Value ... );
            std::uint64_t Val2 = mecs::tools::hash::combine( Val1, (std::is_pointer_v<T_PARAMS> ? mecs::component::descriptor_v<T_PARAMS>.m_Guid.m_Value : 0ull) ... );
            std::uint64_t Val3 = mecs::tools::hash::combine( Val2, (std::is_same_v
                                                           <
                                                                std::remove_reference_t<
                                                                    std::remove_pointer_t<T_PARAMS>
                                                                >
                                                           ,    xcore::types::decay_full_t<T_PARAMS>
                                                           > ? mecs::component::descriptor_v<T_PARAMS>.m_Guid.m_Value 
                                                             : 0ull) ... );
            return Val3;
        }

        //---------------------------------------------------------------------------------

        template< typename T_GET_CALLBACK
                , typename T_CREATE_CALLBACK >
        xforceinline
        void details::cache::getOrCreateCache
        ( const std::uint64_t           FunctionGUID
        , mecs::archetype::instance&    Archetype
        , T_GET_CALLBACK&&              GetCallBack
        , T_CREATE_CALLBACK&&           CreateCallback
        ) noexcept
        {
           // XCORE_PERF_ZONE_SCOPED_N("getOrCreateCache")

            //
            // Stage one, look entry in the cache
            //            
            int  i      = 0;
            _TRY_AGAIN:
            {
               // XCORE_PERF_ZONE_SCOPED_N("Searching")
                for( auto nLines = m_nLines.load(std::memory_order_relaxed); i != nLines; ++i )
                {
                    mecs::archetype::instance* pLineArchetype = m_Lines[i].load(std::memory_order_relaxed);

                    if( pLineArchetype == nullptr )
                    {
                        XCORE_PERF_ZONE_SCOPED_NC("mecs::system::cache:: Waiting for lines", tracy::Color::ColorType::Red)
                        do
                        {
                            // Spining and waiting for someone to finish setting the rest of the functions
                            pLineArchetype = m_Lines[i].load(std::memory_order_relaxed);
                        } while (pLineArchetype == nullptr );
                    }

                    // Linear search
                    if(pLineArchetype != &Archetype )
                        continue;

                    // There are not gets/creates for these kind
                    if( FunctionGUID == 0ull )
                        return;

                    // 
                    // We found the entry (potential cache hit)
                    //
                    int                 j       = 0;
                    auto&               Data    = m_Data[i];

                    // Search which function we are
                    _TRY_AGAIN_PER_FUNCTION:
                    {
                        for( auto nFunctions = Data.m_nFunctions.load(std::memory_order_relaxed); j != nFunctions; ++j )
                        {
                            auto Which = Data.m_WhichFunction[j].load(std::memory_order_relaxed);
                            if(Which == 0 )
                            {
                                XCORE_PERF_ZONE_SCOPED_NC("mecs::system::cache::Waiting for functions", tracy::Color::ColorType::Red)
                                do
                                {
                                    // Spining and waiting for someone to finish setting the rest of the functions
                                    Which = Data.m_WhichFunction[j].load(std::memory_order_relaxed);
                                } while( Which == 0 );
                            }

                            if (Which == FunctionGUID)
                            {
                                const auto& PerFunction = Data.m_PerFunction[j];
                                GetCallBack( PerFunction );
                                return;
                            }
                        }
                    }

                    // Create entry
                    {
                        // Lets try to add our new entry
                        if( false == Data.m_nFunctions.compare_exchange_weak(j, j+1) )
                            goto _TRY_AGAIN_PER_FUNCTION;

                        // Ok now Create new Entry
                        CreateCallback(Data.m_PerFunction[j]);

                        // Done so we can officially added to our entry
                        Data.m_WhichFunction[j].store(FunctionGUID, std::memory_order_relaxed);

                        return;
                    }
                }
            }

            //
            // Not in the cache so we must create it
            //
            {
                int x = i;
                if (false == m_nLines.compare_exchange_weak(i, i + 1))
                    goto _TRY_AGAIN;

                data& MyData = m_Data[i];
                if( FunctionGUID )
                {
                    CreateCallback(MyData.m_PerFunction[0]);
                    MyData.m_nFunctions.store( 1, std::memory_order_relaxed );
                    MyData.m_WhichFunction[0].store(FunctionGUID, std::memory_order_relaxed);
                }
                else
                {
                    MyData.m_nFunctions.store(0, std::memory_order_relaxed);
                }

                // okay release it
                m_Lines[i].store(&Archetype, std::memory_order_relaxed );
            }
        }

        //---------------------------------------------------------------------------------
        // SYSTEM::CUSTOM SYSTEM 
        //---------------------------------------------------------------------------------
        template< typename T_SYSTEM >
        struct custom_system final : T_SYSTEM
        {
            static_assert( std::is_base_of_v< mecs::system::overrides, T_SYSTEM >, "This is not a system" );
            using                           user_system_t                   = T_SYSTEM;
            using                           global_event_tuple_t            = decltype(FilterEventsTuple<system::event::global>(reinterpret_cast<typename user_system_t::events_t*>(nullptr)));
            using                           global_real_events_t            = decltype(GetRealEventsTuple<true>(reinterpret_cast<global_event_tuple_t*>(nullptr)));
            inline static constexpr auto    global_events_descriptors_v     = GetEventDescriptorArray(reinterpret_cast<global_event_tuple_t*>(nullptr));
            using                           exclusive_event_tuple_t         = decltype(FilterEventsTuple<system::event::exclusive>(reinterpret_cast<typename user_system_t::events_t*>(nullptr)));
            using                           exclusive_real_events_t         = decltype(GetRealEventsTuple<false>(reinterpret_cast<exclusive_event_tuple_t*>(nullptr)));
            inline static constexpr auto    exclusive_events_descriptors_v  = GetEventDescriptorArray(reinterpret_cast<exclusive_event_tuple_t*>(nullptr));
            
            exclusive_real_events_t     m_ExclusiveEvents   {};
            global_real_events_t        m_GlobalEvents      {};

            //----------------------------------------------------------------------------------------------
            constexpr
                                    custom_system                       ( typename T_SYSTEM::construct&& Settings 
                                                                        ) noexcept;
            //----------------------------------------------------------------------------------------------
            virtual
            void                    qt_onRun                            ( void 
                                                                        ) noexcept override;
            //----------------------------------------------------------------------------------------------
            inline
            void                    msgSyncPointDone                    ( mecs::sync_point::instance& Syncpoint 
                                                                        ) noexcept;
            //----------------------------------------------------------------------------------------------
            virtual
            void*                   _getExclusiveRealEvent              ( const system::event::type_guid Guid 
                                                                        ) const noexcept override;
            //----------------------------------------------------------------------------------------------
            template< typename...T_REAL_EVENTS >
            xforceinline constexpr
            void*                   getExclusiveRealEventHelper         ( const system::event::type_guid Guid
                                                                        , std::tuple<T_REAL_EVENTS...>*
                                                                        ) const noexcept;
            //----------------------------------------------------------------------------------------------
            virtual
            const descriptor&       getDescriptor                       ( void 
                                                                        ) const noexcept
            {
                return descriptor_v<T_SYSTEM>;
            }
        };

        //----------------------------------------------------------------------------------------------
        // FUNCTIONS
        //----------------------------------------------------------------------------------------------
        template< typename      T_CUSTOM_SYSTEM
                , typename...   T_ARGS>
        constexpr xforceinline
        auto GetGlobalEventTuple( T_CUSTOM_SYSTEM& System, std::tuple<T_ARGS...>* ) noexcept
        {
            return typename T_CUSTOM_SYSTEM::global_real_events_t
            {
                &System.m_World.m_GraphDB.m_SystemGlobalEventDB.getOrCreate< xcore::types::decay_full_t<T_ARGS> >
                    (mecs::system::event::descriptor_v<xcore::types::decay_full_t<T_ARGS>>.m_Guid)
                ...
            };
        }

        //----------------------------------------------------------------------------------------------
        template< typename T_SYSTEM >
        constexpr inline
        custom_system<T_SYSTEM>::custom_system( typename T_SYSTEM::construct&& Settings ) noexcept
        : T_SYSTEM      { std::move(Settings) }
        , m_GlobalEvents{ GetGlobalEventTuple( *this, reinterpret_cast<typename custom_system::global_event_tuple_t*>(nullptr) ) }
        {
            //
            // Register with World events
            //
            if constexpr( &custom_system::msgGraphInit != &overrides::msgGraphInit )
            {
                user_system_t::m_World.m_GraphDB.m_Events.m_GraphInit.AddDelegate<&custom_system::msgGraphInit>(*this);
            }

            if constexpr( &custom_system::msgFrameStart != &overrides::msgFrameStart )
            {
                user_system_t::m_World.m_GraphDB.m_Events.m_FrameStart.AddDelegate<&custom_system::msgFrameStart>(*this);
            }

            if constexpr( &custom_system::msgFrameDone != &overrides::msgFrameDone )
            {
                user_system_t::m_World.m_GraphDB.m_Events.m_FrameDone.AddDelegate<&custom_system::msgFrameDone>(*this);
            }
        }

        //----------------------------------------------------------------------------------------------
        template< typename T_SYSTEM >
        inline
        void custom_system<T_SYSTEM>::qt_onRun( void ) noexcept
        {
            if constexpr (std::is_same_v<T_SYSTEM, mecs::system::instance>)
            {
                xassert(false);
            }
            else
            {
                XCORE_PERF_ZONE_SCOPED_N(user_system_t::type_name_v.m_Str)
                if constexpr ( &user_system_t::msgUpdate != &system::overrides::msgUpdate )
                {
                    user_system_t::msgUpdate();
                }
                else
                {
                    user_system_t::m_World.m_ArchetypeDB.template DoQuery< user_system_t, typename user_system_t::query_t >(user_system_t::m_Query);
                    user_system_t::ForEach( user_system_t::m_Query, *this, user_system_t::entities_per_job_v );
                }
            }
        }

        //----------------------------------------------------------------------------------------------
        template< typename T_SYSTEM >
        inline
        void custom_system<T_SYSTEM>::msgSyncPointDone( mecs::sync_point::instance& Syncpoint ) noexcept
        {
            // Call the user function if he overwrote it
            if constexpr ( &user_system_t::msgSyncPointDone != &system::overrides::msgSyncPointDone )
            {
                user_system_t::msgSyncPointDone(Syncpoint);
            }

            //
            // unlock and sync groups from the cache
            //
            xassert_block_basic()
            {
                for (const auto& MyData : user_system_t::m_Cache.m_Data )
                {
                    for( int i=0, end = MyData.m_nFunctions.load(std::memory_order_relaxed); i < end; ++i )
                    {
                        if ( MyData.m_WhichFunction[i] )
                            mecs::archetype::details::safety::UnlockQueryComponents(*this, MyData.m_PerFunction[i].m_ResultEntry);
                    }
                }
            }

            //
            // Look to see if we have that group in our query
            // If we don't then we must call the memory barrier for that archetype
            //
            {
                for( int i=0, end = user_system_t::m_Cache.m_nLines.load(std::memory_order_relaxed); i<end; ++i )
                {
                    /*
                    bool        bFound      = false;

                    for (const auto& Q : user_system_t::m_Query.m_lResults)
                    {
                        if( pArchetype == Q.m_pArchetype )
                        {
                            bFound = true;
                            break;
                        }
                    }

                    if( bFound == false ) pArchetype->MemoryBarrierSync(Syncpoint);
                    */

                    user_system_t::m_Cache.m_Lines[i].load(std::memory_order_relaxed)->MemoryBarrierSync(Syncpoint);
                }
                user_system_t::m_Cache.clear();
            }

            //
            // Let the archetypes that we used do their memory barriers
            //
            for( auto& R : user_system_t::m_Query.m_lResults )
            {
                R.m_pArchetype->MemoryBarrierSync( Syncpoint );
            }

            user_system_t::m_Query.m_lResults.clear();
        }

        //----------------------------------------------------------------------------------------------
        template< typename T_SYSTEM > 
        template< typename...T_REAL_EVENTS >
        xforceinline constexpr
        void* 
        custom_system<T_SYSTEM>::getExclusiveRealEventHelper
        ( const system::event::type_guid    Guid
        , std::tuple<T_REAL_EVENTS...>*
        ) const noexcept
        {
            const void* pRet = nullptr;
            ( [&]( auto& pRet ) constexpr noexcept
            {
                if( exclusive_events_descriptors_v[ xcore::types::tuple_t2i_v<T_REAL_EVENTS, exclusive_real_events_t> ]->m_Guid == Guid )
                {
                    pRet = static_cast<const void*>(&std::get<T_REAL_EVENTS>(m_ExclusiveEvents));
                    return false;
                }
                return true;
                    
            }(pRet) &&  ... );

            return const_cast<void*>(pRet);
        }

        //----------------------------------------------------------------------------------------------
        template< typename T_SYSTEM >
        xforceinline
        void* custom_system<T_SYSTEM>::_getExclusiveRealEvent( const system::event::type_guid Guid ) const noexcept
        {
            return getExclusiveRealEventHelper(Guid, static_cast<exclusive_real_events_t*>(nullptr) );
        }

        //----------------------------------------------------------------------------------------------
        template< typename T_CALLBACK >
        struct process_resuts_params
        {
            using call_back_t = T_CALLBACK;

            mecs::system::instance&                 m_System;
            xcore::scheduler::channel&              m_Channel;
            const T_CALLBACK&&                      m_Callback;
            const int                               m_nEntriesPerJob;
        };

        //----------------------------------------------------------------------------------------------
        struct params_per_archetype
        {
            mecs::archetype::query::result_entry*   m_pResult;
            std::span<std::uint8_t>                 m_DelegateSpan;
        };

        namespace details
        {
            template< typename T_COMPONENT >
            static constexpr bool is_share_mutable_v = mecs::component::descriptor_v<T_COMPONENT>.m_Type == mecs::component::type::SHARE
                                                    && false == std::is_same_v<T_COMPONENT, const T_COMPONENT >;
            template< typename...T_ARGS >
            struct mutable_share_tuple;

            template< typename...T_ARGS >
            struct mutable_share_tuple< std::tuple<T_ARGS...> >
            {
                using type = xcore::types::tuple_sort_t
                <
                    mecs::component::smaller_guid
                    , xcore::types::tuple_cat_t
                    < 
                        std::conditional_t
                        <
                            is_share_mutable_v<std::remove_reference_t<std::remove_pointer_t<T_ARGS>>>
                        ,   std::tuple<xcore::types::decay_full_t<T_ARGS>>
                        ,   std::tuple<>
                        > ...
                    >
                >;
            };
        }

        template< typename T_TUPLE >
        using mutable_share_tuple_t = typename details::mutable_share_tuple< T_TUPLE >::type;

        template< typename...T_ARGS >
        constexpr xforceinline
        auto ProcessInitArray   (   std::tuple<T_ARGS...>*
                                ,   int                                   iStart
                                ,   int                                   Index
                                ,   const archetype::query::result_entry& R ) noexcept
        {
            using       func_tuple  = std::tuple<T_ARGS...>;
            const auto  Decide      = [&]( int iArg, auto Arg ) constexpr noexcept
            {
                using       t = xcore::types::decay_full_t<decltype(Arg)>;
                const auto& I = R.m_lFunctionToArchetype[iArg];

                if( I.m_Index == mecs::archetype::query::result_entry::invalid_index) return reinterpret_cast<std::byte*>(nullptr);

                if( I.m_isShared ) return reinterpret_cast<std::byte*>(&R.m_pArchetype->m_MainPool.getComponentByIndex<t>( Index, I.m_Index ));

                auto& Specialized = R.m_pArchetype->m_MainPool.getComponentByIndex<mecs::archetype::specialized_pool>(Index, 0);
                return reinterpret_cast<std::byte*>(&Specialized.m_EntityPool.getComponentByIndex<t, false>( iStart, I.m_Index ));
            };
            
            return std::array<std::byte*, sizeof...(T_ARGS)>
            {
                Decide( xcore::types::tuple_t2i_v<T_ARGS, func_tuple>, reinterpret_cast<xcore::types::decay_full_t<T_ARGS>*>(nullptr) ) ...
            };
        }

        template< typename T, typename...T_ARGS>
        constexpr xforceinline
        void ProcesssCall( std::tuple<T_ARGS...>*
                         , std::span<std::byte*>        Span
                         , T&                           Params
                         , int                          iStart
                         , const int                    iEnd ) noexcept
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
                            if constexpr (std::is_pointer_v<T_ARGS>) { if (p) p += mecs::component::descriptor_v<T_ARGS>.m_Size; }
                            else                                              p += mecs::component::descriptor_v<T_ARGS>.m_Size;
                        }

                        if constexpr (mecs::component::descriptor_v<T_ARGS>.m_Type == mecs::component::type::SINGLETON)
                        {
                            auto& UPtr = reinterpret_cast<std::unique_ptr<xcore::types::decay_full_t<T_ARGS>>&>(*pBackup);
                            if constexpr (std::is_pointer_v<T_ARGS>) return UPtr.get();
                            else                                     return *UPtr;
                        }
                        else
                        {
                            if constexpr (std::is_pointer_v<T_ARGS>) return reinterpret_cast<T_ARGS>(pBackup);
                            else                                     return reinterpret_cast<T_ARGS>(*pBackup);
                        }

                    }())...
                );
            } while( ++iStart != iEnd );
        }

        template<   typename    T
                ,   typename... T_SHARE_COMPS
                ,   typename... T_ARGS
        >
        constexpr xforceinline
        void ProcesssCallMutableShareComponents (  std::tuple<T_SHARE_COMPS...>*                        // Share components are order by their guids (less to more)
                                                ,  std::tuple<T_ARGS...>*                               // Arguments are order base on the function call
                                                ,  mecs::archetype::specialized_pool&   Pool
                                                ,  std::span<std::byte*>                Span
                                                ,  T&                                   Params
                                                ,  params_per_archetype&                Params2
                                                ,  component::entity*                   pEntityPool
                                                ,  int                                  iStart
                                                ,  const int                            iEnd ) noexcept
        {
            using func_tuple_decayed = std::tuple< xcore::types::decay_full_t<T_ARGS>... >;
            using func_tuple         = std::tuple<T_ARGS...>;
            using share_tuple        = std::tuple<T_SHARE_COMPS...>;

            xassert(iStart != iEnd);

            std::array<std::uint64_t, mecs::settings::max_data_components_per_entity>   ComponentCRC;
            std::array<std::byte*,    mecs::settings::max_data_components_per_entity>   ShareComponentData;
            share_tuple                                                                 ShareTuple;
            std::array<std::byte*,    sizeof...(T_SHARE_COMPS)>                         OriginalShareComponents;
            std::array<std::uint16_t, sizeof...(T_SHARE_COMPS)>                         ShareIndexArray;

            //
            // Create a Map between Function Parameters (Share Writable Component Only) To ShareComponentIndex in the pool
            //
            {
                const auto& Desc    = Pool.m_pArchetypeInstance->m_Descriptor.m_ShareDescriptorSpan;
                const int   end     = static_cast<int>(Desc.size());
                int         Index   = 0;
                const auto  FindPos = [&]( auto& i, auto& ComponentDesc ) constexpr 
                {
                    for (; i < end; ++i)
                    {
                        if ( ComponentDesc.m_Guid == Desc[i]->m_Guid )
                        {
                           return i++;
                        }
                    }
                    xassert(false);
                    return ~0;
                };

                ((ShareIndexArray[xcore::types::tuple_t2i_v<T_SHARE_COMPS, share_tuple>] = FindPos( Index, mecs::component::descriptor_v<T_SHARE_COMPS> )), ... );
            }

            // Back up all the original pointers
            ((OriginalShareComponents[ xcore::types::tuple_t2i_v<T_SHARE_COMPS, share_tuple> ] = Span[ xcore::types::tuple_t2i_v<T_SHARE_COMPS, func_tuple_decayed> ] ), ... );

            // Replace orinal share components with the new tuple memory (if we had a valid pointer)
            ((Span[xcore::types::tuple_t2i_v<T_SHARE_COMPS, func_tuple_decayed>] = Span[xcore::types::tuple_t2i_v<T_SHARE_COMPS, func_tuple_decayed>] 
                                                                                    ? reinterpret_cast<std::byte*>(&std::get<T_SHARE_COMPS>(ShareTuple))
                                                                                    : nullptr 
             ), ... );

            // Compute a partial CRC to detect changes
            const std::uint64_t MutableShareCRC = (( OriginalShareComponents[xcore::types::tuple_t2i_v<T_SHARE_COMPS, share_tuple>]
                                                        ? Pool.m_ShareComponentKeysSpan[ ShareIndexArray[xcore::types::tuple_t2i_v<T_SHARE_COMPS, share_tuple>] ]
                                                        : 0
                                                   ) + ... );

            //
            // Copy all keys and pointers
            //
            for( int i=0, end = static_cast<int>(Pool.m_pArchetypeInstance->m_Descriptor.m_ShareDescriptorSpan.size()); i<end; ++i )
            {
                ShareComponentData[i] = Pool.m_pArchetypeInstance->m_MainPool.getComponentByIndexRaw( Pool.m_MainPoolIndex, 1+i );
                ComponentCRC[i]       = Pool.m_ShareComponentKeysSpan[i];
            }

            // Override points of share components that we are going to replace
            ((Span[xcore::types::tuple_t2i_v<T_SHARE_COMPS, func_tuple_decayed>]
                ? ShareComponentData[ ShareIndexArray[xcore::types::tuple_t2i_v<T_SHARE_COMPS, share_tuple>] ] = Span[xcore::types::tuple_t2i_v<T_SHARE_COMPS, func_tuple_decayed>], nullptr
                : nullptr
             ), ... );


            //
            // Be ready to notify delegates
            //
            auto&               UpdateComponents = Params2.m_pResult->m_pArchetype->m_Events.m_UpdateComponent;
            const std::uint8_t  endDelegateSpan  = static_cast<std::uint8_t>(Params2.m_DelegateSpan.size());

            //
            // Call the system function
            //
            do
            {
                // Copy the share components that are about to get modifies into the tuple 
                ((OriginalShareComponents[xcore::types::tuple_t2i_v<T_SHARE_COMPS, share_tuple>]
                    ? std::get<T_SHARE_COMPS>(ShareTuple) = *reinterpret_cast<T_SHARE_COMPS*>(OriginalShareComponents[xcore::types::tuple_t2i_v<T_SHARE_COMPS, share_tuple>]), nullptr
                    : nullptr
                ), ... );

                // Call the function
                Params.m_Callback
                (
                    ([&]() constexpr noexcept -> T_ARGS
                    {
                        auto& p         = Span[xcore::types::tuple_t2i_v<T_ARGS, func_tuple>];
                        auto  pBackup   = p;
                        if constexpr ( mecs::component::descriptor_v<T_ARGS>.m_Type != mecs::component::type::SHARE )
                        {
                            if constexpr (std::is_pointer_v<T_ARGS>) { if (p) p += sizeof(xcore::types::decay_full_t<T_ARGS>); }
                            else                                              p += sizeof(xcore::types::decay_full_t<T_ARGS>);
                        }

                        if constexpr (mecs::component::descriptor_v<T_ARGS>.m_Type == mecs::component::type::SINGLETON)
                        {
                            auto& UPtr = reinterpret_cast<std::unique_ptr<xcore::types::decay_full_t<T_ARGS>>&>(*pBackup);
                            if constexpr (std::is_pointer_v<T_ARGS>) return UPtr.get();
                            else                                     return *UPtr;
                        }
                        else
                        {
                            if constexpr (std::is_pointer_v<T_ARGS>) return reinterpret_cast<T_ARGS>(pBackup);
                            else                                     return reinterpret_cast<T_ARGS>(*pBackup);
                        }
                    }())...
                );

                // Deal with delegates 
                for (std::uint8_t i = 0u; i != endDelegateSpan; ++i)
                    UpdateComponents.m_Event.Notify(Params2.m_DelegateSpan[i], *pEntityPool, Params.m_System);

                pEntityPool++;

                // Store partial CRCs we may need to use them
                // also compute the new partial CRC to detect changes
                std::uint64_t MutableShareNewCRC = 0;
                ((OriginalShareComponents[xcore::types::tuple_t2i_v<T_SHARE_COMPS, share_tuple>]
                    ? MutableShareNewCRC += (ComponentCRC[ShareIndexArray[xcore::types::tuple_t2i_v<T_SHARE_COMPS, share_tuple>] ] = mecs::component::descriptor_v<T_SHARE_COMPS>.m_fnGetKey(&std::get<T_SHARE_COMPS>(ShareTuple)))
                        , nullptr 
                    : nullptr
                  ), ... );
                    
                // If CRC has change we need to move the entity to a new pool
                if( MutableShareNewCRC != MutableShareCRC )
                {
                    const auto NewFullGuid = Pool.m_TypeGuid.m_Value - MutableShareCRC + MutableShareNewCRC;

                    Pool.m_pArchetypeInstance->moveEntityBetweenSpecializePools
                    (
                            Params.m_System
                        ,   Pool.m_MainPoolIndex
                        ,   iStart
                        ,   NewFullGuid
                        ,   ShareComponentData.data()
                        ,   ComponentCRC.data()
                    );
                }

            } while( ++iStart != iEnd );
        }

        template<   typename    T
                ,   typename... T_SHARE_COMPS
                ,   typename... T_ARGS
        >
        constexpr xforceinline
        void ProcesssCallMutableShareComponents (  std::tuple<T_SHARE_COMPS...>*                        // Share components are order by their guids (less to more)
                                                ,  std::tuple<T_ARGS...>*                               // Arguments are order base on the function call
                                                ,  mecs::archetype::specialized_pool&   Pool
                                                ,  std::span<std::byte*>                Span
                                                ,  T&                                   Params
                                                ,  int                                  iStart
                                                ,  const int                            iEnd ) noexcept
        {
            using func_tuple_decayed = std::tuple< xcore::types::decay_full_t<T_ARGS>... >;
            using func_tuple         = std::tuple<T_ARGS...>;
            using share_tuple        = std::tuple<T_SHARE_COMPS...>;

            xassert(iStart != iEnd);

            std::array<std::uint64_t, mecs::settings::max_data_components_per_entity>   ComponentCRC;
            std::array<std::byte*,    mecs::settings::max_data_components_per_entity>   ShareComponentData;
            share_tuple                                                                 ShareTuple;
            std::array<std::byte*,    sizeof...(T_SHARE_COMPS)>                         OriginalShareComponents;
            std::array<std::uint16_t, sizeof...(T_SHARE_COMPS)>                         ShareIndexArray;

            //
            // Create a Map between Function Parameters (Share Writable Component Only) To ShareComponentIndex in the pool
            //
            {
                const auto& Desc    = Pool.m_pArchetypeInstance->m_Descriptor.m_ShareDescriptorSpan;
                const int   end     = static_cast<int>(Desc.size());
                int         Index   = 0;
                const auto  FindPos = [&]( auto& i, auto& ComponentDesc ) constexpr 
                {
                    for (; i < end; ++i)
                    {
                        if ( ComponentDesc.m_Guid == Desc[i]->m_Guid )
                        {
                           return i++;
                        }
                    }
                    xassert(false);
                    return ~0;
                };

                ((ShareIndexArray[xcore::types::tuple_t2i_v<T_SHARE_COMPS, share_tuple>] = FindPos( Index, mecs::component::descriptor_v<T_SHARE_COMPS> )), ... );
            }

            // Back up all the original pointers
            ((OriginalShareComponents[ xcore::types::tuple_t2i_v<T_SHARE_COMPS, share_tuple> ] = Span[ xcore::types::tuple_t2i_v<T_SHARE_COMPS, func_tuple_decayed> ] ), ... );

            // Replace orinal share components with the new tuple memory (if we had a valid pointer)
            ((Span[xcore::types::tuple_t2i_v<T_SHARE_COMPS, func_tuple_decayed>] = Span[xcore::types::tuple_t2i_v<T_SHARE_COMPS, func_tuple_decayed>] 
                                                                                    ? reinterpret_cast<std::byte*>(&std::get<T_SHARE_COMPS>(ShareTuple))
                                                                                    : nullptr 
             ), ... );

            // Compute a partial CRC to detect changes
            const std::uint64_t MutableShareCRC = (( OriginalShareComponents[xcore::types::tuple_t2i_v<T_SHARE_COMPS, share_tuple>]
                                                        ? Pool.m_ShareComponentKeysSpan[ ShareIndexArray[xcore::types::tuple_t2i_v<T_SHARE_COMPS, share_tuple>] ]
                                                        : 0
                                                   ) + ... );

            //
            // Copy all keys and pointers
            //
            for( int i=0, end = static_cast<int>(Pool.m_pArchetypeInstance->m_Descriptor.m_ShareDescriptorSpan.size()); i<end; ++i )
            {
                ShareComponentData[i] = Pool.m_pArchetypeInstance->m_MainPool.getComponentByIndexRaw( Pool.m_MainPoolIndex, 1+i );
                ComponentCRC[i]       = Pool.m_ShareComponentKeysSpan[i];
            }

            // Override points of share components that we are going to replace
            ((Span[xcore::types::tuple_t2i_v<T_SHARE_COMPS, func_tuple_decayed>]
                ? ShareComponentData[ ShareIndexArray[xcore::types::tuple_t2i_v<T_SHARE_COMPS, share_tuple>] ] = Span[xcore::types::tuple_t2i_v<T_SHARE_COMPS, func_tuple_decayed>], nullptr
                : nullptr
             ), ... );

            //
            // Call the system function
            //
            do
            {
                // Copy the share components that are about to get modifies into the tuple 
                ((OriginalShareComponents[xcore::types::tuple_t2i_v<T_SHARE_COMPS, share_tuple>]
                    ? std::get<T_SHARE_COMPS>(ShareTuple) = *reinterpret_cast<T_SHARE_COMPS*>(OriginalShareComponents[xcore::types::tuple_t2i_v<T_SHARE_COMPS, share_tuple>]), nullptr
                    : nullptr
                ), ... );

                // Call the function
                Params.m_Callback
                (
                    ([&]() constexpr noexcept -> T_ARGS
                    {
                        auto& p         = Span[xcore::types::tuple_t2i_v<T_ARGS, func_tuple>];
                        auto  pBackup   = p;
                        if constexpr ( mecs::component::descriptor_v<T_ARGS>.m_Type != mecs::component::type::SHARE )
                        {
                            if constexpr (std::is_pointer_v<T_ARGS>) { if (p) p += sizeof(xcore::types::decay_full_t<T_ARGS>); }
                            else                                              p += sizeof(xcore::types::decay_full_t<T_ARGS>);
                        }

                        if constexpr (mecs::component::descriptor_v<T_ARGS>.m_Type == mecs::component::type::SINGLETON)
                        {
                            auto& UPtr = reinterpret_cast<std::unique_ptr<xcore::types::decay_full_t<T_ARGS>>&>(*pBackup);
                            if constexpr (std::is_pointer_v<T_ARGS>) return UPtr.get();
                            else                                     return *UPtr;
                        }
                        else
                        {
                            if constexpr (std::is_pointer_v<T_ARGS>) return reinterpret_cast<T_ARGS>(pBackup);
                            else                                     return reinterpret_cast<T_ARGS>(*pBackup);
                        }
                    }())...
                );

                // Store partial CRCs we may need to use them
                // also compute the new partial CRC to detect changes
                std::uint64_t MutableShareNewCRC = 0;
                ((OriginalShareComponents[xcore::types::tuple_t2i_v<T_SHARE_COMPS, share_tuple>]
                    ? MutableShareNewCRC += (ComponentCRC[ShareIndexArray[xcore::types::tuple_t2i_v<T_SHARE_COMPS, share_tuple>] ] = mecs::component::descriptor_v<T_SHARE_COMPS>.m_fnGetKey(&std::get<T_SHARE_COMPS>(ShareTuple)))
                        , nullptr 
                    : nullptr
                  ), ... );
                    
                // If CRC has change we need to move the entity to a new pool
                if( MutableShareNewCRC != MutableShareCRC )
                {
                    const auto NewFullGuid = Pool.m_TypeGuid.m_Value - MutableShareCRC + MutableShareNewCRC;

                    Pool.m_pArchetypeInstance->moveEntityBetweenSpecializePools
                    (
                            Params.m_System
                        ,   Pool.m_MainPoolIndex
                        ,   iStart
                        ,   NewFullGuid
                        ,   ShareComponentData.data()
                        ,   ComponentCRC.data()
                    );
                }

            } while( ++iStart != iEnd );
        }


        template< typename T, typename...T_ARGS> constexpr xforceinline
        void ProcesssCall( std::tuple<T_ARGS...>*
                         , std::span<std::byte*>        Span
                         , T&                           Params
                         , params_per_archetype&        Params2
                         , component::entity*           pEntityPool
                         , int                          iStart
                         , const int                    iEnd ) noexcept
        {
            using func_tuple    = std::tuple<T_ARGS...>;

            xassert(iStart != iEnd);

            auto&               UpdateComponents = Params2.m_pResult->m_pArchetype->m_Events.m_UpdateComponent;
            const std::uint8_t  end              = static_cast<std::uint8_t>(Params2.m_DelegateSpan.size());

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

                        if constexpr (mecs::component::descriptor_v<T_ARGS>.m_Type == mecs::component::type::SINGLETON)
                        {
                            auto& UPtr = reinterpret_cast<std::unique_ptr<xcore::types::decay_full_t<T_ARGS>>&>(*pBackup);
                            if constexpr (std::is_pointer_v<T_ARGS>) return UPtr.get();
                            else                                     return *UPtr;
                        }
                        else
                        {
                            if constexpr (std::is_pointer_v<T_ARGS>) return reinterpret_cast<T_ARGS>(pBackup);
                            else                                     return reinterpret_cast<T_ARGS>(*pBackup);
                        }
                    }())...
                );

                // Deal with delegates 
                for (std::uint8_t i = 0u; i != end; ++i)
                    UpdateComponents.m_Event.Notify( Params2.m_DelegateSpan[i], *pEntityPool, Params.m_System );

                pEntityPool++;

            } while( ++iStart != iEnd );
        }

        template< typename T, typename...T_ARGS> constexpr xforceinline
        void ProcesssCall( std::tuple<T_ARGS...>*
                         , std::span<std::byte*>        Span
                         , component::entity*           pEntityPool
                         , T&                           Params
                         , params_per_archetype&        Params2
                         , const std::uint64_t          MutableShareCRC
                         , int                          iStart
                         , const int                    iEnd ) noexcept
        {
            using func_tuple    = std::tuple<T_ARGS...>;

            xassert(iStart != iEnd);

            auto&               UpdateComponents = Params2.m_pResult->m_pArchetype->m_Events.m_UpdateComponent;
            const std::uint8_t  end              = static_cast<std::uint8_t>(Params2.m_DelegateSpan.size());

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

                        if constexpr (mecs::component::descriptor_v<T_ARGS>.m_Type == mecs::component::type::SINGLETON)
                        {
                            auto& UPtr = reinterpret_cast<std::unique_ptr<xcore::types::decay_full_t<T_ARGS>>&>(*pBackup);
                            if constexpr (std::is_pointer_v<T_ARGS>) return UPtr.get();
                            else                                     return *UPtr;
                        }
                        else
                        {
                            if constexpr (std::is_pointer_v<T_ARGS>) return reinterpret_cast<T_ARGS>(pBackup);
                            else                                     return reinterpret_cast<T_ARGS>(*pBackup);
                        }
                    }())...
                );

                const std::uint64_t MutableShareNewCRC = 
                (
                    ([&]() constexpr noexcept 
                    {
                        if constexpr (mecs::component::descriptor_v<T_ARGS>.m_Type != mecs::component::type::SHARE && std::is_same_v<T_ARGS, std::remove_const_t<T_ARGS>> )
                        {
                            auto p = Span[xcore::types::tuple_t2i_v<T_ARGS, func_tuple>];
                            if constexpr (std::is_pointer_v<T_ARGS>)
                            {
                                if (p) 
                                {
                                    p -= sizeof(xcore::types::decay_full_t<T_ARGS>);
                                    return mecs::component::descriptor_v<T_ARGS>.m_fnGetKey(p);
                                }
                                else
                                {
                                    return 0ull;
                                }
                            }
                            else
                            {
                                p -= sizeof(xcore::types::decay_full_t<T_ARGS>);
                                return mecs::component::descriptor_v<T_ARGS>.m_fnGetKey(p);
                            }
                        }
                        else
                        {
                            return 0;
                        }
                    }())
                +   ...
                );

                if( MutableShareNewCRC != MutableShareCRC )
                {
                    // TODO: Move to new specialized pool
                }

                // Deal with delegates 
                for (std::uint8_t i = 0u; i != end; ++i)
                    UpdateComponents.m_Event.Notify( Params2.m_DelegateSpan[i], *pEntityPool, Params.m_System );

                pEntityPool++;

            } while( ++iStart != iEnd );
        }
    }

    //----------------------------------------------------------------------------
    // SYSTEM:: INSTANCE
    //----------------------------------------------------------------------------

    //----------------------------------------------------------------------------

    xforceinline
    instance::instance( const construct&& Settings) noexcept
        : overrides{ Settings.m_JobDefinition | xcore::scheduler::triggers::DONT_CLEAN_COUNT }
        , m_World{ Settings.m_World }
        , m_Guid{ Settings.m_Guid }
    { setupName(Settings.m_Name); }

    //----------------------------------------------------------------------------

    template< typename T_CALLBACK >
    constexpr xforceinline
    void instance::ForEach(mecs::archetype::query::instance& QueryI, T_CALLBACK&& Functor, int nEntitiesPerJob ) noexcept
    {
        using func_args_tuple = typename xcore::function::traits<T_CALLBACK>::args_tuple;
        static constexpr auto is_writing_v = details::tuple_type_apply
        (
            static_cast<func_args_tuple*>(nullptr)
            , []( auto... x ) constexpr
            {
                return ((std::is_same_v< std::tuple_element<0, decltype(*x)>, const std::tuple_element<0, decltype(*x)> > == false) || ...);
            }
        );

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
#ifdef _XCORE_PROFILE
        xcore::scheduler::channel Channel{ getName() };
#else
        xcore::scheduler::channel Channel{ xconst_universal_str("") };
#endif
        using                           params_t = details::process_resuts_params<T_CALLBACK>;
        params_t                        Params
        {
              *this
            , Channel
            , std::forward<T_CALLBACK>(Functor)
            , nEntitiesPerJob
        };

        int                                             DelegateStackIndex = 0;
        std::array<std::array<std::uint8_t, 16>, 128 >  DelegateStack;
        std::array<details::params_per_archetype, 16 >  ArchetypeParams;
        int                                             iArchetypeParam = 0;

        for( auto& R : QueryI.m_lResults )
        {
            auto& MainPool = R.m_pArchetype->m_MainPool;
            xassert(MainPool.size());

            // If we need to call events per each entity update then lets precompute as much as we can
            auto& ArchetypeParam = ArchetypeParams[iArchetypeParam++];
            ArchetypeParam.m_pResult    = &R;

            if constexpr (is_writing_v)
            {
                auto& UpdateComponent = R.m_pArchetype->m_Events.m_UpdateComponent;
                if(UpdateComponent.m_Event.hasSubscribers() )
                {
                    const static tools::bits Bits = details::ComputeUpdateBits( static_cast<func_args_tuple*>(nullptr) );

                    const auto& UpdateComponentBits = UpdateComponent.m_Bits;
                    auto& StackEntry = DelegateStack[ DelegateStackIndex++ ];
                    int   nDelegates = 0;

                    for (int i = 0, end = static_cast<int>(UpdateComponentBits.size()); i != end; ++i)
                    {
                        if( UpdateComponentBits[i].isMatchingBits(Bits) )
                            StackEntry[nDelegates++] = static_cast<std::uint8_t>(i);
                    }

                    if(nDelegates) ArchetypeParam.m_DelegateSpan = std::span<std::uint8_t>{ StackEntry.data(), static_cast<std::size_t>(nDelegates) };
                }
            }

            // Process the results
            for (int end = static_cast<int>(MainPool.size()), i = 0; i < end; ++i)
            {
                _ProcessResult(Params, ArchetypeParam, i );
            }
        }

        Channel.join();

        //
        // unlock all the groups that we need to work with so that no one tries to add/remove entities
        //
        mecs::archetype::details::safety::UnlockQueryComponents( *this, QueryI );
    }

    //----------------------------------------------------------------------------

    template< typename...T_SHARE_COMPONENTS >
    xforceinline
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
        auto& NewArchetype = m_World.m_ArchetypeDB.getOrCreateArchetypeBy(
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
    template< typename T_PARAMS, typename T_PARAMS2 > constexpr xforceinline
    void instance::_ProcessResult( T_PARAMS& Params, T_PARAMS2& Params2, const int Index ) noexcept
    {
        using               function_arg_tuple  = typename xcore::function::traits<T_PARAMS::call_back_t>::args_tuple;
        auto&               SpecializedPool     = Params2.m_pResult->m_pArchetype->m_MainPool.getComponentByIndex<mecs::archetype::specialized_pool>(Index, 0);
        using               mutable_share_tuple = details::mutable_share_tuple_t<function_arg_tuple>;

        // If we have to update share components we have to do things more carefully 
        if constexpr ( std::tuple_size_v<mutable_share_tuple> > 0 )
        {
            for (int end = static_cast<int>(SpecializedPool.m_EntityPool.size()), i = 0; i < end; )
            {
                const int MyEnd = std::min<int>(i + Params.m_nEntriesPerJob, end);
                Params.m_Channel.SubmitJob(
                [
                    &Params
                ,   &Params2
                ,   iStart  = i
                ,   iEnd    = MyEnd
                ,   Index
                ] () constexpr noexcept
                {
                    auto        Pointers                = details::ProcessInitArray( reinterpret_cast<function_arg_tuple*>(nullptr), iStart, Index, *Params2.m_pResult );
                    auto&       SpecializedPool         = Params2.m_pResult->m_pArchetype->m_MainPool.getComponentByIndex<mecs::archetype::specialized_pool>(Index, 0);

                    if (Params2.m_DelegateSpan.size())
                    {
                        mecs::component::entity* pEntityPool     = &SpecializedPool.m_EntityPool.getComponentByIndex<mecs::component::entity>(iStart, 0);
                        details::ProcesssCallMutableShareComponents
                        (
                              reinterpret_cast<mutable_share_tuple*>(nullptr)
                            , reinterpret_cast<function_arg_tuple*>(nullptr)
                            , SpecializedPool
                            , Pointers
                            , Params
                            , Params2
                            , pEntityPool
                            , iStart
                            , iEnd
                        );
                    }
                    else
                    {
                        details::ProcesssCallMutableShareComponents
                        (
                              reinterpret_cast<mutable_share_tuple*>(nullptr)
                            , reinterpret_cast<function_arg_tuple*>(nullptr)
                            , SpecializedPool
                            , Pointers
                            , Params
                            , iStart
                            , iEnd
                        );
                    }
                });
                i = MyEnd;
            }
        }
        else
        {
            for( int end=static_cast<int>(SpecializedPool.m_EntityPool.size()), i=0; i<end; )
            {
                const int MyEnd = std::min<int>(i+Params.m_nEntriesPerJob, end);

                Params.m_Channel.SubmitJob(
                [
                    &Params
                ,   &Params2
                ,   iStart  = i
                ,   iEnd    = MyEnd
                ,   Index
                ] () constexpr noexcept
                {
                    auto  Pointers    = details::ProcessInitArray(reinterpret_cast<function_arg_tuple*>(nullptr), iStart, Index, *Params2.m_pResult );

                    if( Params2.m_DelegateSpan.size() )
                    {
                        auto&                    SpecializedPool = Params2.m_pResult->m_pArchetype->m_MainPool.getComponentByIndex<mecs::archetype::specialized_pool>(Index, 0);
                        mecs::component::entity* pEntityPool     = &SpecializedPool.m_EntityPool.getComponentByIndex<mecs::component::entity>( iStart, 0 );

                        details::ProcesssCall
                        (
                            reinterpret_cast<function_arg_tuple*>(nullptr)
                        ,   Pointers
                        ,   Params
                        ,   Params2
                        ,   pEntityPool
                        ,   iStart
                        ,   iEnd
                        );
                    }
                    else
                    {
                        details::ProcesssCall
                        (
                            reinterpret_cast<function_arg_tuple*>(nullptr)
                         ,  Pointers
                         ,  Params
                         ,  iStart
                         ,  iEnd
                        );
                    }
                });
                i = MyEnd;
            }
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

    //----------------------------------------------------------------------------------------------------
    namespace details
    {
        template< typename T_CALLBACK, typename...T_ARGS >
        xforceinline constexpr
        void get_component_call(std::tuple<T_ARGS...>* , T_CALLBACK&& Callback, std::span<std::byte*> Pointers ) noexcept
        {
            using func_tuple = std::tuple<T_ARGS...>;
            Callback
            (
                ([&]() constexpr noexcept -> T_ARGS
                    {
                        auto& p = Pointers[xcore::types::tuple_t2i_v<T_ARGS, func_tuple>];

                        if constexpr ( mecs::component::descriptor_v<T_ARGS>.m_Type == mecs::component::type::SINGLETON )
                        {
                            auto& UPtr = reinterpret_cast<std::unique_ptr<xcore::types::decay_full_t<T_ARGS>>&>(*p);

                            if constexpr (std::is_pointer_v<T_ARGS>) return UPtr.get();
                            else                                     return *UPtr;
                        }
                        else
                        {
                            if constexpr (std::is_pointer_v<T_ARGS>) return reinterpret_cast<T_ARGS>(p);
                            else                                     return reinterpret_cast<T_ARGS>(*p);
                        }
                    }())...
            );
        }
    }

    //----------------------------------------------------------------------------------------------
    template< bool      T_RELAX_V
            , bool      T_ALREADY_LOCKED_V
            , typename  T_CALLBACK >
    constexpr xforceinline
    void instance::_getEntityComponentsRelax( const mecs::component::entity::reference& Reference, T_CALLBACK&& Function ) noexcept
    {
       // XCORE_PERF_ZONE_SCOPED_N("_getEntityComponentsRelax")
        using                   func_tuple              = typename xcore::function::traits<T_CALLBACK>::args_tuple;
        static constexpr auto   func_descriptors        = mecs::archetype::query::details::get_arrays< func_tuple >::value;
        using                   func_sorted_tuple       = xcore::types::tuple_sort_t< mecs::component::smaller_guid, func_tuple >;
        static constexpr auto   func_sorted_descriptors = mecs::archetype::query::details::get_arrays< func_sorted_tuple >::value;
        static constexpr auto   func_remap_from_sort    = mecs::archetype::query::details::remap_arrays< func_tuple, func_sorted_tuple >::value;
        static constexpr auto   is_writing_v            = details::tuple_type_apply
        (
            static_cast<func_tuple*>(nullptr)
            , [](auto... x) constexpr noexcept
            {
                return ((std::is_same_v< std::tuple_element<0, decltype(*x)>, const std::tuple_element<0, decltype(*x)> > == false) || ...);
            }
        );

        auto    Call        = [&]( const mecs::system::details::cache::per_function& Line ) constexpr noexcept
        {
            auto Pointers = details::ProcessInitArray(   reinterpret_cast<func_tuple*>(nullptr)
                                                    ,   Reference.m_Index
                                                    ,   Reference.m_pPool->m_MainPoolIndex
                                                    ,   Line.m_ResultEntry );

            details::get_component_call( static_cast<func_tuple*>(nullptr), std::forward<T_CALLBACK>(Function), Pointers );

            if constexpr ( is_writing_v )
            {
                if( Line.m_nDelegatesIndices )
                {
                    int i = 0;
                    auto& Event  = Line.m_ResultEntry.m_pArchetype->m_Events.m_UpdateComponent.m_Event;
                    auto& Entity = Reference.m_pPool->m_EntityPool.getComponentByIndex<mecs::component::entity>(Reference.m_Index,0); 
                    do
                    {
                        Event.Notify( Line.m_UpdateDelegateIndex[i], const_cast<mecs::component::entity&>(Entity), *this );
                    }
                    while( i != Line.m_nDelegatesIndices );
                }
            }
        };

        //
        // Search the entry from the cache
        //
        auto& Archetype  = *Reference.m_pPool->m_pArchetypeInstance;
        auto  GetFunctor = [&]( const details::cache::per_function& PerFunction ) constexpr noexcept
        {
           // XCORE_PERF_ZONE_SCOPED_N("GetFunctor")
            if constexpr (T_ALREADY_LOCKED_V)
            {
                Call(PerFunction);
            }
            else if constexpr (T_RELAX_V)
            {
                const auto& Map = m_World.m_ArchetypeDB.m_EntityMap;
                Map.find(Map.getKeyFromValue(Reference), [&](auto&) constexpr noexcept
                {
                    Call(PerFunction);
                });
            }
            else
            {
                auto& Map = m_World.m_ArchetypeDB.m_EntityMap;
                Map.find(Map.getKeyFromValue(Reference), [&](auto&) constexpr noexcept
                {
                    Call(PerFunction);
                });
            }
        };

        static constexpr auto function_guid_v = details::getFunctionGUID(static_cast<typename xcore::function::traits<T_CALLBACK>::args_tuple*>(nullptr));
        m_Cache.getOrCreateCache
        (
            function_guid_v
        ,   Archetype
        ,   GetFunctor
        ,   [&]( details::cache::per_function& PerFunction )
        {
           // XCORE_PERF_ZONE_SCOPED_N("getOrCreateCache::Create")

            auto& Entry = PerFunction.m_ResultEntry;
            Entry.m_pArchetype                    = &Archetype;
            Entry.m_nParameters                   = static_cast<std::uint8_t>(std::tuple_size_v<func_tuple>);
            Entry.m_FunctionDescriptors           = func_descriptors;

            PerFunction.m_nDelegatesIndices       = 0;
            if constexpr (is_writing_v)
            {
                auto& UpdateComponent = Archetype.m_Events.m_UpdateComponent;
                if( UpdateComponent.m_Event.hasSubscribers() )
                {
                    const static tools::bits Bits = details::ComputeUpdateBits(static_cast<func_tuple*>(nullptr));
                    const auto& UpdateComponentBits = UpdateComponent.m_Bits;

                    for( int i = 0, end = static_cast<int>(UpdateComponentBits.size()); i != end; ++i )
                    {
                        if( UpdateComponentBits[i].isMatchingBits(Bits) )
                            PerFunction.m_UpdateDelegateIndex[PerFunction.m_nDelegatesIndices++] = static_cast<std::uint8_t>(i);
                    }
                }
            }

            // Build required details to call the function easily 
            mecs::archetype::data_base::DetailsDoQuery< func_sorted_descriptors, func_remap_from_sort >(Entry);

            // Lets lock the archetype to make sure we are doing ok
            mecs::archetype::details::safety::LockQueryComponents( *this, PerFunction.m_ResultEntry );

            // Call the function
            GetFunctor(PerFunction);
        });

        /*
        xassert(Reference.m_pPool->m_pArchetypeInstance);
        auto& Archetype = *Reference.m_pPool->m_pArchetypeInstance;
    TRY_AGAIN_:
        {
            xcore::lock::scope Lk(std::as_const(m_Cache.m_Lines));
            auto& Lines     = m_Cache.m_Lines.get();

            for (const auto& E : Lines)
            {
                if (&Archetype == E.m_ResultEntry.m_pArchetype)
                {
                    if constexpr (T_ALREADY_LOCKED_V)
                    {
                        Call(E);
                    }
                    else
                    {
                        auto& Map = m_World.m_ArchetypeDB.m_EntityMap;
                        Map.find(Map.getKeyFromValue(Reference), [&](auto&) constexpr noexcept
                        {
                            Call(E);
                        });
                    }

                    // All done
                    return;
                }
            }
        }

        //
        // Create a new cache entry
        //
        {
            xcore::lock::scope Lk(m_Cache.m_Lines);
            auto& Lines        = m_Cache.m_Lines.get();
            auto& CacheLine    = Lines.emplace_back();
            auto& Entry        = CacheLine.m_ResultEntry;

            Entry.m_pArchetype                          = &Archetype;
            Entry.m_nParameters                         = static_cast<std::uint8_t>(std::tuple_size_v<func_tuple>);
            Entry.m_FunctionDescriptors                 = func_descriptors;

            CacheLine.m_nDelegatesIndices   = 0;
            // CacheLine.m_pArchetype          = &Archetype;

            if constexpr (is_writing_v)
            {
                auto& UpdateComponent = Archetype.m_Events.m_UpdateComponent;
                if( UpdateComponent.m_Event.hasSubscribers() )
                {
                    const static tools::bits Bits = details::ComputeUpdateBits(static_cast<func_tuple*>(nullptr));
                    const auto& UpdateComponentBits = UpdateComponent.m_Bits;

                    for( int i = 0, end = static_cast<int>(UpdateComponentBits.size()); i != end; ++i )
                    {
                        if( UpdateComponentBits[i].isMatchingBits(Bits) )
                            CacheLine.m_UpdateDelegateIndex[CacheLine.m_nDelegatesIndices++] = static_cast<std::uint8_t>(i);
                    }
                }
            }

            // Build required details to call the function easily 
            mecs::archetype::data_base::DetailsDoQuery< func_sorted_descriptors, func_remap_from_sort >(Entry);

            // Lets lock the archetype to make sure we are doing ok
            mecs::archetype::details::safety::LockQueryComponents( *this, CacheLine.m_ResultEntry );
        }

        goto TRY_AGAIN_;
        */
    }


    //---------------------------------------------------------------------------------
    template< typename T_CALLBACK >
    constexpr xforceinline
    void instance::getEntityComponents( const mecs::component::entity& Entity, T_CALLBACK&& Function ) noexcept
    {
        xassert(Entity.isZombie() == false);
        _getEntityComponentsRelax<false,false>(Entity.getReference(), std::forward<T_CALLBACK&&>(Function) );
    }

    //---------------------------------------------------------------------------------
    template< typename T_CALLBACK >
    constexpr xforceinline
    bool instance::findEntityComponents
    (   mecs::component::entity::guid     gEntity
    ,   T_CALLBACK&&                      Function
    ) noexcept
    {
        return m_World.m_ArchetypeDB.m_EntityMap.find( gEntity, [&]( mecs::component::entity::reference& Reference ) constexpr noexcept
        {
            _getEntityComponentsRelax<false, true>( Reference, std::forward<T_CALLBACK&&>(Function) );
        });
    }

    //---------------------------------------------------------------------------------
    template< typename T_CALLBACK >
    constexpr xforceinline
    bool
    instance::findEntityComponentsRelax
    (   mecs::component::entity::guid     gEntity
    ,   T_CALLBACK&&                      Function
    ) const noexcept
    {
        return std::as_const(m_World.m_ArchetypeDB.m_EntityMap).find( gEntity, [&]( const mecs::component::entity::reference& Reference ) constexpr noexcept 
        {
            _getEntityComponentsRelax<true, true>( Reference, std::forward<T_CALLBACK&&>(Function) );
        });
    }

    //---------------------------------------------------------------------------------
    template< typename  T_FUNCTION_TYPE
            , typename  T_COMPONENT_TUPLE >
    constexpr xforceinline
    mecs::archetype::query::instance& instance::DoQuery
    ( query& Query
    ) const noexcept
    {
        m_World.m_ArchetypeDB.template DoQuery< T_FUNCTION_TYPE, T_COMPONENT_TUPLE >(Query);
        return Query;
    }

    //---------------------------------------------------------------------------------
    template< typename... T_COMPONENTS >
    constexpr xforceinline
    mecs::archetype::instance& instance::getOrCreateArchetype( void ) const noexcept
    {
        return m_World.m_ArchetypeDB.getOrCreateArchitype<T_COMPONENTS ... >();
    }

    //---------------------------------------------------------------------------------
    template< bool          T_RELAX_V
            , typename      T_GET_CALLBACK
            , typename      T_CREATE_CALLBACK
            >
    constexpr xforceinline
    void instance::getOrCreateEntityRelax( mecs::component::entity::guid    gEntity
                                    , mecs::archetype::specialized_pool&    SpecialiedPool
                                    , T_GET_CALLBACK&&                      GetCallback
                                    , T_CREATE_CALLBACK&&                   CreateCallback
                                    ) noexcept
    {

        auto& EntityMap     = m_World.m_ArchetypeDB.m_EntityMap;

        EntityMap.getOrCreate<T_RELAX_V>( gEntity
        , [&](const entity::reference& Reference ) constexpr noexcept
        {
            _getEntityComponentsRelax<T_RELAX_V, true>( Reference, GetCallback );
        }
        , [&]( entity::reference& Reference ) constexpr noexcept
        {
            mecs::archetype::entity_creation Creation
            {
                { &gEntity, 1ull }
                , &EntityMap
                , &SpecialiedPool
                , 0 // This in an invalid value...
                , 1
                , *this
                , *SpecialiedPool.m_pArchetypeInstance
            };

            Creation.ProcessIndirect( Reference, CreateCallback, static_cast<typename xcore::function::traits<T_CREATE_CALLBACK>::args_tuple*>(nullptr) );

            // Creates a cache entry
            m_Cache.getOrCreateCache
            ( 0ull
            , *Reference.m_pPool->m_pArchetypeInstance
            , []( const details::cache::per_function& ) constexpr noexcept
            {
                // Nothing to do
            }
            , []( details::cache::per_function& ) constexpr noexcept
            {
                // Nothing to do
            }
            );
        });
    }

    //---------------------------------------------------------------------------------
    template< typename      T_GET_CALLBACK
            , typename      T_CREATE_CALLBACK
            >
    constexpr xforceinline
    void instance::getOrCreateEntity( mecs::component::entity::guid         gEntity
                                    , mecs::archetype::specialized_pool&    SpecialiedPool
                                    , T_GET_CALLBACK&&                      GetCallback
                                    , T_CREATE_CALLBACK&&                   CreateCallback
                                    ) noexcept
    {
        getOrCreateEntityRelax< false >(gEntity, SpecialiedPool, std::forward<T_GET_CALLBACK&&>(GetCallback), std::forward<T_CREATE_CALLBACK&&>(CreateCallback) );
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
                , []( mecs::system::overrides::construct&& C) noexcept
                {
                    auto p = new sys{ std::move(C) };
                    return std::unique_ptr<mecs::system::instance>{ static_cast<mecs::system::instance*>(p) };
                }
                , T_SYSTEM::type_name_v
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

            //
            // Notify to anyone waiting to know if a new system is created
            //
            if (m_Event.m_CreatedSystem.hasSubscribers())
                m_Event.m_CreatedSystem.NotifyAll(*p);
        });

        xassert(p);
        return *p;
    }

}