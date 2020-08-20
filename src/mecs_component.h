
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
        static constexpr std::uint64_t   getKey(const void*) noexcept { return 0; }
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

    // Possibly new type of components. Still pending definition details...
    struct reference
    {
        constexpr static auto   type_guid_v         { nullptr };
        constexpr static auto   name_v              { xconst_universal_str("unnamed reference") };
        using                   reference_t         = std::tuple<>; // (must have, including tags) Archetype type definition (ex: render mesh)
        entity::guid m_Entity{ nullptr };
        static constexpr std::uint64_t getKey(const void* p) noexcept { return static_cast<const reference*>(p)->m_Entity.m_Value; }
    };

    // Possibly new type of components. Still pending definition details...
    struct share_reference : share
    {
        constexpr static auto   type_guid_v         { nullptr };
        constexpr static auto   name_v              { xconst_universal_str("unnamed share reference") };
        using                   reference_t         = std::tuple<>; // (must have, including tags) Archetype type definition (ex: render mesh)
        entity::guid m_Entity{ nullptr };
        static constexpr std::uint64_t   getKey(const void* p) noexcept { return static_cast<const share_reference*>(p)->m_Entity.m_Value; }
    };

    //----------------------------------------------------------------------------
    // DESCRIPTOR
    //----------------------------------------------------------------------------
    struct descriptor;

    struct descriptor
    {
        using fn_construct          = void(void*)                   noexcept;
        using fn_destruct           = void(void*)                   noexcept;
        using fn_move               = void(void*, void*)            noexcept;
        using fn_getkey             = std::uint64_t(const void*)    noexcept;

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
                        T_COMPONENT::type_guid_v.isValid() ? T_COMPONENT::type_guid_v : type_guid{ __FUNCSIG__ }
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
                    ,   (&T_COMPONENT::getKey == &mecs::component::share::getKey)? []( const void* pData ) constexpr noexcept { xcore::crc<64> X{}; return X.FromBytes( {static_cast<const std::byte*>(pData), sizeof(T_COMPONENT)} ).m_Value; } : &T_COMPONENT::getKey
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
            bool b = m_mapDescriptors.alloc(component::descriptor_v<T_COMPONENT>.m_Guid, [&](const component::descriptor*& pValue) constexpr noexcept
            {
                auto& Desc = component::descriptor_v<T_COMPONENT>;
                Desc.m_BitNumber = Desc.m_Type == type::TAG ? m_TagUniqueBitGenerator++ : m_DataUniqueBitGenerator++;

                m_lDescriptors.push_back(&Desc);
                pValue = &Desc;
            });
            xassert(b==false);
        }

        map_descriptors                             m_mapDescriptors        {};
        std::vector<const component::descriptor*>   m_lDescriptors          {};
        std::uint16_t                               m_TagUniqueBitGenerator {0};
        std::uint16_t                               m_DataUniqueBitGenerator{0};
    };
}