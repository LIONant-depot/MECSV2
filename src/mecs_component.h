
namespace mecs::component
{
    //----------------------------------------------------------------------------
    // BASIC TYPES
    //----------------------------------------------------------------------------
    using type_guid             = xcore::guid::unit<64, struct type_tag>;
    using compound_type_guid    = xcore::guid::unit<64, struct compounded_type_tag>;
    using filter_bits           = xcore::types::make_unique< mecs::tools::bits, struct filter_bits_tag >;

    enum class type_data_access : std::uint8_t
    {
            LINEAR
        ,   DOUBLE_BUFFER
        ,   QUANTUM
        ,   QUANTUM_DOUBLE_BUFFER
        ,   ENUM_COUNT
    };

    enum class type : std::uint8_t
    {
            TAG
        ,   DATA
        ,   SINGLETON
        ,   RESOURCE
        ,   SHARE
        ,   ENUM_COUNT
        ,   SHARE_REF
    };

    //----------------------------------------------------------------------------
    // COMPONENT TYPES
    //----------------------------------------------------------------------------

    struct tag
    {
        using                   type_guid           = component::type_guid;
        constexpr static auto   type_guid_v         { type_guid{nullptr} };
        constexpr static auto   name_v              { xconst_universal_str("unnamed tag") };
    };

    struct data
    {
        using                   type_guid           = component::type_guid;
        using                   type_data_access    = component::type_data_access;
        constexpr static auto   type_guid_v         { type_guid{ nullptr } };
        constexpr static auto   name_v              { xconst_universal_str("data component") };
        constexpr static auto   type_data_access_v  { type_data_access::LINEAR };
    };

    struct singleton
    {
        using                   type_guid           = component::type_guid;
        using                   type_data_access    = component::type_data_access;
        constexpr static auto   type_guid_v         { type_guid{ nullptr } };
        constexpr static auto   name_v              { xconst_universal_str("unnamed singleton") };
        constexpr static auto   type_data_access_v  { type_data_access::LINEAR };
    };

    struct share
    {
        using                   type_guid           = component::type_guid;
        constexpr static auto   type_guid_v         { type_guid{ nullptr } };
        constexpr static auto   name_v              { xconst_universal_str("unnamed share") };
        static constexpr std::uint64_t   getKey(void*) noexcept { return 0; }
    };

    struct entity final : data
    {
        struct reference
        {
            mecs::entity_pool::instance*    m_pPool;
            mecs::entity_pool::index        m_Index;
        };

        using guid  = xcore::guid::unit<64, struct entity_tag>;
        using map   = mecs::tools::fixed_map<guid, reference, settings::max_entities_v>;

        constexpr static type_guid  type_guid_v { 1ull };
        constexpr static auto       name_v      { xconst_universal_str("entity") };

        map::entry* m_pInstance;

        xforceinline      bool              isZombie      ( void ) const noexcept { return reinterpret_cast<std::size_t>(m_pInstance) & 1; }
        xforceinline      map::entry&       getMapEntry   ( void ) const noexcept { return *reinterpret_cast<map::entry*>(reinterpret_cast<std::size_t>(m_pInstance) & (~std::size_t(1))); }
        xforceinline      const reference&  getReference  ( void ) const noexcept { return getMapEntry().m_Value; }
        xforceinline      guid              getGuid       ( void ) const noexcept { return getMapEntry().m_Key; }
        xforceinline      void              MarkAsZombie  ( void )       noexcept { reinterpret_cast<std::size_t&>(m_pInstance) = reinterpret_cast<std::size_t>(m_pInstance) | 1; }
    };

    //
    // RESEARCH BEGIN!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    // Additional type of components...
    //

    // For systems that need to search for which components can be used
    // it could be done with scopes.
    //      First scope is the entity itself
    //      Second scope is the pool
    //      Theird scope is the architype
    //      etc.
    // struct include_pool_scope      : tag {};
    // struct include_archetype_scope : tag {};
    // struct include_all_scopes      : tag {};
    // struct include_resource_scope  : tag {};
    //
    // Or may be it automatically search those scopes as long as there is a component to be found

