namespace mecs::archetype
{
    // virtual page remap - https://stackoverflow.com/questions/17197615/no-mremap-for-windows

    //---------------------------------------------------------------------------------
    // ARCHETYPE:: ....
    //---------------------------------------------------------------------------------

    using tag_sum_guid = xcore::guid::unit<64, struct tag_sum_tag>;

    enum class order : std::uint8_t
    {
            NONE
        ,   HASH                // Hash based on crc64 of the component, multiple share components will be convined to create the unique HASH key
        ,   SORTED              // Sorted base on the crc64 of the component, multiple share components will be different keys
    };

    //----------------------------------------------------------------------------------------------
    // ARCHETYPE::DESCRIPTOR
    //----------------------------------------------------------------------------------------------
    struct descriptor
    {
        using ptr_array = xcore::array<const mecs::component::descriptor*, mecs::settings::max_data_components_per_entity>;

        void Init( const std::span<const mecs::component::descriptor* const> DataDescriptorList
                 , const std::span<const mecs::component::descriptor* const> ShareDescriptorList
                 , const std::span<const mecs::component::descriptor* const> TagDescriptorList   ) noexcept
        {
            xassert(DataDescriptorList.size());

            int i = 0;
            for (auto& e : DataDescriptorList) m_DataDescriptors[i++] = e;
            int j = 0;
            for (auto& e : ShareDescriptorList) m_ShareDescriptors[j++] = e;

            m_TagDescriptorSpan   = TagDescriptorList;
            m_DataDescriptorSpan  = std::span{ m_DataDescriptors.data(),  static_cast<std::size_t>(i)};
            m_ShareDescriptorSpan = std::span{ m_ShareDescriptors.data(), static_cast<std::size_t>(j)};
        }

        std::span<const mecs::component::descriptor* const>     m_DataDescriptorSpan    {};
        std::span<const mecs::component::descriptor* const>     m_ShareDescriptorSpan   {};
        std::span<const mecs::component::descriptor* const>     m_TagDescriptorSpan     {};
        ptr_array                                               m_DataDescriptors       {};
        ptr_array                                               m_ShareDescriptors      {};
    };


    //----------------------------------------------------------------------------------------------
    // ARCHETYPE:: SAFERY
    // Used to detect when two systems are conflicting with each other.
    // Safety should be used only when in editor, in final release it should be removed.
    //----------------------------------------------------------------------------------------------
    namespace details
    {
        struct safety
        {
            struct component_lock_entry
            {
                system::instance*           m_pWritingSystem    { nullptr };        // Which system has this component locked for writing
                std::uint8_t                m_nReaders          { 0 };              // How many systems have this group locked for reading
            };

            using component_list_t = std::array<component_lock_entry, mecs::settings::max_data_components_per_entity>;

            static bool LockQueryComponents     ( system::instance& System, mecs::archetype::query::instance& Query ) noexcept;
            static bool UnlockQueryComponents   ( system::instance& System, mecs::archetype::query::instance& Query ) noexcept;

            std::uint8_t        m_nSharerWriters    {0};
            component_list_t    m_lComponentLocks   {};
        };

        //-----------------------------------------------------------------------------------------
        template< typename...T_ADD_ARGS1, typename...T_SUBTRACT_ARGS2 >
        static constexpr xforceinline auto ComputeArchetypeGuid( std::uint64_t Value, std::tuple<T_ADD_ARGS1...>*, std::tuple<T_SUBTRACT_ARGS2...>* ) noexcept
        {
            if constexpr ( sizeof...(T_ADD_ARGS1) && sizeof...(T_SUBTRACT_ARGS2) )
            {
                return Value 
                    + (( 0ull + mecs::component::descriptor_v<T_ADD_ARGS1>.m_Guid.m_Value ) + ... )
                    - (( 0ull + mecs::component::descriptor_v<T_SUBTRACT_ARGS2>.m_Guid.m_Value ) + ... );
            }
            else if constexpr ( sizeof...(T_ADD_ARGS1) )
            {
                return Value 
                    + (( 0ull + mecs::component::descriptor_v<T_ADD_ARGS1>.m_Guid.m_Value ) + ... );
            
            }
            else if constexpr ( sizeof...(T_SUBTRACT_ARGS2) )
            {
                return Value 
                    - (( 0ull + mecs::component::descriptor_v<T_SUBTRACT_ARGS2>.m_Guid.m_Value ) + ... );
            }
            else
            {
                return Value;
            }
        }
    }

