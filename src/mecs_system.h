namespace mecs::system
{
    using type_guid = xcore::guid::unit<64, struct type_tag>;

    struct descriptor;

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
    // SYSTEM:: INSTANCE OVERRIDES
    //----------------------------------------------------------------------------
    
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
        
        inline                              instance                ( const construct&& Settings) noexcept;

        constexpr
        auto                                getGUID                 ( void ) const noexcept { return m_Guid; }

        template< typename...T_SHARE_COMPONENTS >
        inline
        mecs::archetype::entity_creation    createEntities          ( mecs::archetype::instance::guid   gArchetype
                                                                    , int                               nEntities
                                                                    , std::span<entity::guid>           gEntitySpan
                                                                    , T_SHARE_COMPONENTS&&...           ShareComponents ) noexcept;

        inline
        void                                deleteEntity            ( mecs::component::entity&          Entity ) noexcept;

        template< typename T_CALLBACK >
        constexpr xforceinline
        void                                ForEach                 ( mecs::archetype::query::instance& QueryI
                                                                    , T_CALLBACK&&                      Functor
                                                                    , int                               nEntitiesPerJob ) noexcept;

        template< typename T_ADD_COMPONENTS_AND_TAGS
                , typename T_REMOVE_COMPONENTS_AND_TAGS
                , typename T_CALLBACK >
        constexpr xforceinline
        void                                getArchetypeBy          ( mecs::component::entity&          Entity
                                                                    , T_CALLBACK&&                      Callback ) noexcept;

        template< typename T_CALLBACK = void(*)(), typename...T_SHARE_COMPONENTS >
        constexpr xforceinline
        void                                moveEntityToArchetype   ( mecs::component::entity&          Entity
                                                                    , mecs::archetype::instance&        ToNewArchetype
                                                                    , T_CALLBACK&&                      Callback = []{}
                                                                    , T_SHARE_COMPONENTS&&...           ShareComponents ) noexcept;

        inline
        const time&                         getTime                 ( void ) const noexcept;

        template< typename      T_EVENT
                , typename      T_SYSTEM = typename T_EVENT::system_t
                , typename...   T_ARGS >
        xforceinline constexpr
        void                                EventNotify             ( T_ARGS&&...Args ) noexcept;

        // This function implementation will be provided by MECS
        virtual
        const descriptor&                   getDescriptor           ( void ) const noexcept = 0;

        // Details functions
        virtual
        void*                               DetailsGetExclusiveRealEvent( const system::event::type_guid ) noexcept = 0;

        template< typename T_PARAMS, typename T_PARAMS2 >
        constexpr xforceinline
        void                                ProcessResult           ( T_PARAMS&                             Params
                                                                    , T_PARAMS2&                            Params2
                                                                    , const int                             Index ) noexcept;

        // TODO: This will be private
        mecs::world::instance&              m_World;
        mecs::system::instance::guid        m_Guid;
        mecs::archetype::query::instance    m_Query{};
        mecs::system::instance::type_guid   m_TypeGuid;
        // details::cache      m_Cache;
    };


    //---------------------------------------------------------------------------------
    // SYSTEM:: descriptor
    //---------------------------------------------------------------------------------
    struct descriptor
    {
        using fn_create = std::unique_ptr<mecs::system::instance>( const mecs::system::instance::construct&& C ) noexcept;

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

        void Init( void ) noexcept {}

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