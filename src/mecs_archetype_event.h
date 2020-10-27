namespace mecs::archetype::event
{
    using type_guid = xcore::guid::unit<64, struct type_tag>;

    namespace details
    {
        struct base_event
        {
            using                   event_t         = mecs::tools::event< mecs::component::entity&, mecs::system::instance& >;
        };
    }

    //---------------------------------------------------------------------------------
    // ARCHETYPE:: EVENTS
    //---------------------------------------------------------------------------------
    struct create_entity final : details::base_event
    {
        constexpr static auto   type_guid_v         = type_guid                 { "mecs::archetype::event::create_entity" };
        constexpr static auto   type_name_v         = xconst_universal_str      ( "mecs::archetype::event::create_entity" );
    };

    struct destroy_entity final : details::base_event
    {
        constexpr static auto   type_guid_v         = type_guid                 { "mecs::archetype::event::destroy_entity" };
        constexpr static auto   type_name_v         = xconst_universal_str      ( "mecs::archetype::event::destroy_entity" );
    };

    struct moved_in final : details::base_event
    {
        constexpr static auto   type_guid_v         = type_guid                 { "mecs::archetype::event::moved_in" };
        constexpr static auto   type_name_v         = xconst_universal_str      ( "mecs::archetype::event::moved_in" );
    };

    struct moved_out final : details::base_event
    {
        constexpr static auto   type_guid_v         = type_guid                 { "mecs::archetype::event::moved_out" };
        constexpr static auto   type_name_v         = xconst_universal_str      ( "mecs::archetype::event::moved_out" );
    };

    struct create_pool final : details::base_event
    {
        using                   event_t             = mecs::tools::event< mecs::system::instance&, mecs::archetype::pool& >;
        constexpr static auto   type_guid_v         = type_guid                 { "mecs::archetype::event::create_pool" };
        constexpr static auto   type_name_v         = xconst_universal_str      ( "mecs::archetype::event::create_pool" );
    };

    struct destroy_pool final : details::base_event
    {
        using                   event_t             = mecs::tools::event< mecs::system::instance&, mecs::archetype::pool& >;
        constexpr static auto   type_guid_v         = type_guid                 { "mecs::archetype::event::destroy_pool" };
        constexpr static auto   type_name_v         = xconst_universal_str      ( "mecs::archetype::event::destroy_pool" );
    };

    template< typename...T_COMPONENTS >
    struct updated_component final : details::base_event
    {
        using                   components_t        = std::tuple<T_COMPONENTS...>;
        static_assert(std::is_same_v< components_t, std::tuple<void> > == false);

        constexpr static auto   type_guid_v         = type_guid                 { "mecs::archetype::event::updated_component" };
        constexpr static auto   type_name_v         = xconst_universal_str      ( "mecs::archetype::event::updated_component" );
        constexpr static auto   components_v        = std::array<const mecs::component::descriptor*, sizeof...(T_COMPONENTS)>{ &mecs::component::descriptor_v<T_COMPONENTS>... };
    };

    using updated_entity = updated_component<>;

    //---------------------------------------------------------------------------------
    //
    //---------------------------------------------------------------------------------
    namespace details
    {
        struct events
        {
            using created_entity  = xcore::types::make_unique< mecs::archetype::event::create_entity::event_t,   struct create_entity_tag        >;
            using destroy_entity  = xcore::types::make_unique< mecs::archetype::event::destroy_entity::event_t,  struct destroy_entity_tag       >;
            using move_out_entity = xcore::types::make_unique< mecs::archetype::event::moved_out::event_t,       struct move_out_entity_tag      >;
            using move_in_entity  = xcore::types::make_unique< mecs::archetype::event::moved_in::event_t,        struct move_in_entity_tag       >;
            using created_pool    = xcore::types::make_unique< mecs::archetype::event::create_pool::event_t,     struct create_pool_tag          >;
            using destroy_pool    = xcore::types::make_unique< mecs::archetype::event::destroy_pool::event_t,    struct destroy_pool_tag         >;

            struct updated_component
            {
                using event_t = xcore::types::make_unique< event::updated_component<>::event_t, struct updated_component_tag    >;
                event_t                     m_Event;
                std::vector<tools::bits>    m_Bits;
            };

            created_entity              m_CreatedEntity;
            destroy_entity              m_DestroyEntity;
            move_out_entity             m_MovedOutEntity;
            move_in_entity              m_MovedInEntity;
            created_pool                m_CreatedPool;
            destroy_pool                m_DestroyPool;
            updated_component           m_UpdateComponent;
        };
    }
}