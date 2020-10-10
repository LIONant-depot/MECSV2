namespace mecs::system
{
    using type_guid = xcore::guid::unit<64, struct type_tag>;

    struct descriptor;

    //----------------------------------------------------------------------------
    // SYSTEM:: INSTANCE OVERRIDES
    //----------------------------------------------------------------------------
    namespace details
    {
        struct cache
        {
            struct per_function
            {
                mecs::archetype::query::result_entry    m_ResultEntry;
                std::uint8_t                            m_nDelegatesIndices;
                std::array<std::uint8_t, 16>            m_UpdateDelegateIndex;
            };

            struct data
            {
                using per_func_list = std::array<std::uint64_t, 16>;

                xcore::lock::object< int, xcore::lock::semaphore >          m_nFunctions;
                per_func_list                                               m_WhichFunction;
                std::array<per_function, per_func_list{}.size()>            m_PerFunction;
            };

            using lines = xcore::lock::object< std::vector<mecs::archetype::instance*>, xcore::lock::semaphore >;
            
            lines                   m_Lines;        // Fast, cache friendly, linear search loop up.
            xcore::vector<data>     m_Data;         // Data per each line. It should be a 1:1 mapping with lines

            template< typename T_GET_CALLBACK, typename T_CREATE_CALLBACK >
            xforceinline
            void getOrCreateCache(std::uint64_t FunctionGUID, mecs::archetype::instance& Archetype, T_GET_CALLBACK&& GetCallBack, T_CREATE_CALLBACK&& Create ) noexcept;
        };
    }

    struct overrides : xcore::scheduler::job<mecs::settings::max_syncpoints_per_system>
    {
        using                           type_guid       = mecs::system::type_guid;
        using                           entity          = mecs::component::entity;
        template<typename...T> using    all             = mecs::archetype::query::all<std::tuple<T...>>;
        template<typename...T> using    any             = mecs::archetype::query::any<std::tuple<T...>>;
        template<typename...T> using    none            = mecs::archetype::query::none<std::tuple<T...>>;
        using                           job_t           = xcore::scheduler::job<mecs::settings::max_syncpoints_per_system>;
        using                           guid            = xcore::guid::unit<64, struct mecs_system_instance_tag>;
        using                           archetype       = mecs::archetype::instance;
        using                           query           = mecs::archetype::query::instance;
        template< auto&& T_SHARE_COMPONENT_V> using share_key = mecs::component::details::share_ref_inst< decltype(T_SHARE_COMPONENT_V), decltype(T_SHARE_COMPONENT_V)::getKey(&xcore::types::lvalue(T_SHARE_COMPONENT_V)) >;


        template< typename T_SYSTEM, typename...T_PARAMETERS >
        struct exclusive_event : mecs::system::event::exclusive
        {
            using   real_event_t = define_real_event< T_PARAMETERS... >;
            using   system_t     = T_SYSTEM;
        };

        // Structure used for the construction of the hierarchy 
        struct construct
        {
            guid                            m_Guid;
            mecs::world::instance&          m_World;
            xcore::string::const_universal  m_Name;
            xcore::scheduler::definition    m_JobDefinition;
        };

        // Defaults that can be overwritten by the user
        static constexpr auto               entities_per_job_v  = 5000;
        static constexpr auto               type_guid_v         = type_guid{ nullptr };
        static constexpr auto               name_v              = xconst_universal_str("mecs::System(Unnamed)");
        using                               query_t             = std::tuple<>;
        using                               events_t            = std::tuple<>;

        using job_t::job_t;

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

    struct instance : overrides
    {
        //----------------------------------------------------------------------------
        inline
                                            instance                ( const construct&& Settings
                                                                    ) noexcept;
        //----------------------------------------------------------------------------
        constexpr
        auto                                getGUID                 ( void 
                                                                    ) const noexcept { return m_Guid; }
        //----------------------------------------------------------------------------
        template< typename...T_SHARE_COMPONENTS
                >
        inline
        mecs::archetype::entity_creation    createEntities          ( mecs::archetype::instance::guid   gArchetype
                                                                    , int                               nEntities
                                                                    , std::span<entity::guid>           gEntitySpan
                                                                    , T_SHARE_COMPONENTS&&...           ShareComponents 
                                                                    ) noexcept;
        //----------------------------------------------------------------------------
        inline
        void                                deleteEntity            ( mecs::component::entity&          Entity 
                                                                    ) noexcept;
        //----------------------------------------------------------------------------
        template< typename T_CALLBACK
                >
        constexpr xforceinline
        void                                ForEach                 ( mecs::archetype::query::instance& QueryI
                                                                    , T_CALLBACK&&                      Functor
                                                                    , int                               nEntitiesPerJob
                                                                    ) noexcept;
        //----------------------------------------------------------------------------
        template< typename T_ADD_COMPONENTS_AND_TAGS
                , typename T_REMOVE_COMPONENTS_AND_TAGS
                , typename T_CALLBACK
                >
        constexpr xforceinline
        void                                getArchetypeBy          ( mecs::component::entity&          Entity
                                                                    , T_CALLBACK&&                      Callback 
                                                                    ) noexcept;
        //----------------------------------------------------------------------------
        template< typename      T_CALLBACK          = void(*)()
                , typename...   T_SHARE_COMPONENTS
                >
        constexpr xforceinline
        void                                moveEntityToArchetype   ( mecs::component::entity&          Entity
                                                                    , mecs::archetype::instance&        ToNewArchetype
                                                                    , T_CALLBACK&&                      Callback = []{}
                                                                    , T_SHARE_COMPONENTS&&...           ShareComponents 
                                                                    ) noexcept;
        //----------------------------------------------------------------------------
        inline
        const time&                         getTime                 ( void 
                                                                    ) const noexcept;
        //----------------------------------------------------------------------------
        template< typename      T_EVENT
                , typename      T_SYSTEM = typename T_EVENT::system_t
                , typename...   T_ARGS
                >
        xforceinline constexpr
        void                                EventNotify             ( T_ARGS&&...Args 
                                                                    ) noexcept;
        //----------------------------------------------------------------------------
        // This function implementation will be provided by MECS
        virtual
        const descriptor&                   getDescriptor           ( void 
                                                                    ) const noexcept = 0;

        //----------------------------------------------------------------------------
        template< typename T_CALLBACK
                >
        constexpr xforceinline
        void                                getEntityComponents     ( const mecs::component::entity&    Entity
                                                                    , T_CALLBACK&&                      Function 
                                                                    ) noexcept;
        //----------------------------------------------------------------------------
        template< typename T_CALLBACK
                >
        constexpr xforceinline
        bool                                findEntityComponents    ( mecs::component::entity::guid     gEntity
                                                                    , T_CALLBACK&&                      Function 
                                                                    ) noexcept;
        //----------------------------------------------------------------------------
        template< typename... T_COMPONENTS
                >
        constexpr xforceinline
        mecs::archetype::instance&          getOrCreateArchetype    ( void
                                                                    ) const noexcept;

        //----------------------------------------------------------------------------
        template< typename      T_GET_CALLBACK
                , typename      T_CREATE_CALLBACK
                >
        constexpr xforceinline
        void                                getOrCreateEntity       ( mecs::component::entity::guid         gEntity
                                                                    , mecs::archetype::specialized_pool&    SpecialiedPool
                                                                    , T_GET_CALLBACK&&                      GetCallback
                                                                    , T_CREATE_CALLBACK&&                   CreateCallback
                                                                    ) noexcept;
        //----------------------------------------------------------------------------
        template< typename  T_FUNCTION_TYPE
                , typename  T_COMPONENT_TUPLE = std::tuple<>
                >
        constexpr xforceinline
        void                                DoQuery                 ( query& Query 
                                                                    ) const noexcept;

        //----------------------------------------------------------------------------
        // Details functions
        //----------------------------------------------------------------------------

        //----------------------------------------------------------------------------
        template< bool      T_ALREADY_LOCKED_V
                , typename  T_CALLBACK >
        constexpr xforceinline
        void                                _getEntityComponents    ( const mecs::component::entity::reference& Reference
                                                                    , T_CALLBACK&&                              Function 
                                                                    ) noexcept;
        //----------------------------------------------------------------------------
        virtual
        void*                               _getExclusiveRealEvent  ( const system::event::type_guid 
                                                                    ) const noexcept = 0;
        //----------------------------------------------------------------------------
        template< typename T_PARAMS
                , typename T_PARAMS2 >
        constexpr xforceinline
        void                                ProcessResult           ( T_PARAMS&                             Params
                                                                    , T_PARAMS2&                            Params2
                                                                    , const int                             Index 
                                                                    ) noexcept;
        //----------------------------------------------------------------------------
        void                                DetailsClearGroupCache  ( void 
                                                                    ) noexcept;

        //----------------------------------------------------------------------------
        // VARIABLES
        //----------------------------------------------------------------------------

        // TODO: This will be private
        mecs::world::instance&              m_World;
        query                               m_Query     {};
        details::cache                      m_Cache     {};
        mecs::system::instance::guid        m_Guid;
    };


    //---------------------------------------------------------------------------------
    // SYSTEM:: descriptor
    //---------------------------------------------------------------------------------
    struct descriptor
    {
        using fn_create = std::unique_ptr<mecs::system::instance>( mecs::system::instance::construct&& C ) noexcept;

        mecs::system::type_guid                             m_Guid;
        fn_create*                                          m_fnCreate;
        xcore::string::const_universal                      m_Name;
        std::span<const system::event::descriptor* const >  m_ExclusiveEvents;
        std::span<const system::event::descriptor* const >  m_GlobalEvents;
    };
    
    namespace details
    {
        template< typename T_SYSTEM >
        xforceinline constexpr
        auto MakeDescriptor( void ) noexcept;
    }

    template< typename T_SYSTEM >
    inline static constexpr auto descriptor_v = details::MakeDescriptor<T_SYSTEM>();

    //---------------------------------------------------------------------------------
    // SYSTEM:: DESCRIPTOR DATA BASE
    //---------------------------------------------------------------------------------

    struct descriptors_data_base
    {
        using map_system_types = mecs::tools::fixed_map< mecs::system::type_guid, const descriptor*, mecs::settings::max_systems >;

        void Init( void ) noexcept {}

        template< typename...T_SYSTEMS >
        void Register( mecs::system::event::descriptors_data_base& EventDescriptionDataBase ) noexcept;

        std::vector<const descriptor*>      m_lDescriptors;
        map_system_types                    m_mapDescriptors;
    };
    
    //---------------------------------------------------------------------------------
    // SYSTEM:: INSTANCE DATA BASE
    //---------------------------------------------------------------------------------
    namespace details
    {
        struct events
        {
            using created_system = xcore::types::make_unique< mecs::tools::event<system::instance&>, struct created_system_tag  >;
            using deleted_system = xcore::types::make_unique< mecs::tools::event<system::instance&>, struct delete_system_tag   >;

            created_system   m_CreatedSystem;
            deleted_system   m_DeletedSystem;
        };
    }

    struct instance_data_base
    {
        using map_systems = mecs::tools::fixed_map< mecs::system::type_guid, std::unique_ptr<mecs::system::instance>, mecs::settings::max_systems >;

        instance_data_base( mecs::world::instance& World ) : m_World(World) {}

        inline
        instance& Create( const mecs::system::descriptor& Descriptor, mecs::system::instance::guid Guid ) noexcept;

        template< typename T_SYSTEM >
        instance& Create( instance::guid Guid ) noexcept
        {
            return Create( descriptor_v<T_SYSTEM>, Guid );
        }

        details::events                     m_Event;
        mecs::world::instance&              m_World;
        std::vector<instance*>              m_lInstance;
        map_systems                         m_mapInstances;
    };
}