    //----------------------------------------------------------------------------------------------
    // ARCHETYPE:: SPECIALIZED
    // Specialized archetypes are used to create the actual entities.
    // It is call specialized because given a set of share components their unique values
    // represent a "unique" or specialized archetype.
    // This archetypes are what can't be share across systems.
    //----------------------------------------------------------------------------------------------
    struct specialized_pool : mecs::component::singleton
    {
        constexpr static auto           type_guid_v             = mecs::component::type_guid{ 2ull };
        constexpr static auto           name_v                  = xconst_universal_str("mecs::archetype::specialized");
        using                           guid                    = xcore::guid::unit<64, struct specialized_pool_tag>;
        using                           type_guid               = xcore::guid::unit<64, struct specialized_pool_type_tag>;
        using                           share_component_keys    = std::array<std::uint64_t, mecs::settings::max_data_components_per_entity>;

        type_guid                       m_TypeGuid                  {};
        instance*                       m_pArchetypeInstance        {};
        mecs::entity_pool::instance     m_EntityPool                {};
        std::span<std::uint64_t>        m_ShareComponentKeysSpan    {};
        share_component_keys            m_ShareComponentKeysMemory  {};
        specialized_pool*               m_pNext                     {nullptr};
    };

    //----------------------------------------------------------------------------------------------
    // ARCHETYPE:: INSTANCE
    // m_MainPool< Entity, Pool<E,C...>, specialized, SC... >
    //----------------------------------------------------------------------------------------------
    /*
    template< typename...T_COMPONENTS >
    struct call_details;

    template< typename...T_COMPONENTS >
    struct call_details< std::tuple<T_COMPONENTS...> >
    {
        using tuple_share_components = xcore::types::tuple_sort_t
        <
              mecs::component::smaller_guid
            , mecs::component::tuple_share_components<xcore::types::decay_full_t<T_COMPONENTS>...>
        >;

        using tuple_data_components = xcore::types::tuple_sort_t
        <
            mecs::component::smaller_guid
            , mecs::component::tuple_data_components<xcore::types::decay_full_t<T_COMPONENTS>...>
        >;
    };

    template
    < 
        typename    T_CALLBACK
    ,   typename... T_CALL_COMPONENTS
    ,   typename... T_DATA_COMPONENTS
    ,   typename... T_SHARE_COMPONENTS
    >
    void CreateEntity(
            mecs::entity_pool::instance& MainPool
        ,   mecs::entity_pool::index     MainPool_Index
        ,   mecs::entity_pool::instance& EntityPool
        ,   mecs::entity_pool::index     EntityPool_Index
        ,   T_CALLBACK&& Callback
        ,   std::tuple<T_CALL_COMPONENTS...>*
        ,   std::tuple<T_DATA_COMPONENTS...>*
        ,   std::tuple<T_SHARE_COMPONENTS...>* ) noexcept
    {
        if constexpr ( sizeof...(T_SHARE_COMPONENTS) && sizeof...(T_DATA_COMPONENTS) )
        {
            std::tuple ComponentsOrder =
            {
                  MainPool.getComponentByIndex< T_SHARE_COMPONENTS>(MainPool_Index,    1 + xcore::types::tuple_t2i_v<T_SHARE_COMPONENTS, std::tuple<T_SHARE_COMPONENTS...>> ) ...
                , EntityPool.getComponentByIndex<T_DATA_COMPONENTS>(EntityPool_Index,  1 + xcore::types::tuple_t2i_v<T_DATA_COMPONENTS,  std::tuple<T_DATA_COMPONENTS...>> ) ...
            };
            
            Callback( std::get<T_CALL_COMPONENTS>(ComponentsOrder) ... );
        }
        else if constexpr (sizeof...(T_SHARE_COMPONENTS) && 0 == sizeof...(T_DATA_COMPONENTS))
        {
            std::tuple ComponentsOrder =
            {
                MainPool.getComponentByIndex<T_SHARE_COMPONENTS>(MainPool_Index, 1 + xcore::types::tuple_t2i_v < T_SHARE_COMPONENTS, std::tuple<T_SHARE_COMPONENTS...>>) ...
            };

            Callback(std::get<T_CALL_COMPONENTS>(ComponentsOrder) ...);
        }
        else if constexpr (0 == sizeof...(T_SHARE_COMPONENTS) && sizeof...(T_DATA_COMPONENTS))
        {
            std::tuple ComponentsOrder =
            {
                EntityPool.getComponentByIndex<T_DATA_COMPONENTS>(EntityPool_Index, 1 + xcore::types::tuple_t2i_v < T_DATA_COMPONENTS, std::tuple<T_DATA_COMPONENTS...>>) ...
            };

            Callback( std::get<T_CALL_COMPONENTS>(ComponentsOrder) ... );
        }
    }
    */

