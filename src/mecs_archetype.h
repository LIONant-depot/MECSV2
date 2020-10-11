namespace mecs::archetype
{
    // virtual page remap - https://stackoverflow.com/questions/17197615/no-mremap-for-windows

    using tag_sum_guid = xcore::guid::unit<64, struct tag_sum_tag>;
 
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
                std::uint8_t                    m_nReaders{ 0 };                        // How many readers have this archetype locked for reading
                std::uint8_t                    m_nWriters{ 0 };                        // How many writers have this archetype locked for writing
            };

            struct per_system
            {
                using component_list_t = std::array<component_lock_entry, mecs::settings::max_data_components_per_entity>;

                mecs::system::instance*         m_pSystem           {nullptr};
                component_list_t                m_lComponentLocks   {};
                int                             m_nTotalInformation { 0 };
                std::uint8_t                    m_nShareWriters     { 0 };
            };

        #if _XCORE_ASSERTS
            static bool LockQueryComponents     ( system::instance& System,     const mecs::archetype::query::instance&       Query ) noexcept;
            static bool LockQueryComponents     ( system::instance& System,     const mecs::archetype::query::result_entry&   E     ) noexcept;
            static bool LockQueryComponents     ( per_system&       PerSystem,  const mecs::archetype::query::result_entry&   E     ) noexcept;

            static bool UnlockQueryComponents   ( system::instance& System,     const mecs::archetype::query::instance&       Query ) noexcept;
            static bool UnlockQueryComponents   ( system::instance& System,     const mecs::archetype::query::result_entry&   E     ) noexcept;
            static bool UnlockQueryComponents   ( per_system&       PerSystem,  const mecs::archetype::query::result_entry&   E     ) noexcept;
        #else
            xforceinline static bool LockQueryComponents     ( system::instance&,  const mecs::archetype::query::instance&     ) noexcept { return false; }
            xforceinline static bool LockQueryComponents     ( system::instance&,  const mecs::archetype::query::result_entry& ) noexcept { return false; }
            xforceinline static bool LockQueryComponents     ( per_system&      ,  const mecs::archetype::query::result_entry& ) noexcept { return false; }

            xforceinline static bool UnlockQueryComponents   ( system::instance&,  const mecs::archetype::query::instance&     ) noexcept { return false; }
            xforceinline static bool UnlockQueryComponents   ( system::instance&,  const mecs::archetype::query::result_entry& ) noexcept { return false; }
            xforceinline static bool UnlockQueryComponents   ( per_system&      ,  const mecs::archetype::query::result_entry& ) noexcept { return false; }
        #endif

            std::vector<per_system>         m_PerSystem;
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
        using                           share_component_index   = std::array<std::uint32_t, mecs::settings::max_data_components_per_entity>;

        type_guid                       m_TypeGuid                  {};
        instance*                       m_pArchetypeInstance        {};
        mecs::entity_pool::instance     m_EntityPool                {};
        std::span<std::uint64_t>        m_ShareComponentKeysSpan    {};
        mecs::entity_pool::index        m_MainPoolIndex             {};
        share_component_keys            m_ShareComponentKeysMemory  {};
        share_component_index           m_ShareMapIndices           {};
    };

    //----------------------------------------------------------------------------------------------
    // ARCHETYPE:: ENTITY CREATION
    //----------------------------------------------------------------------------------------------

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

        //-----------------------------------------------------------------------------------
        constexpr
        bool                    isValid                 ( void 
                                                        ) const noexcept;
        //-----------------------------------------------------------------------------------
        template< typename T_CALLBACK >
        void                    operator()              ( T_CALLBACK&& Callback 
                                                        ) noexcept;
        //-----------------------------------------------------------------------------------
        inline
        void                    operator()              ( void
                                                        ) noexcept;

        //-----------------------------------------------------------------------------------
        template< typename      T_CALLBACK
                , typename...   T_COMPONENTS
                >
        xforceinline constexpr
        void                    ProcessIndirect         ( mecs::component::entity::reference&   Reference
                                                        , T_CALLBACK&&                          Callback
                                                        , std::tuple<T_COMPONENTS...>*          
                                                        ) const noexcept;
        //-----------------------------------------------------------------------------------
        template< typename...T_COMPONENTS > 
        xforceinline constexpr
        void                    getTrueComponentIndices ( std::span<std::uint8_t>                               Array
                                                        , std::span<const mecs::component::descriptor* const>   Span
                                                        , std::tuple<T_COMPONENTS...>*                                  
                                                        ) const noexcept;
        //-----------------------------------------------------------------------------------
        template< typename      T_CALLBACK
                , typename...   T_COMPONENTS >
        xforceinline
        void                    Initialize              ( T_CALLBACK&&                      Callback
                                                        , std::tuple<T_COMPONENTS...>*      
                                                        ) noexcept;
    };

    //----------------------------------------------------------------------------------------------
    // ARCHETYPE:: INSTANCE
    //----------------------------------------------------------------------------------------------

    struct instance
    {
        using guid = xcore::guid::unit<64, struct archetype_tag>;

        //-----------------------------------------------------------------------------------
        inline
        void                            Init                ( const std::span<const mecs::component::descriptor* const>   DataDescriptorList
                                                            , const std::span<const mecs::component::descriptor* const>   ShareDescriptorList
                                                            , const std::span<const mecs::component::descriptor* const>   TagDescriptorList
                                                            , mecs::component::entity::map*                               pEntityMap 
                                                            ) noexcept;
        //-----------------------------------------------------------------------------------
        constexpr xforceinline
        auto&                           getGuid             ( void 
                                                            ) const noexcept { return m_Guid; }
        //-----------------------------------------------------------------------------------
        inline
        void                            MemoryBarrierSync   ( mecs::sync_point::instance& SyncPoint 
                                                            ) noexcept;
        //-----------------------------------------------------------------------------------
        inline
        void                            Start               ( void 
                                                            ) noexcept;
        //-----------------------------------------------------------------------------------
        template< typename...   T_SHARE_COMPONENTS
                >
        entity_creation                 CreateEntities      ( mecs::system::instance&                   Instance
                                                            , const int                                 nEntities
                                                            , std::span<mecs::component::entity::guid>  Guids
                                                            , T_SHARE_COMPONENTS&&...                   ShareComponents 
                                                            ) noexcept;
        //-----------------------------------------------------------------------------------
        template< typename      T_CALLBACK          = void(*)()
                , typename...   T_SHARE_COMPONENTS >
        xforceinline
        void                            CreateEntity        ( mecs::system::instance&                   Instance
                                                            , T_CALLBACK&&                              CallBack = [](){}
                                                            , T_SHARE_COMPONENTS&&...                   ShareComponents 
                                                            ) noexcept;
        //-----------------------------------------------------------------------------------
        inline
        void                            deleteEntity        ( mecs::system::instance&                   System
                                                            , mecs::component::entity&                  Entity 
                                                            ) noexcept;
        //-----------------------------------------------------------------------------------
        template< typename...   T_SHARE_COMPONENTS >
        constexpr
        specialized_pool::type_guid     getSpecializedPoolGuid ( const instance&                        Instance
                                                               , T_SHARE_COMPONENTS&&...                ShareComponents
                                                               ) const noexcept;
        //-----------------------------------------------------------------------------------
        inline
        void                        moveEntityBetweenSpecializePools        ( system::instance&         System
                                                                            , int                       FromSpecializePoolIndex
                                                                            , int                       EntityIndexInSpecializedPool
                                                                            , std::uint64_t             NewCRCPool
                                                                            , std::byte**               pPointersToShareComponents
                                                                            , std::uint64_t*            pListOfCRCFromShareComponents 
                                                                            ) noexcept;
        //-----------------------------------------------------------------------------------
        template< typename... T_SHARE_COMPONENTS >
        inline
        specialized_pool&           getOrCreateSpecializedPool              ( system::instance&         System
                                                                            , int                       MinFreeEntries = 1
                                                                            , int                       MaxEntries     = mecs::settings::max_default_entities_per_pool
                                                                            , T_SHARE_COMPONENTS...     ShareComponents 
                                                                            ) noexcept;
        //-----------------------------------------------------------------------------------
        
        struct double_buffer_info
        {
            static_assert(mecs::settings::max_data_components_per_entity <= 64);
            std::uint64_t               m_StateBits{ 0 };           // Tells which of the two columns is T0. Index is same as the component inside the archetype descriptor.
            std::atomic<std::uint64_t>  m_DirtyBits{ 0 };           // Tells which bits in the StateBits need to change state.
        };


        using main_pool_descriptors = std::array<const mecs::component::descriptor*, mecs::settings::max_data_components_per_entity>;
        using safety_lock_object    = xcore::lock::object<details::safety, xcore::lock::semaphore >;
        using share_one_to_many_map = mecs::tools::fixed_map<guid, std::vector<specialized_pool*>, settings::max_specialized_pools_v>;
        using share_map             = std::shared_ptr<share_one_to_many_map>;

        //-----------------------------------------------------------------------------------

        mecs::component::entity::map*                   m_pEntityMap                { nullptr };

        descriptor                                      m_Descriptor                {};
        std::span<const mecs::component::descriptor*>   m_MainPoolDescriptorSpan    {};
        mecs::entity_pool::instance                     m_MainPool                  {};     // this pool should be sorted by its share components values
                                                                                            // component of this pool includes: < specialized, ShareComponents... > 

        double_buffer_info                              m_DoubleBufferInfo          {};
        mecs::archetype::event::details::events         m_Events                    {};

        xcore::lock::semaphore_reentrant                m_SemaphoreLock             {};
        safety_lock_object                              m_Safety                    {};

        mecs::sync_point::instance*                     m_pLastSyncPoint            { nullptr };
        std::uint32_t                                   m_LastTick                  { 0 };

        guid                                            m_Guid                      {};

        mecs::component::filter_bits                    m_ArchitypeBits             { nullptr };
        mecs::component::filter_bits                    m_TagBits                   { nullptr };

        std::array<std::shared_ptr<share_map>,mecs::settings::max_data_components_per_entity> m_ShareMapTable;

        main_pool_descriptors                           m_MainPoolDescriptorData    {};
    };


    //---------------------------------------------------------------------------------
    // ARCHETYPE::INSTANCE DATA BASE
    //---------------------------------------------------------------------------------
    namespace details
    {
        //---------------------------------------------------------------------------------
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

        //---------------------------------------------------------------------------------
        struct events
        {
            using created_archetype = xcore::types::make_unique< mecs::tools::event<archetype::instance&>, struct created_archetype_tag  >;
            using deleted_archetype = xcore::types::make_unique< mecs::tools::event<archetype::instance&>, struct delete_archetype_tag   >;

            created_archetype   m_CreatedArchetype;
            deleted_archetype   m_DeletedArchetype;
        };
    }

    //---------------------------------------------------------------------------------
    struct data_base
    {
        //-----------------------------------------------------------------------------------
        template< typename...T_COMPONENTS >
        constexpr
        instance&                   getOrCreateArchitype                    ( void 
                                                                            ) noexcept;
        //-----------------------------------------------------------------------------------
        template< typename...T_ADD_COMPONENTS_AND_TAGS
                , typename...T_REMOVE_COMPONENTS_AND_TAGS >
        constexpr xforceinline
        instance&                   getOrCreateGroupBy                      ( instance&                                 OldArchetype
                                                                            , std::tuple<T_ADD_COMPONENTS_AND_TAGS...>*
                                                                            , std::tuple<T_REMOVE_COMPONENTS_AND_TAGS...>* 
                                                                            ) noexcept;
        //-----------------------------------------------------------------------------------
        inline
        void                        Init                                    ( void 
                                                                            ) noexcept;

        //-----------------------------------------------------------------------------------
        inline
        void                        Start                                   ( void 
                                                                            ) noexcept;
        //-----------------------------------------------------------------------------------
        template< auto& func_sorted_descriptors
                , auto& func_remap_from_sort >
        static void                 DetailsDoQuery                          ( mecs::archetype::query::result_entry& Entry 
                                                                            ) noexcept;
        //-----------------------------------------------------------------------------------
        template< typename T_FUNCTION
                , typename T_TUPLE_QUERY >
        constexpr xforceinline
        void                        DoQuery                                 ( query::instance& Instance 
                                                                            ) const noexcept;
        //-----------------------------------------------------------------------------------
        template< typename  T_FUNCTION
                , auto&     Defined >
        constexpr xforceinline
        void                        DetailsDoQuery                          ( query::instance& Instance 
                                                                            ) const noexcept;
        //-----------------------------------------------------------------------------------
        xforceinline
        instance&                   getArchetype                            ( mecs::archetype::instance::guid Guid 
                                                                            ) noexcept;
        //-----------------------------------------------------------------------------------
        xforceinline
        instance&                   getOrCreateArchitypeDetails             ( mecs::archetype::instance::guid                       Guid
                                                                            , mecs::archetype::tag_sum_guid                         TagSumGuid
                                                                            , std::span<const mecs::component::descriptor* const>   DataDiscriptorList
                                                                            , std::span<const mecs::component::descriptor* const>   ShareDiscriptorList
                                                                            , std::span<const mecs::component::descriptor* const>   TagDiscriptorList 
                                                                            ) noexcept;
        //-----------------------------------------------------------------------------------
        template< typename T_FUNCTION >
        inline
        void                        InitializeQuery                         ( mecs::archetype::query::instance& QueryInstance 
                                                                            ) noexcept;
        //-----------------------------------------------------------------------------------
        template< typename      T_CALLBACK          = void(*)()
                , typename...   T_SHARE_COMPONENTS >
        inline
        void                        moveEntityToArchetype                   ( system::instance&           System
                                                                            , mecs::component::entity&    OldEntity
                                                                            , archetype::instance&        NewArchetype
                                                                            , T_CALLBACK&&                Callback = [](){}
                                                                            , T_SHARE_COMPONENTS&&...     ShareComponents 
                                                                            ) noexcept;
        //-----------------------------------------------------------------------------------

        using map_archetypes    = mecs::tools::fixed_map<mecs::archetype::instance::guid, mecs::archetype::instance*, mecs::settings::max_archetype_types >;
        using map_tags          = mecs::tools::fixed_map<mecs::archetype::tag_sum_guid, details::tag_entry*, mecs::settings::max_archetype_types >;
        using tag_list          = std::array<details::tag_entry, mecs::settings::max_archetype_types>;

        //-----------------------------------------------------------------------------------
        details::events                         m_Event;
        mecs::component::entity::map            m_EntityMap;
        map_archetypes                          m_mapArchetypes;
        map_tags                                m_mapTags;
        std::uint16_t                           m_nTagEntries{0};
        tag_list                                m_lTagEntries{};
    };
}