    // TODO: not implemented yet, aka pool_share or archetype_share or resource_share
    // Share is like scope, which is the scope that is searching for components types
    // type_share could be one of:
    //      entity_share    -> Any component within the entity
    //                         This is the default state so this can be skip... 
    //      pool_share      -> Could be a bounding box where all the entities in there will render at the same time
    //                         Entities with the same pool_share->getKey() will get group into the same pool
    //      archetype_share -> Could be like a tags, or some other component
    //                         Entities with the same archetype_share->getKey() will get group into the same pool
  /*
    enum class type_share : std::uint8_t
    {
          POOL
        , ARCHETYPE
    };

    struct share
    {
        constexpr static auto           type_guid_v             { component::type_guid{ nullptr } };
        constexpr static auto           name_v                  { xconst_universal_str("unnamed resource") };
        constexpr static auto           type_data_access_v      { type_data_access::LINEAR };
        static constexpr entity::guid   getKey( void ) noexcept { return nullptr; }
    };
    */

    //      resource_share  -> Could be like a texture, mesh, etc.
    //                         Entities with the same resource_share->getKey() will get group into the same pool


    // TODO: Not implemented yet... work in progress..
    // This are done usually for meshes, textures, etc. Things that entities don't really have by they "reference" to
    // Group_by
    
    struct resource
    {
        constexpr static auto   type_guid_v         { component::type_guid{ nullptr } };
        constexpr static auto   name_v              { xconst_universal_str("unnamed resource") };
        constexpr static auto   type_data_access_v  { type_data_access::LINEAR };

        struct ref_base
        {
            constexpr static auto   group_by_reference_v{ false };
            entity::guid            m_Value;
        };

        template< typename T_COMPONENT >
        struct ref : ref_base
        {
            static_assert( std::is_base_of_v<resource, T_COMPONENT> );
            using type = T_COMPONENT;
        };

        template< typename T_COMPONENT >
        struct group_ref : ref<T_COMPONENT>
        {
            constexpr static auto   group_by_reference_v{ true };
        };
    };

    /*
    struct grouping
    {
        constexpr static auto   type_guid_v         { component::type_guid{ nullptr } };
        constexpr static auto   name_v              { xconst_universal_str("unnamed resource") };
        entity::guid            m_Value;
    };
    */
    //
    // RESEARCH END!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    //

    //----------------------------------------------------------------------------
    // DESCRIPTOR
    //----------------------------------------------------------------------------
    struct descriptor;

    struct descriptor
    {
        using fn_construct          = void(void*)           noexcept;
        using fn_destruct           = void(void*)           noexcept;
        using fn_move               = void(void*, void*)    noexcept;
        using fn_getkey             = std::uint64_t(void*)  noexcept;

        const type_guid                         m_Guid;
        union
        {
            const xcore::string::const_universal    m_Name;
            const descriptor*                       m_pDescriptor;
        };
        mutable std::int16_t                    m_BitNumber;
        const type                              m_Type;
        fn_construct* const                     m_fnConstruct;
        fn_destruct*  const                     m_fnDestruct;
        fn_move* const                          m_fnMove;
        fn_getkey* const                        m_fnGetKey;
        //property::table&                      m_PropertyTable;
        const std::uint32_t                     m_Size;
        const std::uint16_t                     m_Alignment;
        const type_data_access                  m_DataAccess;
        const bool                              m_isDoubleBuffer;
    };

    namespace details
    {
        struct share_ref{};

        template< typename T_SHARE_COMPONENT, std::uint64_t T_KEY_V >
        struct share_ref_inst : share_ref
        {
            static constexpr auto   key  = T_KEY_V;
            using                   type = T_SHARE_COMPONENT;
        };

        template< typename T_COMPONENT >
        static constexpr descriptor::fn_move* movable_fn_v = std::is_move_assignable_v<T_COMPONENT> == false
                                                           ? reinterpret_cast<descriptor::fn_move*>(nullptr)
                                                           : []( void* pDest, void* pSrc) noexcept
                                                             { *reinterpret_cast<T_COMPONENT*>(pDest) = std::move(*reinterpret_cast<T_COMPONENT*>(pSrc)); };

        template< typename T_COMPONENT >
        static constexpr descriptor::fn_destruct* destruct_fn_v = std::is_trivially_destructible_v<T_COMPONENT>
                                                                ? reinterpret_cast<descriptor::fn_destruct*>(nullptr)
                                                                : [](void* pData) noexcept
                                                                  { std::destroy_at(reinterpret_cast<T_COMPONENT*>(pData)); };