    struct entity_creation
    {
        static constexpr auto max_entries_single_thread = 10000;

        std::span<mecs::component::entity::guid>    m_Guids;
        mecs::component::entity::map*               m_pEntityMap;
        specialized_pool*                           m_pPool;
        mecs::entity_pool::index                    m_StartIndex;
        int                                         m_Count;
        mecs::system::instance&                     m_System;
        mecs::archetype::instance&                  m_Archetype;

        constexpr bool isValid( void ) const noexcept;
        template< typename T >
        void operator() ( T&& Callback ) noexcept;

        inline
        void operator() (void) noexcept;

        template< typename...T_COMPONENTS > xforceinline
        constexpr void getTrueComponentIndices( std::span<std::uint8_t> Array, std::span<const mecs::component::descriptor* const> Span, std::tuple<T_COMPONENTS...>* ) const noexcept;

        template< typename T, typename...T_COMPONENTS > xforceinline
        void Initialize( T&& Callback, std::tuple<T_COMPONENTS...>* );
    };


    struct instance
    {
        using guid = xcore::guid::unit<64, struct archetype_tag>;

        void Init(  const std::span<const mecs::component::descriptor* const>   DataDescriptorList
                ,   const std::span<const mecs::component::descriptor* const>   ShareDescriptorList
                ,   const std::span<const mecs::component::descriptor* const>   TagDescriptorList
                ,   mecs::component::entity::map*                               pEntityMap ) noexcept
        {
            m_Descriptor.Init(DataDescriptorList, ShareDescriptorList, TagDescriptorList );
            m_MainPoolDescriptorData[0] = &mecs::component::descriptor_v<specialized_pool>;
            m_MainPoolDescriptorSpan    = std::span{ m_MainPoolDescriptorData.data(), ShareDescriptorList.size() + 1 };
            m_pEntityMap                = pEntityMap;

            if( ShareDescriptorList.size() )
            {
                for( int i=0; i< ShareDescriptorList.size(); ++ i) m_MainPoolDescriptorData[1+i] = ShareDescriptorList[i];
            }

            m_MainPool.Init( *this, m_MainPoolDescriptorSpan, 100000u );
        }

        constexpr xforceinline auto& getGuid( void ) const noexcept { return m_Guid; }

        void MemoryBarrierSync( mecs::sync_point::instance& SyncPoint ) noexcept
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

        void Start( void ) noexcept 
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

        template< typename...T_SHARE_COMPONENTS >
        entity_creation CreateEntities( mecs::system::instance& Instance, const int nEntities, std::span<mecs::component::entity::guid> Guids, T_SHARE_COMPONENTS&&...ShareComponents ) noexcept
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

                const specialized_pool::type_guid SpecializedGuidTypeGuid{ mecs::tools::hash::combine( ShareComponentKeys[xcore::types::tuple_t2i_v<T_SHARE_COMPONENTS, share_tuple>] ...) };

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
                Specialized.m_EntityPool.Init( *this, m_Descriptor.m_DataDescriptorSpan, std::max<int>(nEntities, 100000 /*MaxEntries*/ ));
                Specialized.m_pArchetypeInstance = this;
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

        template< typename T_CALLBACK = void(*)(), typename...T_SHARE_COMPONENTS > xforceinline
        void CreateEntity( mecs::system::instance& Instance, T_CALLBACK&& CallBack = []{}, T_SHARE_COMPONENTS&&...ShareComponents) noexcept
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

        inline void deleteEntity( mecs::system::instance& System, mecs::component::entity& Entity ) noexcept;

/*
        template< typename T, typename...T_SHARE_COMPONENTS > xforceinline 
        void CreateEntity(mecs::component::entity::guid Guid, T&& CallBack, T_SHARE_COMPONENTS&&...ShareComponents) noexcept
        {
            std::array GuidList { Guid };
            CreateEntities( 1, GuidList, std::forward<T_SHARE_COMPONENTS>(ShareComponents) ... )(CallBack);
        }
*/

