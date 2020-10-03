namespace mecs::archetype
{
    //----------------------------------------------------------------------------------------------------
    // ARCHETYPE ENTITY CREATION
    //----------------------------------------------------------------------------------------------------

    //----------------------------------------------------------------------------------------------------

    constexpr bool entity_creation::isValid( void ) const noexcept { return m_pEntityMap; }

    //----------------------------------------------------------------------------------------------------

    template< typename T >
    void entity_creation::operator() ( T&& Callback ) noexcept
    {
        xassert(isValid());
        using fn_traits = xcore::function::traits<T>;
        Initialize( std::forward<T&&>(Callback), reinterpret_cast<typename fn_traits::args_tuple*>(nullptr) );
    }

    //----------------------------------------------------------------------------------------------------

    inline
    void entity_creation::operator() (void) noexcept
    {
        if(m_Guids.size()) 
        {
            if(m_Count <= max_entries_single_thread )
            {
                for( int i = m_StartIndex, end = m_StartIndex + m_Count, g=0; i != end; ++i, ++g )
                {
                    m_pEntityMap->alloc(m_Guids[g], [&]( mecs::component::entity::reference& Reference )
                    {
                        Reference.m_Index = i;
                        Reference.m_pPool = m_pPool;
                        m_pPool->m_EntityPool.getComponentByIndex<mecs::component::entity>(i,0).m_pInstance = &m_pEntityMap->getEntryFromItsValue(Reference);
                        m_pPool->m_pArchetypeInstance->m_Events.m_CreatedEntity.NotifyAll(m_pPool->m_EntityPool.getComponentByIndex<mecs::component::entity>(i,0), m_System );
                    });
                }
            }
            else
            {
                xcore::scheduler::channel Channel( xconst_universal_str("entity_creation::Initialize+g"));
                for (int i = m_StartIndex, end = m_StartIndex + m_Count, g = 0; i != end; )
                {
                    int my_end = std::min<int>( end, i + max_entries_single_thread);
                    Channel.SubmitJob([this, gStart=g, iStart=i, my_end ]() constexpr noexcept
                    {
                        for( auto i=iStart,g=gStart; i != my_end; ++i, ++g ) m_pEntityMap->alloc(m_Guids[g], [&](mecs::component::entity::reference& Reference)
                        {
                            Reference.m_Index = i;
                            Reference.m_pPool = m_pPool;
                            m_pPool->m_EntityPool.getComponentByIndex<mecs::component::entity>(i, 0).m_pInstance = &m_pEntityMap->getEntryFromItsValue(Reference);
                            m_pPool->m_pArchetypeInstance->m_Events.m_CreatedEntity.NotifyAll(m_pPool->m_EntityPool.getComponentByIndex<mecs::component::entity>(i, 0), m_System);
                        });
                    });

                    g += my_end - i;
                    i = my_end;
                }
                Channel.join();
            }
        }
        else 
        {
            if (m_Count <= max_entries_single_thread)
            {
                for (int i = m_StartIndex, end = m_StartIndex + m_Count; i != end; ++i)
                {
                    m_pEntityMap->alloc(mecs::component::entity::guid{ xcore::not_null }, [&](mecs::component::entity::reference& Reference)
                    {
                        Reference.m_Index = i;
                        Reference.m_pPool = m_pPool;
                        m_pPool->m_EntityPool.getComponentByIndex<mecs::component::entity>(i, 0).m_pInstance = &m_pEntityMap->getEntryFromItsValue(Reference);
                        m_pPool->m_pArchetypeInstance->m_Events.m_CreatedEntity.NotifyAll(m_pPool->m_EntityPool.getComponentByIndex<mecs::component::entity>(i, 0), m_System );
                    });
                }
            }
            else
            {
                xcore::scheduler::channel Channel( xconst_universal_str("entity_creation::Initialize"));
                for (int i = m_StartIndex, end = m_StartIndex + m_Count; i != end; )
                {
                    int my_end = std::min<int>( end, i + max_entries_single_thread);
                    Channel.SubmitJob([this, iStart=i, my_end ] () constexpr noexcept
                    {
                        xassert(iStart< my_end);
                        for( auto i=iStart; i != my_end; ++i ) m_pEntityMap->alloc(mecs::component::entity::guid{ xcore::not_null }, [&](mecs::component::entity::reference& Reference)
                        {
                            Reference.m_Index = i;
                            Reference.m_pPool = m_pPool;
                            m_pPool->m_EntityPool.getComponentByIndex<mecs::component::entity>(i, 0).m_pInstance = &m_pEntityMap->getEntryFromItsValue(Reference);
                            m_pPool->m_pArchetypeInstance->m_Events.m_CreatedEntity.NotifyAll(m_pPool->m_EntityPool.getComponentByIndex<mecs::component::entity>(i, 0), m_System);
                        });
                    });

                    i = my_end;
                }
                Channel.join();
            }
        }
        
    }

    //----------------------------------------------------------------------------------------------------

    template< typename...T_COMPONENTS > xforceinline
    constexpr void entity_creation::getTrueComponentIndices( std::span<std::uint8_t> Array, std::span<const mecs::component::descriptor* const> Span, std::tuple<T_COMPONENTS...>* ) const noexcept
    {
        static constexpr std::array Inputs{ &mecs::component::descriptor_v<T_COMPONENTS> ... };
        static_assert( ((std::is_const_v<std::remove_pointer_t<std::remove_reference_t<T_COMPONENTS>>> == false) && ...) );
        int I = 0;
        for(std::uint8_t c=0, end = static_cast<std::uint8_t>(Span.size()); c<end; c++ )
        {
            auto& Desc = *Span[c];
            if( Desc.m_Guid == Inputs[I]->m_Guid )
            {
                if( Desc.m_isDoubleBuffer )
                {
                    const std::uint32_t state = (m_Archetype.m_DoubleBufferInfo.m_StateBits >> Desc.m_BitNumber) & 1;
                    const std::uint32_t iBase = (Span[c - 1]->m_Guid == Desc.m_Guid) ? c - 1 : c; 
                    Array[I] = c + 1 - state;
                }
                else
                {
                    Array[I] = c;
                }
                if( I == static_cast<std::uint8_t>(sizeof...(T_COMPONENTS)-1) ) return;
                I++;
            }
        }

        // can't find a component
        xassume(false);
    }

    //----------------------------------------------------------------------------------------------------
    template< typename T_CALLBACK, typename...T_COMPONENTS >
    inline
    void entity_creation::ProcessIndirect
    ( mecs::component::entity::reference&   Reference
    , T_CALLBACK&&                          Callback
    , std::tuple<T_COMPONENTS...>*
    ) const noexcept
    {
        static_assert(((mecs::component::descriptor_v<T_COMPONENTS>.m_Type != mecs::component::type::SHARE) && ...));
        static_assert(((mecs::component::descriptor_v<T_COMPONENTS>.m_Type != mecs::component::type::TAG)   && ...));
        static_assert(((std::is_same_v<T_COMPONENTS, xcore::types::decay_full_t<T_COMPONENTS>&>) && ...));

        using raw_tuple = std::tuple<T_COMPONENTS...>;
        using sorted_tuple = xcore::types::tuple_sort_t< mecs::component::smaller_guid, raw_tuple >;

        std::array<std::uint8_t, sizeof...(T_COMPONENTS)> ComponentIndices;
        getTrueComponentIndices
        (
            std::span<std::uint8_t>                             { ComponentIndices }
        ,   std::span<const mecs::component::descriptor* const> { m_pPool->m_EntityPool.getDescriptors() }
        ,   reinterpret_cast<sorted_tuple*>(nullptr) 
        );

        Reference.m_Index = m_pPool->m_EntityPool.append(1);
        Reference.m_pPool = m_pPool;

        m_pPool->m_EntityPool.getComponentByIndex<mecs::component::entity>(Reference.m_Index, 0).m_pInstance = &m_pEntityMap->getEntryFromItsValue(Reference);
        Callback(m_pPool->m_EntityPool.getComponentByIndex<xcore::types::decay_full_t<T_COMPONENTS>>(Reference.m_Index, ComponentIndices[xcore::types::tuple_t2i_v<T_COMPONENTS, sorted_tuple>]) ...);

        auto& Event = m_pPool->m_pArchetypeInstance->m_Events.m_CreatedEntity;
        if(Event.hasSubscribers()) Event.NotifyAll(m_pPool->m_EntityPool.getComponentByIndex<mecs::component::entity>(Reference.m_Index, 0), m_System);
    }

    //----------------------------------------------------------------------------------------------------

    template< typename T, typename...T_COMPONENTS > xforceinline
    void entity_creation::Initialize( T&& Callback, std::tuple<T_COMPONENTS...>* ) noexcept
    {
        static_assert( ((mecs::component::descriptor_v<T_COMPONENTS>.m_Type != mecs::component::type::SHARE) && ...) );
        static_assert( ((mecs::component::descriptor_v<T_COMPONENTS>.m_Type != mecs::component::type::TAG)   && ...) );
        static_assert( ((std::is_same_v<T_COMPONENTS, xcore::types::decay_full_t<T_COMPONENTS>&>) && ...));

        using raw_tuple    = std::tuple<T_COMPONENTS...>;
        using sorted_tuple = xcore::types::tuple_sort_t< mecs::component::smaller_guid, raw_tuple >;

        std::array<std::uint8_t, sizeof...(T_COMPONENTS)> ComponentIndices;
        getTrueComponentIndices
        (
            std::span<std::uint8_t>                             { ComponentIndices }
        ,   std::span<const mecs::component::descriptor* const> { m_pPool->m_EntityPool.getDescriptors() }
        ,   reinterpret_cast<sorted_tuple*>(nullptr) 
        );
        if(m_Guids.size()) 
        {
            xassert( m_Guids.size() == m_Count );
            if(m_Count <= max_entries_single_thread )
            {
                for( int i = m_StartIndex, end = m_StartIndex + m_Count, g=0; i != end; ++i, ++g )
                {
                    m_pEntityMap->alloc(m_Guids[g], [&]( mecs::component::entity::reference& Reference )
                    {
                        Reference.m_Index = i;
                        Reference.m_pPool = m_pPool;
                        m_pPool->m_EntityPool.getComponentByIndex<mecs::component::entity>(i,0).m_pInstance = &m_pEntityMap->getEntryFromItsValue(Reference);
                        Callback(m_pPool->m_EntityPool.getComponentByIndex<xcore::types::decay_full_t<T_COMPONENTS>>(i, ComponentIndices[xcore::types::tuple_t2i_v<T_COMPONENTS, sorted_tuple>]) ...);
                        m_pPool->m_pArchetypeInstance->m_Events.m_CreatedEntity.NotifyAll(m_pPool->m_EntityPool.getComponentByIndex<mecs::component::entity>(i,0), m_System );
                    });
                }
            }
            else
            {
                xcore::scheduler::channel Channel( xconst_universal_str("entity_creation::Initialize+g"));
                for (int i = m_StartIndex, end = m_StartIndex + m_Count, g = 0; i != end; )
                {
                    int my_end = std::min<int>( end, i + max_entries_single_thread);
                    Channel.SubmitJob([this,&Callback,&ComponentIndices, gStart=g, iStart=i, my_end ]() constexpr noexcept
                    {
                        for( auto i=iStart,g=gStart; i != my_end; ++i, ++g ) m_pEntityMap->alloc(m_Guids[g], [&](mecs::component::entity::reference& Reference)
                        {
                            Reference.m_Index = i;
                            Reference.m_pPool = m_pPool;
                            m_pPool->m_EntityPool.getComponentByIndex<mecs::component::entity>(i, 0).m_pInstance = &m_pEntityMap->getEntryFromItsValue(Reference);
                            Callback(m_pPool->m_EntityPool.getComponentByIndex<xcore::types::decay_full_t<T_COMPONENTS>>(i, ComponentIndices[xcore::types::tuple_t2i_v<T_COMPONENTS, sorted_tuple>]) ...);
                            m_pPool->m_pArchetypeInstance->m_Events.m_CreatedEntity.NotifyAll(m_pPool->m_EntityPool.getComponentByIndex<mecs::component::entity>(i, 0), m_System);
                        });
                    });

                    g += my_end - i;
                    i = my_end;
                }
                Channel.join();
            }
        }
        else 
        {
            if (m_Count <= max_entries_single_thread)
            {
                for (int i = m_StartIndex, end = m_StartIndex + m_Count; i != end; ++i)
                {
                    m_pEntityMap->alloc(mecs::component::entity::guid{ xcore::not_null }, [&](mecs::component::entity::reference& Reference)
                    {
                        Reference.m_Index = i;
                        Reference.m_pPool = m_pPool;
                        m_pPool->m_EntityPool.getComponentByIndex<mecs::component::entity>(i, 0).m_pInstance = &m_pEntityMap->getEntryFromItsValue(Reference);
                        Callback(m_pPool->m_EntityPool.getComponentByIndex< xcore::types::decay_full_t<T_COMPONENTS>>(i, ComponentIndices[xcore::types::tuple_t2i_v<T_COMPONENTS, sorted_tuple>]) ...);
                        m_pPool->m_pArchetypeInstance->m_Events.m_CreatedEntity.NotifyAll(m_pPool->m_EntityPool.getComponentByIndex<mecs::component::entity>(i, 0), m_System );
                    });
                }
            }
            else
            {
                xcore::scheduler::channel Channel( xconst_universal_str("entity_creation::Initialize"));
                for (int i = m_StartIndex, end = m_StartIndex + m_Count; i != end; )
                {
                    int my_end = std::min<int>( end, i + max_entries_single_thread);
                    Channel.SubmitJob([this,&Callback,&ComponentIndices, iStart=i, my_end ] () constexpr noexcept
                    {
                        xassert(iStart< my_end);
                        for( auto i=iStart; i != my_end; ++i ) m_pEntityMap->alloc(mecs::component::entity::guid{ xcore::not_null }, [&](mecs::component::entity::reference& Reference)
                        {
                            Reference.m_Index = i;
                            Reference.m_pPool = m_pPool;
                            m_pPool->m_EntityPool.getComponentByIndex<mecs::component::entity>(i, 0).m_pInstance = &m_pEntityMap->getEntryFromItsValue(Reference);
                            Callback(m_pPool->m_EntityPool.getComponentByIndex<xcore::types::decay_full_t<T_COMPONENTS>>(i, ComponentIndices[xcore::types::tuple_t2i_v<T_COMPONENTS, sorted_tuple>]) ...);
                            m_pPool->m_pArchetypeInstance->m_Events.m_CreatedEntity.NotifyAll(m_pPool->m_EntityPool.getComponentByIndex<mecs::component::entity>(i, 0), m_System);
                        });
                    });

                    i = my_end;
                }
                Channel.join();
            }
        }
    }

    //----------------------------------------------------------------------------------------------------
    // ARCHETYPE INSTANCE
    //----------------------------------------------------------------------------------------------------
    inline
    void instance::Init(
        const std::span<const mecs::component::descriptor* const>   DataDescriptorList
    ,   const std::span<const mecs::component::descriptor* const>   ShareDescriptorList
    ,   const std::span<const mecs::component::descriptor* const>   TagDescriptorList
    ,   mecs::component::entity::map*                               pEntityMap          ) noexcept
    {
        m_Descriptor.Init(DataDescriptorList, ShareDescriptorList, TagDescriptorList);
        m_MainPoolDescriptorData[0] = &mecs::component::descriptor_v<specialized_pool>;
        m_MainPoolDescriptorSpan = std::span{ m_MainPoolDescriptorData.data(), ShareDescriptorList.size() + 1 };
        m_pEntityMap = pEntityMap;

        if (ShareDescriptorList.size())
        {
            for (int i = 0; i < ShareDescriptorList.size(); ++i)
            {
                auto pShare = ShareDescriptorList[i];
                const auto Index = i + 1;
                m_MainPoolDescriptorData[Index] = pShare;

                // For those share components that the user wants us to create a hash map we crate one
                if (pShare->m_ShareComponentMapType != mecs::component::share::type_map::NONE) m_ShareMapTable[pShare->m_BitNumber] = std::make_unique<share_map>();
            }
        }

        m_MainPool.Init(*this, m_MainPoolDescriptorSpan, 100000u);
    }

    //----------------------------------------------------------------------------------------------------
    inline
    void instance::MemoryBarrierSync( mecs::sync_point::instance& SyncPoint ) noexcept
    {
        XCORE_PERF_ZONE_SCOPED_N("mecs::archetype::MemoryBarrierSync")

        //
        // Avoid updating multiple times for a given sync_point done
        //
        if (m_pLastSyncPoint == &SyncPoint && m_LastTick == SyncPoint.m_Tick)
            return;

        // Save state of previous update to make sure no one ask us to update twice
        m_pLastSyncPoint = &SyncPoint;
        m_LastTick       = SyncPoint.m_Tick;

        //
        // Make sure to lock for writing
        // TODO: Do we really need this lock here?
        //
        xcore::lock::scope Lock(m_SemaphoreLock);

        //
        // Update all the pools
        //
        m_MainPool.MemoryBarrier();
        if(m_MainPool.size()==0) return;

        for( int i=0, end = static_cast<int>(m_MainPool.size()); i != end; ++i )
        {
            auto& Specialized = m_MainPool.getComponentByIndex<specialized_pool>(i,0);
            Specialized.m_EntityPool.MemoryBarrier();
        }

        //
        // Update double buffer state if we have any
        //
        m_DoubleBufferInfo.m_StateBits ^= m_DoubleBufferInfo.m_DirtyBits.load(std::memory_order_relaxed);
        m_DoubleBufferInfo.m_DirtyBits.store(0, std::memory_order_relaxed);
    }

    //----------------------------------------------------------------------------------------------------
    inline
    void instance::Start( void ) noexcept
    {
        //
        // Update the main pool just in case
        //
        m_MainPool.MemoryBarrier();

        //
        // Lets update all our pools
        //
        for (int end = m_MainPool.size(), i = 0; i < end; ++i)
        {
            auto& Specialized = m_MainPool.getComponentByIndex<specialized_pool>(i, 0);
            Specialized.m_EntityPool.MemoryBarrier();
        }

        //
        // Update double buffer state if we have any
        //
        std::uint64_t DirtyBits = 0;
        for( int i = 1, end = static_cast<int>(m_Descriptor.m_DataDescriptorSpan.size()); i < end; ++i )
        {
            const auto& Descriptor = *m_Descriptor.m_DataDescriptorSpan[i];
            if(Descriptor.m_isDoubleBuffer)
            {
                DirtyBits |= (1ull << i);
                i++;
                xassert( m_Descriptor.m_DataDescriptorSpan[i]->m_Guid == Descriptor.m_Guid );
            }
        }
        m_DoubleBufferInfo.m_StateBits ^= DirtyBits;
        m_DoubleBufferInfo.m_DirtyBits.store(0, std::memory_order_relaxed);
    }

    //----------------------------------------------------------------------------------------------------

    template< typename...T_SHARE_COMPONENTS > 
    entity_creation instance::CreateEntities( 
        mecs::system::instance&                     Instance
    ,   const int                                   nEntities
    ,   std::span<mecs::component::entity::guid>    Guids
    ,   T_SHARE_COMPONENTS&&...                     ShareComponents ) noexcept
    {
        xassert(Guids.size() == 0u || Guids.size() == static_cast<std::uint32_t>(nEntities) );

        if constexpr ( !!(sizeof...(T_SHARE_COMPONENTS)) )
        {
            static_assert( ((mecs::component::descriptor_v<T_SHARE_COMPONENTS>.m_Type == mecs::component::type::SHARE) && ...) );
            static_assert( ((std::is_same_v<T_SHARE_COMPONENTS, xcore::types::decay_full_t<T_SHARE_COMPONENTS>>) && ...) );

            xassert(sizeof...(T_SHARE_COMPONENTS) == m_Descriptor.m_ShareDescriptorSpan.size());

            using share_tuple  = std::tuple<T_SHARE_COMPONENTS...>;
            using sorted_tuple = xcore::types::tuple_sort_t< mecs::component::smaller_guid, share_tuple >;

            // Create array containg all the keys in the right order
            std::array<std::uint64_t, sizeof...(T_SHARE_COMPONENTS)> ShareComponentKeys;
            ((ShareComponentKeys[xcore::types::tuple_t2i_v<T_SHARE_COMPONENTS, sorted_tuple>] = mecs::component::descriptor_v<T_SHARE_COMPONENTS>.m_fnGetKey(&ShareComponents)), ...);

            const specialized_pool::type_guid SpecializedGuidTypeGuid{ (ShareComponentKeys[xcore::types::tuple_t2i_v<T_SHARE_COMPONENTS, share_tuple>] + ...) };

            static constexpr auto nKeys = sizeof...(T_SHARE_COMPONENTS);
            for (int end = static_cast<int>(m_MainPool.size()), i = 0; i < end; ++i)
            {
                auto& Specialized = m_MainPool.getComponentByIndex<specialized_pool>(i, 0);

                if( Specialized.m_TypeGuid == SpecializedGuidTypeGuid )
                {
                    const auto  Index = Specialized.m_EntityPool.append(nEntities);
                    if (Index != ~0u) return
                    {
                        Guids
                    ,   m_pEntityMap
                    ,   &Specialized
                    ,   Index
                    ,   nEntities
                    ,   Instance
                    ,   *this
                    };
                }
            }

            //TODO: The append may need to take a call back here to keep it thread safe
            //      Allocates an pool entry
            const int PoolIndex   = m_MainPool.append(); m_MainPool.MemoryBarrier();
            auto&     Specialized = m_MainPool.getComponentByIndex<specialized_pool>(PoolIndex, 0);

            // Copy all the keys
            for( int nKeys = static_cast<int>(ShareComponentKeys.size()), k = 0; k < nKeys; ++k) Specialized.m_ShareComponentKeysMemory[k] = ShareComponentKeys[k];
            Specialized.m_ShareComponentKeysSpan = std::span<std::uint64_t>{ Specialized.m_ShareComponentKeysMemory.data(), ShareComponentKeys.size() };
            Specialized.m_TypeGuid               = SpecializedGuidTypeGuid;
            Specialized.m_MainPoolIndex          = PoolIndex;

            // Copy all the components into the right location
            ( 
                (
                    m_MainPool.getComponentByIndex<T_SHARE_COMPONENTS>
                    (
                        PoolIndex
                        , 1 + xcore::types::tuple_t2i_v<T_SHARE_COMPONENTS, sorted_tuple>
                    ) = std::move(ShareComponents)
                ) 
                , ... 
            );

            // Initialize the pool
            Specialized.m_EntityPool.Init(*this, m_Descriptor.m_DataDescriptorSpan, std::max<int>(nEntities, 100000 /*MaxEntries*/ ));
            Specialized.m_pArchetypeInstance = this;

            // Notify possible listeners
            if (m_Events.m_CreatedPool.hasSubscribers())
                m_Events.m_CreatedPool.NotifyAll(Instance, Specialized);

            return
            {
                Guids
                , m_pEntityMap
                , &Specialized
                , Specialized.m_EntityPool.append(nEntities)
                , nEntities
                , Instance
                , *this
            };
        }
        else
        {
            // If you hit this assert is because you forgot to pass the share component
            xassert( m_Descriptor.m_ShareDescriptorSpan.size() == 0 );

            for (int end = m_MainPool.size(), i = 0; i < end; ++i)
            {
                auto&       Specialized = m_MainPool.getComponentByIndex<specialized_pool>(i, 0);
                const auto  Index       = Specialized.m_EntityPool.append(nEntities); 
                if (Index != ~0u) return
                {
                    Guids
                ,   m_pEntityMap
                ,   &Specialized
                ,   Index
                ,   nEntities
                ,   Instance
                ,   *this
                };
            }

            //TODO: The append may need to take a call back here to keep it thread safe
            //      Allocates an pool entry
            const int PoolIndex = m_MainPool.append(); m_MainPool.MemoryBarrier();
            auto& Specialized = m_MainPool.getComponentByIndex<specialized_pool>(PoolIndex, 0);

            //TODO: Preallocate nEntites before returning
            Specialized.m_TypeGuid.setNull();
            Specialized.m_MainPoolIndex = PoolIndex;
            Specialized.m_EntityPool.Init( *this, m_Descriptor.m_DataDescriptorSpan, std::max<int>(nEntities, 100000 /*MaxEntries*/ ));
            Specialized.m_pArchetypeInstance = this;

            // Notify possible listeners
            if (m_Events.m_CreatedPool.hasSubscribers())
                m_Events.m_CreatedPool.NotifyAll(Instance, Specialized);

            return
            {
                Guids
            ,   m_pEntityMap
            ,   &Specialized
            ,   Specialized.m_EntityPool.append(nEntities)
            ,   nEntities
            ,   Instance
            ,   *this
            };
        }
    }

    //----------------------------------------------------------------------------------------------------

    template< typename T_CALLBACK, typename...T_SHARE_COMPONENTS > xforceinline
    void instance::CreateEntity( 
        mecs::system::instance& Instance
    ,   T_CALLBACK&&            CallBack
    ,   T_SHARE_COMPONENTS&&... ShareComponents ) noexcept
    {
        // Make sure the first parameter is a function and must not have a return
        static_assert( std::is_same_v<xcore::function::traits<T_CALLBACK>::return_type, void> );

        std::array< mecs::component::entity::guid,1> GuidList { mecs::component::entity::guid{ xcore::not_null } };
        if constexpr( std::is_same_v< T_CALLBACK, void(*)() > )
        {
            CreateEntities(Instance, 1, GuidList, std::forward<T_SHARE_COMPONENTS&&>(ShareComponents) ...)();
        }
        else
        {
            CreateEntities(Instance, 1, GuidList, std::forward<T_SHARE_COMPONENTS&&>(ShareComponents) ...)(CallBack);
        }
    }

    //----------------------------------------------------------------------------------------------------

    template< typename...T_SHARE_COMPONENTS >constexpr
    specialized_pool::type_guid instance::getSpecializedPoolGuid( const instance& Instance, T_SHARE_COMPONENTS&&...ShareComponents ) const noexcept
    {
        static_assert(mecs::component::descriptor_v<T_SHARE_COMPONENTS>.m_Type == mecs::component::type::SHARE);
        xassert( sizeof...(T_SHARE_COMPONENTS) == Instance.m_Descriptor.m_ShareDescriptorSpan.size() );
        return{ (mecs::component::descriptor_v<T_SHARE_COMPONENTS>.m_fn_getKey(&ShareComponents) + ... ) };
    }

    //----------------------------------------------------------------------------------------------------

    inline
    void instance::deleteEntity( mecs::system::instance& System, mecs::component::entity& Entity ) noexcept
    {
        XCORE_PERF_ZONE_SCOPED()
        if (Entity.isZombie()) return;

        // Mark the entity as a zombie
        Entity.MarkAsZombie();

        // Mark for deletion in the pool
        auto& Reference = Entity.getReference();
        Reference.m_pPool->m_EntityPool.deleteBySwap(Reference.m_Index);

        //
        // Notify that we are deleting an entity
        //
        auto& Archetype = *Reference.m_pPool->m_pArchetypeInstance;
        if (Archetype.m_Events.m_DestroyEntity.hasSubscribers())
            Archetype.m_Events.m_DestroyEntity.NotifyAll(Entity, System);
    }

    //----------------------------------------------------------------------------------------------------

    inline
    void instance::moveEntityBetweenSpecializePools(
          system::instance& System
        , int               FromSpecializePoolIndex
        , int               EntityIndexInSpecializedPool
        , std::uint64_t     NewCRCPool
        , std::byte**       pPointersToShareComponents
        , std::uint64_t*    pListOfCRCFromShareComponents ) noexcept
    {
        //
        // Sanity Check
        //
        xassert_block_basic()
        {
            std::uint64_t CRC = 0;
            for( int i=0, end = static_cast<int>(m_Descriptor.m_ShareDescriptorSpan.size()); i<end; ++i )
            {
                CRC += pListOfCRCFromShareComponents[i];
            }
            xassert( CRC == NewCRCPool );
        }

        //
        // Try to find an existing pool
        //
        int               i     = 0;
        specialized_pool* pPool = nullptr;
    TRY_FIND_AGAIN:

        for( int end = m_MainPool.size(); i<end; ++i )
        {
            auto& Specialized = m_MainPool.getComponentByIndex<specialized_pool>(i, 0);
            if(Specialized.m_TypeGuid.m_Value == NewCRCPool)
            {
                pPool = &Specialized;
                break;
            }
        }

        //
        // Create a pool if we can not find one
        //
        if( pPool == nullptr )
        {
            xcore::lock::scope Lk(m_SemaphoreLock);
            if( i != m_MainPool.size() ) goto TRY_FIND_AGAIN;

            const int PoolIndex = m_MainPool.append(); m_MainPool.MemoryBarrier();
            auto& Specialized   = m_MainPool.getComponentByIndex<specialized_pool>(PoolIndex, 0);

            //TODO: Preallocate nEntites before returning
            Specialized.m_TypeGuid.m_Value       = NewCRCPool;
            Specialized.m_MainPoolIndex          = PoolIndex;
            Specialized.m_EntityPool.Init(*this, m_Descriptor.m_DataDescriptorSpan, 100000 /*MaxEntries*/ );
            Specialized.m_pArchetypeInstance     = this;
            Specialized.m_ShareComponentKeysSpan = std::span{ Specialized.m_ShareComponentKeysMemory.data(), m_Descriptor.m_ShareDescriptorSpan.size() };

            for( int i=0; i< Specialized.m_ShareComponentKeysSpan.size(); i++ )
            {
                Specialized.m_ShareComponentKeysSpan[i] = pListOfCRCFromShareComponents[i];
                memcpy( m_MainPool.getComponentByIndexRaw(PoolIndex, 1 + i), pPointersToShareComponents[i], m_Descriptor.m_ShareDescriptorSpan[i]->m_Size );
            }

            // Notify possible listeners
            if (m_Events.m_CreatedPool.hasSubscribers())
                m_Events.m_CreatedPool.NotifyAll(System, Specialized);

            pPool = &Specialized;
        }

        //
        // Move the entity
        //
        auto Index           = pPool->m_EntityPool.append(1);

        // If we failt to allocate then we run out of space and must find another pool
        if( Index == ~0 )
        {
            i++;
            goto TRY_FIND_AGAIN;
        }

        auto& OldSpecialized = m_MainPool.getComponentByIndex<specialized_pool>(FromSpecializePoolIndex, 0);
        for( i=0; i< m_Descriptor.m_DataDescriptorSpan.size(); i++ )
        {
            m_Descriptor.m_DataDescriptorSpan[i]->m_fnMove(pPool->m_EntityPool.getComponentByIndexRaw(Index, i),
                OldSpecialized.m_EntityPool.getComponentByIndexRaw(EntityIndexInSpecializedPool, i) );
        }

        // We must update the entities now
        // TODO: Should it be possible to change the entity without accessing the entity hash map?
        {
            auto& OldEntity = OldSpecialized.m_EntityPool.getComponentByIndex<mecs::component::entity>(EntityIndexInSpecializedPool, 0);
            auto& NewEntity = pPool->m_EntityPool.getComponentByIndex<mecs::component::entity>(Index, 0);

            m_pEntityMap->find( OldEntity.getGUID(), [&]( auto& Entry ) constexpr
            {
                Entry.m_pPool           = pPool;
                Entry.m_Index           = Index; 
            });

            // Mark the old entity for deletion
            OldSpecialized.m_EntityPool.deleteBySwap(EntityIndexInSpecializedPool);
            OldEntity.MarkAsZombie();
        }
    }

    //----------------------------------------------------------------------------------------------------

    template< typename... T_SHARE_COMPONENTS >
    inline
    specialized_pool& instance::getOrCreateSpecializedPool( system::instance&         System
                                                          , int                       MinFreeEntries
                                                          , int                       MaxEntries    
                                                          , T_SHARE_COMPONENTS...     ShareComponents
                                                          ) noexcept
    {
        // Share components count must match the archetype share component list
        xassert( sizeof...(T_SHARE_COMPONENTS) == m_Descriptor.m_ShareDescriptorSpan.size() );

        const std::array<std::uint64_t, sizeof...(T_SHARE_COMPONENTS)> ArrayList{ mecs::component::descriptor_v<T_SHARE_COMPONENTS>.m_fnGetKey(&ShareComponents)  ... };
        const auto ShareKey = [] ( auto& Array ){ std::uint64_t Key = 0; for( auto x : Array ) Key += x; return Key; }( ArrayList );

        //
        // Search for the right specialized pool
        //
        int i=0;
        CONTINUE_SEARCHING:
        {
            const std::uint32_t MinFree = MinFreeEntries;
            for (int end = m_MainPool.size(); i < end; ++i)
            {
                auto& Pool = m_MainPool.getComponentByIndex<specialized_pool>(i, 0);
                if (Pool.m_TypeGuid.m_Value == ShareKey && Pool.m_EntityPool.FreeCount() >= MinFree) return Pool;
            }
        }

        //
        // Create new pool
        //
        {
            xcore::lock::scope Lk(m_SemaphoreLock);
            if (i != m_MainPool.size()) goto CONTINUE_SEARCHING;

            const auto Index = m_MainPool.append(1);
            auto&      Pool  = m_MainPool.getComponentByIndex<specialized_pool>( Index, 0 );

            using unsorted_tuple = std::tuple<T_SHARE_COMPONENTS ...>;
            using sorted_tuple   = xcore::types::tuple_sort_t< mecs::component::smaller_guid, unsorted_tuple >;

            // Copy all the share components
            ((m_MainPool.getComponentByIndex<T_SHARE_COMPONENTS>(Index, 1 + xcore::types::tuple_t2i_v< T_SHARE_COMPONENTS, sorted_tuple> ) = ShareComponents), ... );

            // Copy all the share components keys
            ((Pool.m_ShareComponentKeysMemory[xcore::types::tuple_t2i_v< T_SHARE_COMPONENTS, sorted_tuple>] = ArrayList[xcore::types::tuple_t2i_v< T_SHARE_COMPONENTS, unsorted_tuple>]), ... );

            Pool.m_MainPoolIndex          = Index;
            Pool.m_ShareComponentKeysSpan = std::span{ Pool.m_ShareComponentKeysMemory.data(), sizeof...(T_SHARE_COMPONENTS) };
            Pool.m_TypeGuid.m_Value       = ShareKey;
            Pool.m_pArchetypeInstance     = this;
            Pool.m_EntityPool.Init( *this, m_Descriptor.m_DataDescriptorSpan, MaxEntries );

            if( m_Events.m_CreatedPool.hasSubscribers() )
                m_Events.m_CreatedPool.NotifyAll( System, Pool );

            // Officially update the count
            m_MainPool.UpdateCount();

            return Pool;
        }
    }




    /*
    details::entity_creation findSpecializedPoolForAllocation(int nEntities = 1, specialized_pool::type_guid SpecialiedTypeGuid = specialized_pool::type_guid{ 0ull }) noexcept
    {
        if(SpecialiedTypeGuid.isValid())
        {
            for (int end = m_MainPool.size(), i = 0; i < end; ++i)
            {
                auto& Specialized = m_MainPool.getComponentByIndex<specialized_pool>(i, 1);

                if (Specialized.m_TypeGuid == SpecialiedTypeGuid )
                {
                    const auto Index = Specialized.m_EntityPool.append(nEntities);
                    if( Index != ~0u ) return
                    {
                        m_pEntityMap
                    ,   &Specialized
                    ,   Index
                    ,   nEntities
                    };
                }
            }
        }
        else
        {
            for (int end = m_MainPool.size(), i = 0; i < end; ++i)
            {
                auto&       Specialized = m_MainPool.getComponentByIndex<specialized_pool>(i, 1);
                const auto  Index       = Specialized.m_EntityPool.append(nEntities);
                if (Index != ~0u) return
                {
                    m_pEntityMap
                ,   &Specialized
                ,   Index
                ,   nEntities
                };
            }
        }
        return { nullptr, nullptr, 0, 0 };
    }
    */


    /*
            template< typename T, typename...T_SHARE_COMPONENTS > xforceinline
            void CreateEntity(mecs::component::entity::guid Guid, T&& CallBack, T_SHARE_COMPONENTS&&...ShareComponents) noexcept
            {
                std::array GuidList { Guid };
                CreateEntities( 1, GuidList, std::forward<T_SHARE_COMPONENTS>(ShareComponents) ... )(CallBack);
            }
    */

    /*
    template< typename T >
    void CreateEntity( mecs::system::instance& System, T&& Callback ) noexcept
    {
        using fn_traits = xcore::function::traits<T>;

        auto CreateEntity = [&]( mecs::entity_pool::index i ) noexcept
        {
            auto& Specialized = m_MainPool.get<specialized>(i);
            if (Specialized.m_EntityPool.size() < Specialized.m_EntityPool.capacity())
            {
                const auto Index = Specialized.m_EntityPool.append();

                if constexpr (fn_traits::arg_count_v)
                {
                    using call_details = details::call_details<fn_traits::args_tuple>;
                    details::CreateEntity
                    (
                        m_MainPool
                        , i
                        , Specialized.m_EntityPool
                        , Index
                        , std::forward<T>(Callback)
                        , reinterpret_cast<typename fn_traits::args_tuple*>(nullptr)
                        , reinterpret_cast<typename call_details::tuple_data_components*>(nullptr)
                        , reinterpret_cast<typename call_details::tuple_share_components*>(nullptr)
                    );
                }

                auto& Entity = Specialized.m_EntityPool.get<mecs::component::entity>(Index);
                Specialized.m_Events.m_CreateEntity(System, Entity);
                m_Events.m_CreatedEntity.NotifyAll(System, Entity);
                break;
            }
        };

        for (int end = m_MainPool.size(), i = 0; i != end; ++i)
        {
            auto& Specialized = m_MainPool.get<specialized>(i);
            if (Specialized.m_EntityPool.size() < Specialized.m_EntityPool.capacity())
            {
                CreateEntity(i);
                return;
            }
        }

        // If we could not find an entry create a new one
        CreateEntity(m_MainPool.append());
    }
    */

    /*
    template< typename T >
    specialized& getOrCreateSpecializedPool( T&& Callback, int MaxEntries = -1 ) noexcept
    {
        using fn_traits = xcore::function::traits<T>;
        xassert( m_Descriptor.m_ShareDescriptors.size() == fn_traits::arg_count_v );
        if constexpr (fn_traits::arg_count_v)
        {
            fn_traits::args_tuple                               Args;
            std::array<std::uint64_t, fn_traits::arg_count_v>   Keys;
            Callback(Args...);
            for( int i=0;i< fn_traits::arg_count_v;++i)
            {
                Keys[i] = mecs::component::descriptor_v<Args>.m_fn_getkey( std::get<Args>(Args) );
            }
        }
        else
        {
            m_lSpecializedArchetypes.size()
        }
    }
    */

    //m_PoolPools.Init(*this, m_ShareDescriptorSpan, 30000);

    //---------------------------------------------------------------------------------
    // ARCHETYPE:: DATA BASE DETAILS
    //---------------------------------------------------------------------------------
    namespace details
    {
        //---------------------------------------------------------------------------------
        template< typename...T_ARGS >
        constexpr auto getComponentDescriptorArray( std::tuple<T_ARGS...>* ) noexcept
        {
            return std::array<const mecs::component::descriptor*,sizeof...(T_ARGS)>{ &mecs::component::descriptor_v<T_ARGS> ... };
        }

        //---------------------------------------------------------------------------------
        template< typename...T_ARGS >
        constexpr auto hasDuplicateComponents(void) noexcept
        {
            std::array<mecs::component::type_guid, sizeof...(T_ARGS)> Array{ mecs::component::descriptor_v<T_ARGS>.m_Guid ... };
            for( int i=0;   i<Array.size(); ++i )
            for( int j=i+1; j<Array.size(); ++j )
            {
                if( Array[i] == Array[j] ) return true;
            }
            return false;
        }

        //---------------------------------------------------------------------------------
        template< typename...T_COMPONENTS >
        constexpr auto ComputeGUID(std::tuple<T_COMPONENTS...>*) noexcept
        {
            return mecs::component::entity::guid
            {
                ((mecs::component::descriptor_v<T_COMPONENTS>.m_Guid.m_Value) + ... + 0ull)
            };
        }

        //---------------------------------------------------------------------------------
        template< typename...T_COMPONENTS >
        constexpr auto getTupleDataComponents( std::tuple<T_COMPONENTS...>* ) noexcept
        {
            static_assert( sizeof...(T_COMPONENTS) < (mecs::settings::max_data_components_per_entity - 1) );
            static_assert( (std::is_same_v< xcore::types::decay_full_t<T_COMPONENTS>, T_COMPONENTS > && ...), "Make sure you only pass the component type (no const, *, & )" );
            static_assert( ((mecs::component::descriptor_v<T_COMPONENTS>.m_Guid.m_Value != 0ull) && ...), "You are passing a component which have not a type_guid_v set" );
            static_assert( ((mecs::component::descriptor_v<T_COMPONENTS>.m_Type != mecs::component::type::TAG) && ...) );
            static_assert( hasDuplicateComponents<T_COMPONENTS...>() == false );

            using expanded_tuple = xcore::types::tuple_sort_t
            <
                mecs::component::smaller_guid
            ,   xcore::types::tuple_cat_t
                <
                    std::conditional_t
                    < 
                        true //T_EXPAND_V
                    ,   std::conditional_t
                        <
                            mecs::component::descriptor_v<T_COMPONENTS>.m_isDoubleBuffer
                        ,   std::tuple<T_COMPONENTS, T_COMPONENTS>
                        ,   std::tuple<T_COMPONENTS>
                        >
                    ,   std::tuple<T_COMPONENTS>
                    > ...
                >
            >;

            return std::tuple
            {
                ComputeGUID( reinterpret_cast<expanded_tuple*>(nullptr) )
            ,   getComponentDescriptorArray( reinterpret_cast<expanded_tuple*>(nullptr) )
            };
        }

        //---------------------------------------------------------------------------------
        template< typename...T_COMPONENTS >
        constexpr auto getTupleDataComponentsAddEntity( std::tuple<T_COMPONENTS...>* ) noexcept
        {
            static_assert(((mecs::component::descriptor_v<T_COMPONENTS>.m_Guid.m_Value != 1ull) && ...), "You can not have an entity component in the given list");
            using my_tuple = xcore::types::tuple_cat_t< std::tuple<mecs::component::entity>, std::tuple<T_COMPONENTS...> >;
            return getTupleDataComponents( reinterpret_cast<my_tuple*>(nullptr) );
        }

        //---------------------------------------------------------------------------------
        template< typename...T_COMPONENTS >
        constexpr auto getTupleShareComponents( std::tuple<T_COMPONENTS...>* ) noexcept
        {
            static_assert(sizeof...(T_COMPONENTS) < (mecs::settings::max_data_components_per_entity - 1));
            static_assert((std::is_same_v< xcore::types::decay_full_t<T_COMPONENTS>, T_COMPONENTS > && ...), "Make sure you only pass the component type (no const, *, & )");
            static_assert(((mecs::component::descriptor_v<T_COMPONENTS>.m_Guid.m_Value != 0ull) && ...), "You are passing a component which have not a type_guid_v set");
            static_assert(((mecs::component::descriptor_v<T_COMPONENTS>.m_Guid.m_Value != 1ull) && ...), "You can not have an entity component in the given list");
            static_assert(((mecs::component::descriptor_v<T_COMPONENTS>.m_Type == mecs::component::type::SHARE) && ...));

            using expanded_tuple = xcore::types::tuple_sort_t< mecs::component::smaller_guid, std::tuple<T_COMPONENTS ... > >;
            return std::tuple
            {
                mecs::component::entity::guid{ ( mecs::component::descriptor_v<T_COMPONENTS>.m_Guid.m_Value + ... + 0ull ) }
            ,   getComponentDescriptorArray( reinterpret_cast<expanded_tuple*>(nullptr) )
            };
        }

        //---------------------------------------------------------------------------------
        template< typename...T_COMPONENTS >
        constexpr auto getTupleTagComponents( std::tuple<T_COMPONENTS...>* ) noexcept
        {
            static_assert(sizeof...(T_COMPONENTS) < (mecs::settings::max_data_components_per_entity - 1));
            static_assert((std::is_same_v< xcore::types::decay_full_t<T_COMPONENTS>, T_COMPONENTS > && ...), "Make sure you only pass the component type (no const, *, & )");
            static_assert(((mecs::component::descriptor_v<T_COMPONENTS>.m_Guid.m_Value != 0ull) && ...), "You are passing a component which have not a type_guid_v set");
            static_assert(((mecs::component::descriptor_v<T_COMPONENTS>.m_Type == mecs::component::type::TAG) && ...));

            using expanded_tuple = xcore::types::tuple_sort_t< mecs::component::smaller_guid, std::tuple<T_COMPONENTS...> >;
            return std::tuple
            {
                mecs::component::entity::guid{ ( mecs::component::descriptor_v<T_COMPONENTS>.m_Guid.m_Value + ... + 0ull ) }
            ,   getComponentDescriptorArray(reinterpret_cast<expanded_tuple*>(nullptr) )
            };
        }

        //---------------------------------------------------------------------------------
        template< typename T_CALLBACK, typename...T_ARGS, typename...T_EXTRA_ARGS > xforceinline
        void CallFunction( mecs::component::entity::reference& Value, T_CALLBACK&& Callback, std::tuple<T_ARGS...>*, T_EXTRA_ARGS&... ExtraArgs ) noexcept
        {
          //  static_assert(( (std::is_const_v<std::remove_reference_t<std::remove_pointer_t<T_ARGS>>> == false) && ... ));
            static_assert(( (mecs::component::descriptor_v< xcore::types::decay_full_t<T_ARGS> >.m_Type != mecs::component::type::SHARE) && ... ));

            using                               function_tuple       = std::tuple<T_ARGS...>;
            using                               function_tuple_decay = std::tuple< xcore::types::decay_full_t<T_ARGS>... >;

            using                               sorted_tuple   = xcore::types::tuple_sort_t< mecs::component::smaller_guid, function_tuple_decay >;
            static constexpr std::array         SortedDesc     { &mecs::component::descriptor_v< std::tuple_element_t< xcore::types::tuple_t2i_v< T_ARGS, function_tuple >, sorted_tuple>> ... };
            static constexpr std::array         RemapArray     { xcore::types::tuple_t2i_v<      std::tuple_element_t< xcore::types::tuple_t2i_v< T_ARGS, function_tuple >, sorted_tuple>, function_tuple_decay > ... };
            auto&                               Pool           = Value.m_pPool->m_EntityPool;
            const auto&                         Desc           = Pool.m_Descriptors;
            std::array<int, sizeof...(T_ARGS)>  Indices;

            for( int i=0, c=0, end = static_cast<int>(Desc.size()); i<end; ++i )
            {
                if( Desc[i]->m_Guid.m_Value == SortedDesc[c]->m_Guid.m_Value)
                {
                    Indices[RemapArray[c++]] = i;
                    if( c == static_cast<int>(sizeof...(T_ARGS))) break;
                }
            }

            const auto& Archetype = *Value.m_pPool->m_pArchetypeInstance;
            Callback
            (
                ExtraArgs...
                , ([&]() constexpr noexcept -> T_ARGS
                    {
                        using arg_t = xcore::types::decay_full_t<T_ARGS>;
                        if constexpr (mecs::component::descriptor_v< arg_t>.m_isDoubleBuffer )
                        {
                            const auto isNotConst = !(std::is_const_v<std::remove_reference_t<std::remove_pointer_t<T_ARGS>>>);
                            const auto Offset     = isNotConst - static_cast<int>((Archetype.m_DoubleBufferInfo.m_StateBits>>mecs::component::descriptor_v< arg_t>.m_BitNumber)&1);
                            auto&      C          = Pool.getComponentByIndex<arg_t>(Value.m_Index, Offset + Indices[xcore::types::tuple_t2i_v<T_ARGS, std::tuple<T_ARGS...>>]);

                            if constexpr (std::is_pointer_v<T_ARGS>) return reinterpret_cast<T_ARGS>(&C);
                            else                                     return reinterpret_cast<T_ARGS>(C);
                        }
                        else
                        {
                            auto& C = Pool.getComponentByIndex<arg_t>(Value.m_Index, Indices[xcore::types::tuple_t2i_v<T_ARGS, std::tuple<T_ARGS...>>]);

                            if constexpr (std::is_pointer_v<T_ARGS>) return reinterpret_cast<T_ARGS>(&C);
                            else                                     return reinterpret_cast<T_ARGS>(C);
                        }
                    }())...
            );
        }

        //---------------------------------------------------------------------------------
        template< typename T_CALLBACK, typename...T_ARGS, typename...T_EXTRA_ARGS > xforceinline
        void CallPoolFunction( mecs::archetype::specialized_pool& Pool, T_CALLBACK&& Callback, std::tuple<T_ARGS...>*, T_EXTRA_ARGS&... ExtraArgs ) noexcept
        {
            static_assert(( (mecs::component::descriptor_v< xcore::types::decay_full_t<T_ARGS> >.m_Type == mecs::component::type::SHARE) && ... ));
            static_assert( ((std::is_same_v< const T_ARGS, T_ARGS> ) && ...));

            using                               function_tuple       = std::tuple<T_ARGS...>;
            using                               function_tuple_decay = std::tuple< xcore::types::decay_full_t<T_ARGS>... >;

            using                               sorted_tuple   = xcore::types::tuple_sort_t< mecs::component::smaller_guid, function_tuple_decay >;
            static constexpr std::array         SortedDesc     { &mecs::component::descriptor_v< std::tuple_element_t< xcore::types::tuple_t2i_v< T_ARGS, function_tuple >, sorted_tuple>> ... };
            static constexpr std::array         RemapArray     { xcore::types::tuple_t2i_v<      std::tuple_element_t< xcore::types::tuple_t2i_v< T_ARGS, function_tuple >, sorted_tuple>, function_tuple_decay > ... };
            const auto&                         Archetype      = *Pool.m_pArchetypeInstance;
            const auto&                         Desc           = Archetype.m_MainPoolDescriptorSpan;
            std::array<int, sizeof...(T_ARGS)>  Indices;

            for( int i=0, c=0, end = static_cast<int>(Desc.size()); i<end; ++i )
            {
                if( Desc[i]->m_Guid.m_Value == SortedDesc[c]->m_Guid.m_Value)
                {
                    Indices[RemapArray[c++]] = i;
                    if( c == static_cast<int>(sizeof...(T_ARGS))) break;
                }
            }

            Callback
            (
                ExtraArgs...
                , Pool
                , ([&]() constexpr noexcept -> T_ARGS
                    {
                        using arg_t = xcore::types::decay_full_t<T_ARGS>;
                        {
                            auto& C = Archetype.m_MainPool.getComponentByIndex<arg_t>( Pool.m_MainPoolIndex, Indices[xcore::types::tuple_t2i_v<T_ARGS, std::tuple<T_ARGS...>>]);
                            if constexpr (std::is_pointer_v<T_ARGS>) return reinterpret_cast<T_ARGS>(&C);
                            else                                     return reinterpret_cast<T_ARGS>(C);
                        }
                    }())...
            );
        }

        //---------------------------------------------------------------------------------
        constexpr std::tuple<int, archetype::tag_sum_guid> FillArray (
                    std::span<const mecs::component::descriptor*> Array
            , const std::span<const mecs::component::descriptor* const> ToAdd
            , const std::span<const mecs::component::descriptor* const> ToSub
            , const std::span<const mecs::component::descriptor* const> Current ) noexcept
        {
            const int AddEnd    = static_cast<int>(ToAdd.size()); 
            const int SubEnd    = static_cast<int>(ToSub.size());
            const int CurEnd    = static_cast<int>(Current.size());
            const int ArrayEnd  = static_cast<int>(Array.size());

            if(SubEnd && AddEnd && CurEnd)
            {
                // Remove all the entries
                int i    = ArrayEnd - 1;
                int iSub = SubEnd - 1;
                int iCur = CurEnd - 1;
                while(iCur > 0)
                {
                    if( ToSub[iSub]->m_Guid.m_Value > Current[iCur]->m_Guid.m_Value )
                    {
                        iSub--;
                        if (iSub < 0 )
                        {
                            while (iCur >=0)
                            {
                                Array[i] = Current[iCur];
                                i--;
                                iCur--;
                            }
                            break;
                        }
                    }
                    else if (Current[iCur]->m_Guid.m_Value == ToSub[iSub]->m_Guid.m_Value)
                    {
                        iCur--;
                        iSub--;
                    }
                    else
                    {
                        Array[i] = Current[iCur];
                        i--;
                        iCur--;
                    }
                }

                // Add all entries
                std::uint64_t Value = 0;
                int    iAdd = 0;
                iCur = i+1;
                i    = 0;
                while( iCur != ArrayEnd )
                {
                    if( ToAdd[iAdd]->m_Guid.m_Value < Array[iCur]->m_Guid.m_Value )
                    {
                        Array[i] = ToAdd[iAdd];
                        i++;
                        iAdd++;
                        if(iAdd == AddEnd)
                        {
                            while(iCur != ArrayEnd)
                            {
                                Array[i] = Array[iCur];
                                Value += Array[i]->m_Guid.m_Value;
                                i++;
                                iCur++;
                            }
                            break;
                        }
                    }
                    else
                    {
                        Array[i] = Array[iCur];
                        Value += Array[i]->m_Guid.m_Value;
                        i++;
                        iCur++;
                    }
                }

                return { i, archetype::tag_sum_guid{Value} };
            }
            else if (AddEnd)
            {
                if (CurEnd) 
                {
                    std::uint64_t Value=0;
                    int iAdd = 0;
                    int iCur = 0;
                    int i    = 0;

                    do 
                    {
                        if (ToAdd[iAdd]->m_Guid.m_Value < Current[iCur]->m_Guid.m_Value)
                        {
                            Array[i] = ToAdd[iAdd];
                            Value += Array[i]->m_Guid.m_Value;
                            i++;
                            iAdd++;
                            if (iAdd == AddEnd)
                            {
                                while (iCur != CurEnd)
                                {
                                    Array[i] = Current[iCur];
                                    Value += Array[i]->m_Guid.m_Value;
                                    i++;
                                    iCur++;
                                }
                                break;
                            }
                        }
                        else
                        {
                            Array[i] = Current[iCur];
                            Value += Array[i]->m_Guid.m_Value;
                            i++;
                            iCur++;
                            if( iCur == CurEnd )
                            {
                                while (iAdd != AddEnd)
                                {
                                    Array[i] = ToAdd[iAdd];
                                    Value += Array[i]->m_Guid.m_Value;
                                    i++;
                                    iAdd++;
                                }
                                break;
                            }
                        }
                    } while (iCur != CurEnd);

                    return { i, archetype::tag_sum_guid{ Value } };
                }
                else
                {
                    std::uint64_t Value = 0;
                    for (int i = 0; i < AddEnd; ++i)
                    {
                        Array[i] = ToAdd[i];
                        Value += Array[i]->m_Guid.m_Value;
                    }
                    return { AddEnd, archetype::tag_sum_guid{ Value } };
                }
            }
            else if(SubEnd)
            {
                if(CurEnd)
                {
                    std::uint64_t Value = 0;
                    int iSub = 0;
                    int iCur = 0;
                    int i    = 0;

                    do
                    {
                        if (Current[iCur]->m_Guid.m_Value > ToSub[iSub]->m_Guid.m_Value )
                        {
                            iSub++;
                            if (iSub == SubEnd)
                            {
                                while (iCur != CurEnd)
                                {
                                    Array[i] = Current[iCur];
                                    Value += Array[i]->m_Guid.m_Value;
                                    i++;
                                    iCur++;
                                }
                                break;
                            }
                        }
                        else if (Current[iCur]->m_Guid.m_Value == ToSub[iSub]->m_Guid.m_Value)
                        {
                            iCur++;
                            iSub++;
                            if (iSub == SubEnd)
                            {
                                while (iCur != CurEnd)
                                {
                                    Array[i] = Current[iCur];
                                    Value += Array[i]->m_Guid.m_Value;
                                    i++;
                                    iCur++;
                                }
                                break;
                            }
                        }
                        else
                        {
                            Array[i] = Current[iCur];
                            Value += Array[i]->m_Guid.m_Value;
                            i++;
                            iCur++;
                        }

                    } while (iCur != CurEnd);

                    return { i, archetype::tag_sum_guid{Value} };
                }
                else
                {
                    return { 0, archetype::tag_sum_guid{ 0ull } };
                }
            }
            else
            {
                std::uint64_t Value = 0;
                for( int i=0; i<CurEnd; ++i )
                {
                    Array[i] = Current[i];
                    Value += Array[i]->m_Guid.m_Value;
                }
                return { CurEnd, archetype::tag_sum_guid{ Value } };
            }
        }
    }

    //---------------------------------------------------------------------------------

    template< typename...T_COMPONENTS >
    constexpr instance& data_base::getOrCreateArchitype( void ) noexcept
    {
        static_assert( ((mecs::component::descriptor_v<T_COMPONENTS>.m_Type < mecs::component::type::ENUM_COUNT) && ...) );
        constexpr static auto DataTuple  = details::getTupleDataComponentsAddEntity ( reinterpret_cast<mecs::component::tuple_data_components <T_COMPONENTS...>*>(nullptr));
        constexpr static auto TagTuple   = details::getTupleTagComponents           ( reinterpret_cast<mecs::component::tuple_tag_components  <T_COMPONENTS...>*>(nullptr));
        constexpr static auto ShareTuple = details::getTupleShareComponents         ( reinterpret_cast<mecs::component::tuple_share_components<T_COMPONENTS...>*>(nullptr));
        return getOrCreateArchitypeDetails
        ( 
            archetype::instance::guid   { std::get<0>(DataTuple).m_Value + std::get<0>(TagTuple).m_Value + std::get<0>(ShareTuple).m_Value }
        ,   archetype::tag_sum_guid     { std::get<0>(TagTuple).m_Value }
        ,                               { std::get<1>(DataTuple).data() , std::get<1>(DataTuple).size()     }
        ,                               { std::get<1>(ShareTuple).data(), std::get<1>(ShareTuple).size()    }
        ,                               { std::get<1>(TagTuple).data()  , std::get<1>(TagTuple).size()      }
        );
    }

    //---------------------------------------------------------------------------------

    template< typename...T_ADD_COMPONENTS_AND_TAGS, typename...T_REMOVE_COMPONENTS_AND_TAGS> constexpr xforceinline
    instance& data_base::getOrCreateGroupBy( instance& OldArchetype, std::tuple<T_ADD_COMPONENTS_AND_TAGS...>*, std::tuple<T_REMOVE_COMPONENTS_AND_TAGS...>* ) noexcept
    {
        // Make sure we are not adding the same component as deleting 
        using remove_tuple = std::tuple<T_REMOVE_COMPONENTS_AND_TAGS...>;
        static_assert((( xcore::types::tuple_has_type_v< T_ADD_COMPONENTS_AND_TAGS, remove_tuple> == false) && ... ));
        static_assert( xcore::types::tuple_has_duplicates_v< std::tuple<T_ADD_COMPONENTS_AND_TAGS...   >> == false );
        static_assert( xcore::types::tuple_has_duplicates_v< std::tuple<T_REMOVE_COMPONENTS_AND_TAGS...>> == false);
        static_assert((( std::is_same_v<T_REMOVE_COMPONENTS_AND_TAGS, mecs::component::entity> == false ) && ... ));
        static_assert((( std::is_same_v<T_ADD_COMPONENTS_AND_TAGS, mecs::component::entity> == false ) && ... ));

        constexpr static auto AddDataTuple  = details::getTupleDataComponents   ( reinterpret_cast<mecs::component::tuple_data_components <T_ADD_COMPONENTS_AND_TAGS...>*>(nullptr));
        constexpr static auto AddTagTuple   = details::getTupleTagComponents    ( reinterpret_cast<mecs::component::tuple_tag_components  <T_ADD_COMPONENTS_AND_TAGS...>*>(nullptr));
        constexpr static auto AddShareTuple = details::getTupleShareComponents  ( reinterpret_cast<mecs::component::tuple_share_components<T_ADD_COMPONENTS_AND_TAGS...>*>(nullptr));
        constexpr static auto SubDataTuple  = details::getTupleDataComponents   ( reinterpret_cast<mecs::component::tuple_data_components <T_REMOVE_COMPONENTS_AND_TAGS...>*>(nullptr));
        constexpr static auto SubTagTuple   = details::getTupleTagComponents    ( reinterpret_cast<mecs::component::tuple_tag_components  <T_REMOVE_COMPONENTS_AND_TAGS...>*>(nullptr));
        constexpr static auto SubShareTuple = details::getTupleShareComponents  ( reinterpret_cast<mecs::component::tuple_share_components<T_REMOVE_COMPONENTS_AND_TAGS...>*>(nullptr));

        //
        // Compute new descriptors
        //
        std::array<const mecs::component::descriptor*, settings::max_data_components_per_entity + 1>  ArrayDataComponents;
        std::array<const mecs::component::descriptor*, settings::max_data_components_per_entity + 1>  ArrayShareComponents;
        std::array<const mecs::component::descriptor*, settings::max_tag_components_per_entity  + 1>  ArrayTagComponents;

        const auto DataTuple  = details::FillArray( ArrayDataComponents,   std::get<1>(AddDataTuple),  std::get<1>(SubDataTuple),  OldArchetype.m_Descriptor.m_DataDescriptorSpan  );
        const auto ShareTuple = details::FillArray( ArrayShareComponents,  std::get<1>(AddShareTuple), std::get<1>(SubShareTuple), OldArchetype.m_Descriptor.m_ShareDescriptorSpan );
        const auto TagTuple   = details::FillArray( ArrayTagComponents,    std::get<1>(AddTagTuple),   std::get<1>(SubTagTuple),   OldArchetype.m_Descriptor.m_TagDescriptorSpan   );

        return getOrCreateArchitypeDetails
        ( 
            archetype::instance::guid{ std::get<1>(DataTuple).m_Value + std::get<1>(TagTuple).m_Value + std::get<1>(ShareTuple).m_Value }
        ,   archetype::tag_sum_guid  { std::get<1>(TagTuple) }
        ,                            { ArrayDataComponents.data() , static_cast<std::size_t>(std::get<0>(DataTuple))     }
        ,                            { ArrayShareComponents.data(), static_cast<std::size_t>(std::get<0>(ShareTuple))    }
        ,                            { ArrayTagComponents.data()  , static_cast<std::size_t>(std::get<0>(TagTuple))      }
        );
    }

    //---------------------------------------------------------------------------------
    inline
    void data_base::Init( void ) noexcept
    {
    }

    //---------------------------------------------------------------------------------
    inline
    void data_base::Start( void ) noexcept
    {
        //
        // Make sure that the scene is ready to go
        // So we are going to loop thought all the archetypes and tell them to get ready
        //
        for (const auto& TagE : std::span{ m_lTagEntries.data(), m_nTagEntries })
        {
            xcore::lock::scope Lk(TagE.m_ArchetypeDB);
            for (auto& ArchetypeEntry : std::span{ TagE.m_ArchetypeDB.get().m_uPtr->data(), TagE.m_ArchetypeDB.get().m_nArchetypes })
            {
                ArchetypeEntry.m_upArchetype->Start();
            }
        }
    }

    //---------------------------------------------------------------------------------

    template< auto& func_sorted_descriptors, auto& func_remap_from_sort >
    static void data_base::DetailsDoQuery( mecs::archetype::query::result_entry& Entry ) noexcept
    {
        const auto&     Archetype                   = *Entry.m_pArchetype;
        const auto&     ArchetypeDataCompDescSpan   = Archetype.m_Descriptor.m_DataDescriptorSpan;
        const auto&     ArchetypeShareCompDescSpan  = Archetype.m_Descriptor.m_ShareDescriptorSpan;
        const int       ArchetypeDataCompSize       = static_cast<int>(ArchetypeDataCompDescSpan.size());
        const int       ArchetypeShareCompSize      = static_cast<int>(ArchetypeShareCompDescSpan.size());
        const int       FunctionParamSize           = Entry.m_nParameters;
        int             iArchetypeShareCompDesc     = 0;
        int             iArchetypeDataCompDesc      = 0;

        std::uint64_t   DoubleBufferDirtyBits       = 0;

        for( int iFunctionSortedCompDesc = 0; iFunctionSortedCompDesc < FunctionParamSize; ++iFunctionSortedCompDesc )
        {
            auto& SortedFunctionComponent = std::get<0>(func_sorted_descriptors[iFunctionSortedCompDesc]);
            if( SortedFunctionComponent.m_Type == mecs::component::type::SHARE )
            {
                if( iArchetypeShareCompDesc < ArchetypeShareCompSize )
                {
                    // Lets find the right share component
                    while( SortedFunctionComponent.m_Guid.m_Value > ArchetypeShareCompDescSpan[iArchetypeShareCompDesc]->m_Guid.m_Value )
                    {
                        iArchetypeShareCompDesc++;
                        xassert( iArchetypeShareCompDesc < ArchetypeShareCompSize );
                    }

                    // If we can not find it then mark it as unfound
                    // TODO: Note that If this component is not a pointer in the function we should assert here
                    if( SortedFunctionComponent.m_Guid != ArchetypeShareCompDescSpan[iArchetypeShareCompDesc]->m_Guid )
                    {
                        Entry.m_lFunctionToArchetype[func_remap_from_sort[iFunctionSortedCompDesc]].m_Index     = mecs::archetype::query::result_entry::invalid_index;
                        Entry.m_lFunctionToArchetype[func_remap_from_sort[iFunctionSortedCompDesc]].m_isShared  = true;
                    }
                    else
                    {
                        // Please note that we need to offset the share component index by one because the very first share component in fact is the pool itself
                        Entry.m_lFunctionToArchetype[func_remap_from_sort[iFunctionSortedCompDesc]].m_Index     = 1 + iArchetypeShareCompDesc;
                        Entry.m_lFunctionToArchetype[func_remap_from_sort[iFunctionSortedCompDesc]].m_isShared  = true;
                    }

                    // Move to the next component
                    iArchetypeShareCompDesc++;
                }
                else
                {
                    // This should be an optional component that was not found
                    Entry.m_lFunctionToArchetype[func_remap_from_sort[iFunctionSortedCompDesc]].m_Index     = mecs::archetype::query::result_entry::invalid_index;
                    Entry.m_lFunctionToArchetype[func_remap_from_sort[iFunctionSortedCompDesc]].m_isShared  = true;
                }
            }
            else if(iArchetypeDataCompDesc < ArchetypeDataCompDescSpan.size() )
            {
                // Lets find the right data component
                while( SortedFunctionComponent.m_Guid.m_Value > ArchetypeDataCompDescSpan[iArchetypeDataCompDesc]->m_Guid.m_Value )
                {
                    iArchetypeDataCompDesc++;
                    xassert(iArchetypeDataCompDesc < ArchetypeDataCompSize);
                }

                // If we can not find it then mark it as unfound
                // TODO: Note that If this component is not a pointer in the function we should assert here
                if( SortedFunctionComponent.m_Guid != ArchetypeDataCompDescSpan[iArchetypeDataCompDesc]->m_Guid )
                {
                    Entry.m_lFunctionToArchetype[func_remap_from_sort[iFunctionSortedCompDesc]].m_Index     = mecs::archetype::query::result_entry::invalid_index;
                    Entry.m_lFunctionToArchetype[func_remap_from_sort[iFunctionSortedCompDesc]].m_isShared  = false;
                }
                // We found our entry, but lets check if we need to handle the double buffer case
                else if( SortedFunctionComponent.m_isDoubleBuffer )
                {
                    // First find the base for this double buffer component
                    int iBase = (iArchetypeDataCompDesc > 0)
                                    ? (ArchetypeDataCompDescSpan[iArchetypeDataCompDesc - 1]->m_Guid == SortedFunctionComponent.m_Guid 
                                        ? (iArchetypeDataCompDesc - 1) 
                                        :  iArchetypeDataCompDesc)
                                    : 0;

                    // If we are a read only parameter then we need to be looking for T0
                    if( std::get<1>(func_sorted_descriptors[iFunctionSortedCompDesc]) )
                    {
                        iBase += static_cast<int>((Archetype.m_DoubleBufferInfo.m_StateBits >> iBase) & 1);
                    }
                    else
                    {
                        // Mark the Double Buffer State as Dirty since we are going to update it
                        // Note that we always change the base component index for the double buffer.
                        DoubleBufferDirtyBits |= (1ull << iBase);

                        // Now ge the new iBase
                        iBase += static_cast<int>(1 - ((Archetype.m_DoubleBufferInfo.m_StateBits >> iBase) & 1));
                    }

                    // Okay ready to set the offsets now
                    Entry.m_lFunctionToArchetype[func_remap_from_sort[iFunctionSortedCompDesc]].m_Index     = iBase;
                    Entry.m_lFunctionToArchetype[func_remap_from_sort[iFunctionSortedCompDesc]].m_isShared  = false;
                }
                else
                {
                    Entry.m_lFunctionToArchetype[func_remap_from_sort[iFunctionSortedCompDesc]].m_Index     = iArchetypeDataCompDesc;
                    Entry.m_lFunctionToArchetype[func_remap_from_sort[iFunctionSortedCompDesc]].m_isShared  = false;
                }

                // Move to the next component
                iArchetypeDataCompDesc++;
            }
            else
            {
                // This should be an optional component that was not found
                Entry.m_lFunctionToArchetype[func_remap_from_sort[iFunctionSortedCompDesc]].m_Index    = mecs::archetype::query::result_entry::invalid_index;
                Entry.m_lFunctionToArchetype[func_remap_from_sort[iFunctionSortedCompDesc]].m_isShared = false;
            }
        }

        // Update the double buffer bits if we got any
        if (DoubleBufferDirtyBits)
        {
            for (auto L = Archetype.m_DoubleBufferInfo.m_DirtyBits.load(std::memory_order_relaxed);
                 false == const_cast<std::atomic<std::uint64_t>&>(Archetype.m_DoubleBufferInfo.m_DirtyBits).compare_exchange_weak(L, L | DoubleBufferDirtyBits);
                );
        }
    }

    //---------------------------------------------------------------------------------

    template<typename T_FUNCTION, typename T_TUPLE_QUERY > constexpr xforceinline
    void data_base::DoQuery( query::instance& Instance ) const noexcept
    {
        static constexpr auto query_v = mecs::archetype::query::details::define< T_TUPLE_QUERY >{};
        struct functor { void operator()() noexcept {} };
        DetailsDoQuery< std::conditional_t< std::is_same_v< void, T_FUNCTION>, functor, T_FUNCTION>, query_v>(Instance);
    }

    //---------------------------------------------------------------------------------

    template<typename T_FUNCTION, auto& Defined > constexpr xforceinline
    void data_base::DetailsDoQuery( query::instance& Instance ) const noexcept
    {
        static_assert(std::is_convertible_v< decltype(Defined), query::details::define_data >);

        XCORE_PERF_ZONE_SCOPED()

        if( Instance.m_isInitialized == false )
        {
            Instance.Initialize<T_FUNCTION, Defined>();
        }

        using                   func_tuple              = typename xcore::function::traits<T_FUNCTION>::args_tuple;
        static constexpr auto   func_descriptors        = mecs::archetype::query::details::get_arrays< func_tuple >::value;
        using                   func_sorted_tuple       = xcore::types::tuple_sort_t< mecs::component::smaller_guid, func_tuple >;
        static constexpr auto   func_sorted_descriptors = mecs::archetype::query::details::get_arrays< func_sorted_tuple >::value;
        static constexpr auto   func_remap_from_sort    = mecs::archetype::query::details::remap_arrays< func_tuple, func_sorted_tuple >::value;

        //
        // Do query
        //
        Instance.m_lResults.clear();
        for( const auto& TagE : std::span{ m_lTagEntries.data(), m_nTagEntries} )
        {
            if( false == TagE.m_TagBits.Query( Instance.m_TagQuery.m_All, Instance.m_TagQuery.m_Any, Instance.m_TagQuery.m_None ) ) continue;

            xcore::lock::scope Lk( std::as_const(TagE.m_ArchetypeDB) );
            for( auto& ArchetypeE : std::span{ TagE.m_ArchetypeDB.get().m_uPtr->data(), TagE.m_ArchetypeDB.get().m_nArchetypes } )
            {
                if( false == ArchetypeE.m_ArchitypeBits.Query( Instance.m_ComponentQuery.m_All, Instance.m_ComponentQuery.m_Any, Instance.m_ComponentQuery.m_None ) ) continue;
                if( ArchetypeE.m_upArchetype->m_MainPool.size() == 0 ) continue;

                //
                // Create new entry for this query
                //
                auto&           Entry                       = Instance.m_lResults.append();
                Entry.m_pArchetype                          = ArchetypeE.m_upArchetype.get();
                Entry.m_nParameters                         = static_cast<std::uint8_t>(std::tuple_size_v<func_tuple>);
                Entry.m_FunctionDescriptors                 = func_descriptors;

                DetailsDoQuery<func_sorted_descriptors, func_remap_from_sort>(Entry);
            }
        }
    }

    /*
    instance& getOrCreateArchitype(const std::span<const mecs::component::descriptor*> UnsortedDiscriptorList) noexcept
    {
        xassert(UnsortedDiscriptorList.size() < (mecs::settings::max_components_per_entity - 1));

        std::array<const mecs::component::descriptor*, mecs::settings::max_data_components_per_entity> DataDiscriptorList;
        std::array<const mecs::component::descriptor*, mecs::settings::max_data_components_per_entity> ShareDiscriptorList;
        std::array<const mecs::component::descriptor*, mecs::settings::max_data_components_per_entity> TagDiscriptorList;

        // Validate and copy the list
        std::uint32_t   iShare = 0;
        std::uint32_t   iTag   = 0;
        std::uint32_t   iData  = 1;
        mecs::archetype::instance::guid     Guid        { mecs::component::entity::type_guid_v.m_Value };
        mecs::archetype::tag_sum_guid       TagSumGuid  { 0ull };
        DataDiscriptorList[0] = &mecs::component::descriptor_v<mecs::component::entity>;
        for (auto& pE : UnsortedDiscriptorList)
        {
            xassert(pE->m_Guid.isValid());
            xassert(pE->m_Guid.m_Value != mecs::component::entity::type_guid_v.m_Value);
            xassert(pE->m_Type < mecs::component::type::ENUM_COUNT);

            if(pE->m_Type == mecs::component::type::TAG)
            {
                TagDiscriptorList[iTag++] = pE;
                TagSumGuid.m_Value += pE->m_Guid.m_Value;
            }
            else if(pE->m_Type == mecs::component::type::SHARE)
            {
                ShareDiscriptorList[iShare++] = pE;
                Guid.m_Value += pE->m_Guid.m_Value;
            }
            else
            {
                DataDiscriptorList[iData++] = pE;
                if (pE->m_isDoubleBuffer) DataDiscriptorList[iData++] = pE;
                Guid.m_Value += pE->m_Guid.m_Value;
            }
        }

        Guid.m_Value += TagSumGuid.m_Value;

        if(iData) std::sort(&DataDiscriptorList[0], &DataDiscriptorList[iData], [](const mecs::component::descriptor* a, const mecs::component::descriptor* b) constexpr
        {
            return a->m_Guid.m_Value < b->m_Guid.m_Value;
        });

        if(iShare)
        {
            if( iData >= 2 && DataDiscriptorList[1]->m_Guid == mecs::entity_pool::component::type_guid_v )
            {
                for( std::uint32_t i=0;i<iShare;i++)
                {
                    DataDiscriptorList[iData++] = ShareDiscriptorList[i];
                }
                std::sort(&DataDiscriptorList[0], &DataDiscriptorList[iData], [](const mecs::component::descriptor* a, const mecs::component::descriptor* b) constexpr
                {
                    return a->m_Guid.m_Value < b->m_Guid.m_Value;
                });
                iShare = 0;
            }
            else
            {
                std::sort(&ShareDiscriptorList[0], &ShareDiscriptorList[iShare], [](const mecs::component::descriptor* a, const mecs::component::descriptor* b) constexpr
                {
                    return a->m_Guid.m_Value < b->m_Guid.m_Value;
                });
            }
        }

        if(iTag) std::sort(&TagDiscriptorList[0], &TagDiscriptorList[iTag], [](const mecs::component::descriptor* a, const mecs::component::descriptor* b) constexpr
        {
            return a->m_Guid.m_Value < b->m_Guid.m_Value;
        });

        return getOrCreateArchitypeDetails(
                Guid
            ,   TagSumGuid
            ,   std::span{ DataDiscriptorList.data(),  iData  }
            ,   std::span{ ShareDiscriptorList.data(), iShare }
            ,   std::span{ TagDiscriptorList.data(),   iTag   }
            );
    }
    */

    //---------------------------------------------------------------------------------

    xforceinline
    instance& data_base::getArchetype( mecs::archetype::instance::guid Guid ) noexcept
    {
        return *m_mapArchetypes.get(Guid);
    }

    //---------------------------------------------------------------------------------

    xforceinline
    instance& data_base::getOrCreateArchitypeDetails(
          mecs::archetype::instance::guid                       Guid
        , mecs::archetype::tag_sum_guid                         TagSumGuid
        , std::span<const mecs::component::descriptor* const>   DataDiscriptorList
        , std::span<const mecs::component::descriptor* const>   ShareDiscriptorList
        , std::span<const mecs::component::descriptor* const>   TagDiscriptorList ) noexcept
    {
        mecs::archetype::instance* p;
        m_mapArchetypes.getOrCreate( Guid
        , [&]( auto& pInstance ) constexpr noexcept
        {
            p = pInstance;
        }
        , [&]( auto& pInstance ) constexpr noexcept
        {
            details::tag_entry* pt;
            const auto TagGuid = TagSumGuid.isValid() ? TagSumGuid : mecs::archetype::tag_sum_guid{1};

            m_mapTags.getOrCreate(TagGuid
            , [&]( details::tag_entry*& pEntry ) constexpr noexcept
            {
                pt = pEntry;
            }
            , [&]( details::tag_entry*& pEntry ) constexpr noexcept
            {
                pt = pEntry = &m_lTagEntries[m_nTagEntries++];
                if(TagDiscriptorList.size())
                {
                    pEntry->m_TagDescriptors.New(TagDiscriptorList.size()).CheckError();
                    int i = 0;
                    for (auto& e : TagDiscriptorList)
                    {
                        pEntry->m_TagBits.AddBit(e->m_BitNumber);
                        pEntry->m_TagDescriptors[i++] = e;
                    }
                }
            });

            xcore::lock::scope  Lk(pt->m_ArchetypeDB);
            auto&               ArchetypeDB = pt->m_ArchetypeDB.get();
            auto&               E           = (*ArchetypeDB.m_uPtr)[ArchetypeDB.m_nArchetypes++];

            for (auto& e : DataDiscriptorList ) E.m_ArchitypeBits.AddBit(e->m_BitNumber);
            for (auto& e : ShareDiscriptorList) E.m_ArchitypeBits.AddBit(e->m_BitNumber);
            E.m_upArchetype = std::make_unique<mecs::archetype::instance>();
            E.m_upArchetype->Init(DataDiscriptorList, ShareDiscriptorList, pt->m_TagDescriptors, &m_EntityMap );
            E.m_upArchetype->m_Guid = Guid;
            E.m_upArchetype->m_ArchitypeBits = E.m_ArchitypeBits;
            E.m_upArchetype->m_TagBits       = pt->m_TagBits;
            p = pInstance = E.m_upArchetype.get();
            if(m_Event.m_CreatedArchetype.hasSubscribers()) 
                m_Event.m_CreatedArchetype.NotifyAll(*E.m_upArchetype);
        });

        return *p;
    }

    //---------------------------------------------------------------------------------

    template< typename T_FUNCTION >
    void data_base::InitializeQuery( mecs::archetype::query::instance& QueryInstance ) noexcept
    {
        using funcition = T_FUNCTION;

        if( QueryInstance.m_isInitialized ) return;
        QueryInstance.m_isInitialized = true;
    }

    //---------------------------------------------------------------------------------

    template< typename T_CALLBACK, typename...T_SHARE_COMPONENTS >
    void data_base::moveEntityToArchetype( 
        system::instance&           System
      , mecs::component::entity&    OldEntity
      , archetype::instance&        NewArchetype
      , T_CALLBACK&&                Callback
      , T_SHARE_COMPONENTS&&...     ShareComponents ) noexcept
    {
        XCORE_PERF_ZONE_SCOPED()

        // This entity has been mark for deletion.... there is nothing we can accomplish here 
        xassert( OldEntity.isZombie() == false );

                auto& OldMapEntry  = OldEntity.getMapEntry();
                auto& OldSpecific  = *OldMapEntry.m_Value.m_pPool;
                auto& OldArchetype = *OldSpecific.m_pArchetypeInstance;
        const   auto  OldIndex     = OldMapEntry.m_Value.m_Index;
        const   auto  EntityGuid   = OldMapEntry.m_Key;

        // If we are going to move it to the same group there is nothing to do
        if( &NewArchetype == &OldArchetype ) return;

        // Let anyone that is interested know
        if (OldArchetype.m_Events.m_MovedOutEntity.hasSubscribers())
            OldArchetype.m_Events.m_MovedOutEntity.NotifyAll(OldEntity, System);

        // Mark the old entity as a zombie now... since it is about to move
        OldEntity.MarkAsZombie();

        auto& NewDescriptor = NewArchetype.m_Descriptor;
        auto& OldDescriptor = OldArchetype.m_Descriptor;

        xassert(NewDescriptor.m_ShareDescriptorSpan.size() == sizeof...(T_SHARE_COMPONENTS) );

        const auto ShareKeys = [&]() constexpr noexcept
        {
           if constexpr ( !!sizeof...(T_SHARE_COMPONENTS) ) return std::array{ component::descriptor_v<T_SHARE_COMPONENTS>.m_fnGetKey( &ShareComponents ) ... };
           else                                             return std::array<std::uint64_t,0>{};
        }();

        //
        // Allocate the entity in the new pool
        //
        std::uint32_t       Index        = ~0u;
        specialized_pool*   pSpecialized = nullptr;
        if(!!sizeof...(T_SHARE_COMPONENTS))
        {
            auto& MainPool = NewArchetype.m_MainPool;
            auto  FindPool = [&]
            {
                for (int i = 0, end = static_cast<int>(NewArchetype.m_MainPool.size()); i != end; ++i)
                {
                    auto& Specialized = MainPool.getComponentByIndex<specialized_pool>(i,0);
                    bool  bCompatible = true;
                    for( int k=0, end2 = static_cast<int>(Specialized.m_ShareComponentKeysSpan.size()); k != end2; ++k )
                    {
                        if( Specialized.m_ShareComponentKeysSpan[k] != ShareKeys[k] )
                        {
                            bCompatible = false;
                            break;
                        }
                    }

                    if(bCompatible == false )
                        continue;

                    Index = Specialized.m_EntityPool.append();
                    if (Index != ~0u) 
                    {
                        pSpecialized = &Specialized;
                        break;
                    }
                }
            };

            FindPool();
            if(pSpecialized == nullptr)
            {
                xcore::lock::scope Lk( NewArchetype.m_SemaphoreLock );
                FindPool();
                if (pSpecialized == nullptr)
                {
                    using sorter_tuple = xcore::types::tuple_sort_t<mecs::component::smaller_guid, std::tuple<T_SHARE_COMPONENTS ...>>;

                    auto& MainPool = NewArchetype.m_MainPool;
                    Index = MainPool.append(1);

                    (
                       ( mecs::component::descriptor_v<T_SHARE_COMPONENTS>.m_fnMove
                            ( &MainPool.getComponentByIndex<T_SHARE_COMPONENTS>( Index, 1 + xcore::types::tuple_t2i_v<T_SHARE_COMPONENTS, sorter_tuple > )
                            , &ShareComponents )
                       )
                       , ... 
                    );

                    auto& Specialized = MainPool.getComponentByIndex<specialized_pool>(Index, 0);
                    Specialized.m_pArchetypeInstance        = &NewArchetype;
                    Specialized.m_TypeGuid.m_Value          = 0u;
                    Specialized.m_MainPoolIndex             = Index;
                    Specialized.m_ShareComponentKeysSpan    = std::span{ Specialized.m_ShareComponentKeysMemory.data(), sizeof...(T_SHARE_COMPONENTS) };
                    Specialized.m_EntityPool.Init( NewArchetype, NewArchetype.m_Descriptor.m_DataDescriptorSpan, settings::max_default_entities_per_pool );
                    for (int k = 0, end2 = static_cast<int>(Specialized.m_ShareComponentKeysSpan.size()); k != end2; ++k) 
                        Specialized.m_ShareComponentKeysMemory[k] = ShareKeys[k];

                    if( NewArchetype.m_Events.m_CreatedPool.hasSubscribers() )
                        NewArchetype.m_Events.m_CreatedPool.NotifyAll( System, Specialized );

                    pSpecialized = &Specialized;
                    Index = pSpecialized->m_EntityPool.append(1);

                    // Publish our archetype
                    MainPool.MemoryBarrier();
                }
            }
        }
        else
        {
            auto& MainPool = NewArchetype.m_MainPool;
            auto  FindPool = [&]
            {
                for (int i = 0, end = static_cast<int>(NewArchetype.m_MainPool.size()); i != end; ++i)
                {
                    auto& Specialized = MainPool.getComponentByIndex<specialized_pool>(i, 0);
                    Index = Specialized.m_EntityPool.append();
                    if (Index != ~0)
                    {
                        pSpecialized = &Specialized;
                        break;
                    }
                }
            };

            FindPool();
            if (pSpecialized == nullptr)
            {
                xcore::lock::scope Lk( NewArchetype.m_SemaphoreLock );
                FindPool();
                if (pSpecialized == nullptr)
                {
                    auto& MainPool = NewArchetype.m_MainPool;
                    Index = MainPool.append(1);

                    auto& Specialized = MainPool.getComponentByIndex<specialized_pool>(Index, 0);
                    Specialized.m_pArchetypeInstance        = &NewArchetype;
                    Specialized.m_TypeGuid.m_Value          = 0u;
                    Specialized.m_MainPoolIndex             = Index;
                    Specialized.m_EntityPool.Init( NewArchetype, NewArchetype.m_Descriptor.m_DataDescriptorSpan, settings::max_default_entities_per_pool );

                    if (NewArchetype.m_Events.m_CreatedPool.hasSubscribers())
                        NewArchetype.m_Events.m_CreatedPool.NotifyAll(System, Specialized);

                    pSpecialized = &Specialized;
                    Index        = pSpecialized->m_EntityPool.append(1);

                    // Publish our archetype
                    MainPool.MemoryBarrier();
                }
            }
        }

        //
        // Move components to the new pool
        //
        m_EntityMap.find( EntityGuid, [&]( component::entity::reference& Value )
        {
            //
            // Move all the components to the new entry
            //
            const int NewEnd = static_cast<int>(NewDescriptor.m_DataDescriptorSpan.size());
            const int OldEnd = static_cast<int>(OldDescriptor.m_DataDescriptorSpan.size());
            int iNew = 1, iOld = 1; // We can skip the entity so start at 1
            while( iOld != OldEnd && iNew != NewEnd )
            {
                const auto& DescNew = *NewDescriptor.m_DataDescriptorSpan[iNew];
                const auto& DescOld = *OldDescriptor.m_DataDescriptorSpan[iOld];
                if(DescNew.m_Guid.m_Value > DescOld.m_Guid.m_Value )
                {
                    ++iOld;
                }
                else if (DescNew.m_Guid.m_Value < DescOld.m_Guid.m_Value)
                {
                    iNew++;
                }
                else
                {
                    if(DescNew.m_isDoubleBuffer )
                    {
                        if( (NewArchetype.m_DoubleBufferInfo.m_StateBits ^ OldArchetype.m_DoubleBufferInfo.m_StateBits) & (1ull << DescNew.m_BitNumber) )
                        {
                            const auto nNew = static_cast<int>((NewArchetype.m_DoubleBufferInfo.m_StateBits >> DescNew.m_BitNumber)&1);
                            const auto nOld = static_cast<int>((OldArchetype.m_DoubleBufferInfo.m_StateBits >> DescNew.m_BitNumber)&1);

                            DescNew.m_fnMove(
                                pSpecialized->m_EntityPool.getComponentByIndexRaw(Index,    iNew + nNew)
                                , OldSpecific.m_EntityPool.getComponentByIndexRaw(OldIndex, iOld + nOld));

                            DescNew.m_fnMove(
                                pSpecialized->m_EntityPool.getComponentByIndexRaw(Index,    iNew + (1-nNew))
                                , OldSpecific.m_EntityPool.getComponentByIndexRaw(OldIndex, iOld + (1-nOld)));

                            iNew += 2;
                            iOld += 2;
                        }
                        else
                        {
                            DescNew.m_fnMove(
                                pSpecialized->m_EntityPool.getComponentByIndexRaw(Index,    iNew)
                                , OldSpecific.m_EntityPool.getComponentByIndexRaw(OldIndex, iOld));

                            ++iNew;
                            ++iOld;

                            DescNew.m_fnMove(
                                pSpecialized->m_EntityPool.getComponentByIndexRaw(Index,    iNew)
                                , OldSpecific.m_EntityPool.getComponentByIndexRaw(OldIndex, iOld));

                            ++iNew;
                            ++iOld;
                        }
                    }
                    else
                    {
                        DescNew.m_fnMove(
                            pSpecialized->m_EntityPool.getComponentByIndexRaw(Index,    iNew)
                            , OldSpecific.m_EntityPool.getComponentByIndexRaw(OldIndex, iOld));

                        ++iNew;
                        ++iOld;
                    }
                }
            }

            //
            // Update entity with new data
            //
            auto& NewEntity = pSpecialized->m_EntityPool.getComponentByIndex<component::entity>( Index, 0 );
            NewEntity.m_pInstance                   = &OldMapEntry;
            NewEntity.m_pInstance->m_Value.m_pPool  = pSpecialized;
            NewEntity.m_pInstance->m_Value.m_Index  = Index;

            //
            // Call the user callback
            //
            if constexpr( std::is_same_v< T_CALLBACK, void(*)() > == false )
            {
                details::CallFunction( NewEntity.m_pInstance->m_Value, Callback, reinterpret_cast<typename xcore::function::traits<T_CALLBACK>::args_tuple*>(nullptr) );
            }

            //
            // Notify whoever is interested
            //
            if (NewArchetype.m_Events.m_MovedInEntity.hasSubscribers())
                NewArchetype.m_Events.m_MovedInEntity.NotifyAll(NewEntity, System);
        });

        //
        // Lets tell the system to delete the old component
        //
        OldSpecific.m_EntityPool.deleteBySwap( OldIndex );
    }

    //----------------------------------------------------------------------------------------------------
    // ARCHETYPE QUERY
    //----------------------------------------------------------------------------------------------------
    namespace query
    {
        inline
        bool instance::TryAppendArchetype(mecs::archetype::instance& Archetype) noexcept
        {
            if (false == Archetype.m_ArchitypeBits.Query(m_ComponentQuery.m_All, m_ComponentQuery.m_Any, m_ComponentQuery.m_None)) return false;
            if (false == Archetype.m_ArchitypeBits.Query(m_TagQuery.m_All,       m_TagQuery.m_Any,       m_TagQuery.m_None)      ) return false;

            auto& R = m_lResults.append();
            R.m_pArchetype  = &Archetype;
            R.m_nParameters = 0;
            return true;
        }
    }
}