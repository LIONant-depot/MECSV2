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

    template< typename...T_COMPONENTS >
    struct moved_in final : details::base_event
    {
        constexpr static auto   type_guid_v         = type_guid                 { "mecs::archetype::event::moved_in" };
        constexpr static auto   type_name_v         = xconst_universal_str      ( "mecs::archetype::event::moved_in" );
        using                   components_t        = std::tuple<T_COMPONENTS...>;
    };

    template< typename...T_COMPONENTS >
    struct moved_out final : details::base_event
    {
        constexpr static auto   type_guid_v         = type_guid                 { "mecs::archetype::event::moved_out" };
        constexpr static auto   type_name_v         = xconst_universal_str      ( "mecs::archetype::event::moved_out" );
        using                   components_t        = std::tuple<T_COMPONENTS...>;
    };

    template< typename...T_COMPONENTS >
    struct updated_component final : details::base_event
    {
        constexpr static auto   type_guid_v         = type_guid                 { "mecs::archetype::event::updated_component" };
        constexpr static auto   type_name_v         = xconst_universal_str      ( "mecs::archetype::event::updated_component" );
        using                   components_t        = std::tuple<T_COMPONENTS...>;
    };

    using updated_entity = updated_component<>;

    //---------------------------------------------------------------------------------
    //
    //---------------------------------------------------------------------------------
    namespace details
    {
        struct events
        {
            using created_entity  = xcore::types::make_unique< mecs::archetype::event::create_entity::event_t,   struct new_entity_tag           >;
            using deleted_entity  = xcore::types::make_unique< mecs::archetype::event::destroy_entity::event_t,  struct delete_entity_tag        >;
            using move_out_entity = xcore::types::make_unique< mecs::archetype::event::moved_out<void>::event_t, struct move_out_entity_tag      >;
            using move_in_entity  = xcore::types::make_unique< mecs::archetype::event::moved_in<void>::event_t,  struct move_in_entity_tag       >;

            struct updated_component
            {
                using event_t = xcore::types::make_unique< event::updated_component<void>::event_t, struct updated_component_tag    >;
                event_t                     m_Event;
                std::vector<tools::bits>    m_Bits;
            };

            created_entity              m_CreatedEntity;
            deleted_entity              m_DeletedEntity;
            move_out_entity             m_MovedOutEntity;
            move_in_entity              m_MovedInEntity;
            updated_component           m_UpdateComponent;
        };
    }
}