        template< typename...T_SHARE_COMPONENTS >
        constexpr specialized_pool::type_guid getSpecializedPoolGuid( const instance& Instance, T_SHARE_COMPONENTS&&...ShareComponents ) const noexcept
        {
            static_assert(mecs::component::descriptor_v<T_SHARE_COMPONENTS>.m_Type == mecs::component::type::SHARE);
            xassert( sizeof...(T_SHARE_COMPONENTS) == Instance.m_Descriptor.m_ShareDescriptorSpan.size() );

            // Compute the keys and store them in the array
            // using sorted_tuple = xcore::types::tuple_sort_t< mecs::component::smaller_guid, std::tuple<T_SHARE_COMPONENTS...> >;
            //std::array<std::uint64_t,sizeof...(T_SHARE_COMPONENTS)> ShareComponentKeys {};
            //((ShareComponentKeys[xcore::types::tuple_t2i_v<T_SHARE_COMPONENTS, sorted_tuple>] = mecs::component::descriptor_v<T_SHARE_COMPONENTS>.m_fn_getKey(&ShareComponents)) , ... );

            return{ (mecs::component::descriptor_v<T_SHARE_COMPONENTS>.m_fn_getKey(&ShareComponents) + ... ) };
        }

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

        struct double_buffer_info
        {
            static_assert(mecs::settings::max_data_components_per_entity <= 64);
            std::uint64_t               m_StateBits{ 0 };           // Tells which of the two columns is T0. Index is same as the component.
            std::atomic<std::uint64_t>  m_DirtyBits{ 0 };           // Tells which bits in the StateBits need to change state.
        };

        using main_pool_descriptors = std::array<const mecs::component::descriptor*, mecs::settings::max_data_components_per_entity>;
        using safety_lock_object    = xcore::lock::object<details::safety, xcore::lock::semaphore >;

        mecs::component::entity::map*                   m_pEntityMap                { nullptr };

        descriptor                                      m_Descriptor                {};
        std::span<const mecs::component::descriptor*>   m_MainPoolDescriptorSpan    {};
        mecs::entity_pool::instance                     m_MainPool                  {};     // this pool should be sorted by its share components values
                                                                                            // component of this pool includes: < specialized, ShareComponents... > 

        double_buffer_info                              m_DoubleBufferInfo          {};
        mecs::archetype::event::details::events         m_Events                    {};

        xcore::lock::semaphore                          m_SemaphoreLock             {};
        safety_lock_object                              m_Safety                    {};

        mecs::sync_point::instance*                     m_pLastSyncPoint            { nullptr };
        std::uint32_t                                   m_LastTick                  { 0 };

        guid                                            m_Guid                      {};

        mecs::component::filter_bits                    m_ArchitypeBits             { nullptr };
        mecs::component::filter_bits                    m_TagBits                   { nullptr };

        main_pool_descriptors                           m_MainPoolDescriptorData    {};
    };

    //---------------------------------------------------------------------------------
    // ARCHETYPE:: DATA BASE DETAILS
    //---------------------------------------------------------------------------------
    namespace details
    {
        template< typename...T_ARGS >
        constexpr auto getComponentDescriptorArray( std::tuple<T_ARGS...>* ) noexcept
        {
            return std::array<const mecs::component::descriptor*,sizeof...(T_ARGS)>{ &mecs::component::descriptor_v<T_ARGS> ... };
        }

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

        template< typename...T_COMPONENTS >
        constexpr auto ComputeGUID(std::tuple<T_COMPONENTS...>*) noexcept
        {
            return mecs::component::entity::guid
            {
                ((mecs::component::descriptor_v<T_COMPONENTS>.m_Guid.m_Value) + ... + 0ull)
            };
        }

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

        template< typename...T_COMPONENTS >
        constexpr auto getTupleDataComponentsAddEntity( std::tuple<T_COMPONENTS...>* ) noexcept
        {
            static_assert(((mecs::component::descriptor_v<T_COMPONENTS>.m_Guid.m_Value != 1ull) && ...), "You can not have an entity component in the given list");
            using my_tuple = xcore::types::tuple_cat_t< std::tuple<mecs::component::entity>, std::tuple<T_COMPONENTS...> >;
            return getTupleDataComponents( reinterpret_cast<my_tuple*>(nullptr) );
        }

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

        struct tag_entry
        {
            struct archetype_db
            {
                struct architype_entry
                {
                    component::filter_bits                                                            m_ArchitypeBits   {nullptr};
                    std::unique_ptr<archetype::instance>                                              m_upArchetype     {};
                };

                using list = std::array<architype_entry, mecs::settings::max_archetype_types>;