        template< typename T_COMPONENT >
        static constexpr descriptor::fn_construct* construct_fn_v = std::is_trivially_constructible_v<T_COMPONENT>
                                                                  ? reinterpret_cast<descriptor::fn_construct*>(nullptr)
                                                                  : [](void* pData) noexcept
                                                                     { new(pData) T_COMPONENT; };


        template< typename T_COMPONENT >
        constexpr auto MakeDescriptor( void ) noexcept 
        {
            if constexpr ( std::is_base_of_v<data, T_COMPONENT> )
            {
                static_assert(sizeof(T_COMPONENT) < 4096);
                return descriptor
                {
                        T_COMPONENT::type_guid_v.isValid() ? T_COMPONENT::type_guid_v : type_guid{ __FUNCSIG__ }
                    ,   T_COMPONENT::name_v
                    ,   -1
                    ,   type::DATA
                    ,   construct_fn_v<T_COMPONENT>
                    ,   destruct_fn_v<T_COMPONENT>
                    ,   movable_fn_v<T_COMPONENT>
                    ,   nullptr
                    ,   static_cast<std::uint32_t>( sizeof(T_COMPONENT) )
                    ,   static_cast<std::uint16_t>( std::alignment_of_v<T_COMPONENT> )
                    ,   T_COMPONENT::type_data_access_v
                    ,   T_COMPONENT::type_data_access_v == type_data_access::DOUBLE_BUFFER || T_COMPONENT::type_data_access_v == type_data_access::QUANTUM_DOUBLE_BUFFER
                };
            }
            else if constexpr (std::is_base_of_v<tag, T_COMPONENT>)
            {
                static_assert(sizeof(T_COMPONENT) == 1);
                return descriptor
                {
                        T_COMPONENT::type_guid_v
                    ,   T_COMPONENT::name_v
                    ,   -1
                    ,   type::TAG
                    ,   nullptr
                    ,   nullptr
                    ,   nullptr
                    ,   nullptr
                    ,   0u
                    ,   0u
                    ,   type_data_access::ENUM_COUNT
                    ,   false
                };
            }
            else if constexpr( std::is_base_of_v<singleton, T_COMPONENT> )
            {
                static_assert(T_COMPONENT::type_data_access_v == type_data_access::LINEAR || T_COMPONENT::type_data_access_v == type_data_access::QUANTUM);
                return descriptor
                {
                        T_COMPONENT::type_guid_v.isValid() ? T_COMPONENT::type_guid_v : type_guid{ __FUNCSIG__ }
                    ,   T_COMPONENT::name_v
                    ,   -1
                    ,   type::SINGLETON
                    ,   []( void* pData )     constexpr noexcept { new(pData) std::unique_ptr<T_COMPONENT>{ new T_COMPONENT }; }
                    ,   []( void* pData )     constexpr noexcept { std::destroy_at( reinterpret_cast<std::unique_ptr<T_COMPONENT>*>(pData) ); }
                    ,   []( void* d, void* s) constexpr noexcept { *reinterpret_cast<std::unique_ptr<T_COMPONENT>*>(d) = std::move(*reinterpret_cast<std::unique_ptr<T_COMPONENT>*>(s)); }
                    ,   nullptr
                    ,   static_cast<std::uint32_t>( sizeof(std::unique_ptr<T_COMPONENT>) )
                    ,   static_cast<std::uint16_t>( std::alignment_of_v<std::unique_ptr<T_COMPONENT>> )
                    ,   T_COMPONENT::type_data_access_v
                    ,   false
                };
            }
            else if constexpr (std::is_base_of_v<share, T_COMPONENT>)
            {
                static_assert(sizeof(T_COMPONENT) < 4096);
                return descriptor
                {
                        T_COMPONENT::type_guid_v.isValid() ? T_COMPONENT::type_guid_v : type_guid{ __FUNCSIG__ }
                    ,   T_COMPONENT::name_v
                    ,   -1
                    ,   type::SHARE
                    ,   construct_fn_v<T_COMPONENT>
                    ,   destruct_fn_v<T_COMPONENT>
                    ,   movable_fn_v<T_COMPONENT>
                    ,   (&T_COMPONENT::getKey == &mecs::component::share::getKey)? []( void* pData ) constexpr noexcept { xcore::crc<64> X{}; return X.FromBytes( {reinterpret_cast<std::byte*>(pData), sizeof(T_COMPONENT)} ).m_Value; } : &T_COMPONENT::getKey
                    ,   static_cast<std::uint32_t>( sizeof(T_COMPONENT) )
                    ,   static_cast<std::uint16_t>( std::alignment_of_v<T_COMPONENT> )
                    ,   type_data_access::LINEAR
                    ,   false
                };
            }
            else if constexpr (std::is_base_of_v< share_ref, T_COMPONENT> )
            {
                static_assert( std::is_base_of_v<share, T_COMPONENT::type> );
                return descriptor
                {
                      T_COMPONENT::key
                    , &MakeDescriptor<T_COMPONENT::type>()
                    , -1
                    , type::SHARE_REF
                    , nullptr
                    , nullptr
                    , nullptr
                    , nullptr
                    , 0u
                    , 0u
                    , type_data_access::ENUM_COUNT
                    , false
                };
            }
        }