                std::uint32_t                                                                         m_nArchetypes     {0};
                std::unique_ptr<list>                                                                 m_uPtr            { std::make_unique<list>() };
            };

            component::filter_bits                                                                    m_TagBits         { nullptr };    // Tag bitsets (Unique bit set for each tag)
            xcore::lock::object< archetype_db, xcore::lock::semaphore >                               m_ArchetypeDB     {};             // Data base of archetypes
            xcore::unique_span<const mecs::component::descriptor*>                                    m_TagDescriptors  {};
        };

        struct events
        {
            using created_archetype = xcore::types::make_unique< mecs::tools::event<archetype::instance&>,  struct created_archetype_tag  >;
            using deleted_archetype = xcore::types::make_unique< mecs::tools::event<archetype::instance&>,  struct delete_archetype_tag   >;

            created_archetype   m_CreatedArchetype;
            deleted_archetype   m_DeletedArchetype;
        };
    }

    //---------------------------------------------------------------------------------
    // ARCHETYPE::INSTANCE DATA BASE
    //---------------------------------------------------------------------------------
    struct data_base
    {
        details::events m_Event;

        using map_archetypes = mecs::tools::fixed_map<mecs::archetype::instance::guid, mecs::archetype::instance*,  mecs::settings::max_archetype_types >;
        using map_tags       = mecs::tools::fixed_map<mecs::archetype::tag_sum_guid,   details::tag_entry*,         mecs::settings::max_archetype_types >;

        template< typename...T_COMPONENTS >
        constexpr instance& getOrCreateArchitype( void ) noexcept
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

        template< typename...T_ADD_COMPONENTS_AND_TAGS, typename...T_REMOVE_COMPONENTS_AND_TAGS> constexpr xforceinline
        instance& getOrCreateGroupBy( instance& OldArchetype, std::tuple<T_ADD_COMPONENTS_AND_TAGS...>*, std::tuple<T_REMOVE_COMPONENTS_AND_TAGS...>* ) noexcept
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

        void Init( void )
        {
        }

        void Start( void ) noexcept
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

        template<typename T_FUNCTION, auto& Defined >
        void DoQuery( query::instance& Instance ) const noexcept
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
                        else 
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
                    }

                    // Update the double buffer bits if we got any
                    if (DoubleBufferDirtyBits)
                    {
                        for (auto L = Archetype.m_DoubleBufferInfo.m_DirtyBits.load(std::memory_order_relaxed);
                             false == const_cast<std::atomic<std::uint64_t>&>(Archetype.m_DoubleBufferInfo.m_DirtyBits).compare_exchange_weak(L, L | DoubleBufferDirtyBits);
                            );
                    }
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

        xforceinline instance& getArchetype( mecs::archetype::instance::guid Guid ) noexcept
        {
            return *m_mapArchetypes.get(Guid);
        }

        xforceinline instance& getOrCreateArchitypeDetails(
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

        template< typename T_FUNCTION >
        void InitializeQuery( mecs::archetype::query::instance& QueryInstance )
        {
            using funcition = T_FUNCTION;

            if( QueryInstance.m_isInitialized ) return;
            QueryInstance.m_isInitialized = true;
        }

        template< typename T_CALLBACK = void(*)(), typename...T_SHARE_COMPONENTS >
        void moveEntityToArchetype( system::instance&           System
                                  , mecs::component::entity&    OldEntity
                                  , archetype::instance&        NewArchetype
                                  , T_CALLBACK&&                Callback = []{}
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
                        Specialized.m_ShareComponentKeysSpan    = std::span{ Specialized.m_ShareComponentKeysMemory.data(), sizeof...(T_SHARE_COMPONENTS) };
                        Specialized.m_EntityPool.Init( NewArchetype, NewArchetype.m_Descriptor.m_DataDescriptorSpan, settings::max_default_entities_per_pool );
                        for (int k = 0, end2 = static_cast<int>(Specialized.m_ShareComponentKeysSpan.size()); k != end2; ++k) 
                            Specialized.m_ShareComponentKeysMemory[k] = ShareKeys[k];

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
                        Specialized.m_EntityPool.Init( NewArchetype, NewArchetype.m_Descriptor.m_DataDescriptorSpan, settings::max_default_entities_per_pool );

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

        mecs::component::entity::map                                m_EntityMap;
        map_archetypes                                              m_mapArchetypes;
        map_tags                                                    m_mapTags;
        std::uint16_t                                               m_nTagEntries{0};
        std::array<details::tag_entry, mecs::settings::max_archetype_types>  m_lTagEntries{};
    };
}