        template< typename T_COMPONENT >
        static constexpr auto descriptor_variable_v = details::MakeDescriptor<T_COMPONENT>();
    }

    template< typename T_COMPONENT >
    static constexpr auto& descriptor_v = details::descriptor_variable_v<std::remove_const_t<xcore::types::decay_full_t<T_COMPONENT>>>;

    //----------------------------------------------------------------------------
    // TOOLS
    //----------------------------------------------------------------------------
    template< typename...T_DATA_AND_TAGS >
    using tuple_tag_components = xcore::types::tuple_cat_t< std::conditional_t< std::is_base_of_v<tag, T_DATA_AND_TAGS>, std::tuple<T_DATA_AND_TAGS>, std::tuple<> > ... >;

    template< typename...T_DATA_AND_TAGS >
    using tuple_data_share_components = xcore::types::tuple_cat_t< std::conditional_t< std::is_base_of_v<tag, T_DATA_AND_TAGS> == false, std::tuple<T_DATA_AND_TAGS>, std::tuple<> > ... >;

    template< typename...T_DATA_AND_TAGS >
    using tuple_share_components = xcore::types::tuple_cat_t< std::conditional_t< std::is_base_of_v<share, T_DATA_AND_TAGS>, std::tuple<T_DATA_AND_TAGS>, std::tuple<> > ... >;

    template< typename...T_DATA_AND_TAGS >
    using tuple_data_components = xcore::types::tuple_cat_t< std::conditional_t< std::is_base_of_v<share, T_DATA_AND_TAGS> == false && std::is_base_of_v<tag, T_DATA_AND_TAGS> == false, std::tuple<T_DATA_AND_TAGS>, std::tuple<> > ... >;

    template< typename T_COMPARE_A, typename T_COMPARE_B >
    struct smaller_guid
    {;
        static constexpr auto value   = component::descriptor_v<T_COMPARE_A>.m_Guid.m_Value < component::descriptor_v<T_COMPARE_B>.m_Guid.m_Value;
    };

    //----------------------------------------------------------------------------
    // DATA BASES
    //----------------------------------------------------------------------------

    struct descriptors_data_base
    {
        using map_descriptors = mecs::tools::fixed_map<component::type_guid, const component::descriptor*, mecs::settings::max_component_types_v>;

        void Init( void )
        {
            registerComponent<entity>();
        }

        template< typename T_COMPONENT > xforceinline
        void registerComponent(void) noexcept
        {
            bool b = m_mapDescriptors.alloc(component::descriptor_v<T_COMPONENT>.m_Guid, [&](const component::descriptor*& Value) constexpr noexcept
            {
                m_lDescriptors.push_back(&component::descriptor_v<T_COMPONENT>);
                Value = &component::descriptor_v<T_COMPONENT>;
                Value->m_BitNumber = Value->m_Type == type::TAG ? m_TagUniqueBitGenerator++ : m_DataUniqueBitGenerator++;
            });
            xassert(b==false);
        }

        map_descriptors                             m_mapDescriptors        {};
        std::vector<const component::descriptor*>   m_lDescriptors          {};
        std::uint16_t                               m_TagUniqueBitGenerator {0};
        std::uint16_t                               m_DataUniqueBitGenerator{0};
    };
}