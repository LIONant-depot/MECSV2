
#include "xcore.h"

namespace mecs::archetype
{
    struct instance;
    struct specialized_pool;
    using index = std::uint32_t;
}

namespace mecs::world
{
    struct instance;
}

namespace mecs::sync_point
{
    struct instance;
}

namespace mecs::system
{
    struct instance;
}

namespace mecs::archetype
{
    struct instance;
}

namespace mecs::entity_pool
{
    class instance;
    using index = std::uint32_t;
}

/*
namespace mecs::event
{
    struct instance;
}
*/
namespace mecs::graph
{
    struct instance;
}

namespace mecs::universe
{
    struct instance;
}


#include "mecs_settings.h"
#include "mecs_tools.h"
#include "mecs_tools_bits.h"
#include "mecs_tools_fixed_map.h"
#include "mecs_tools_event.h"
#include "mecs_tools_qvst_pool.h"
#include "mecs_component.h"
#include "mecs_entity_pool.h"
#include "mecs_system_events.h"
#include "mecs_system_delegate.h"
#include "mecs_sync_point.h"
#include "mecs_archetype_event.h"
#include "mecs_archetype_query.h"
#include "mecs_archetype.h"
#include "mecs_archetype_delegate.h"
#include "mecs_delegate.h"
#include "mecs_time.h"
#include "mecs_system.h"
#include "mecs_graph.h"
#include "mecs_world.h"
#include "mecs_universe.h"


#include "Implementation/mecs_entity_pool_inline.h"
#include "Implementation/mecs_archetype_inline.h"
#include "Implementation/mecs_graph_inline.h"
#include "Implementation/mecs_system_inline.h"

namespace mecs
{
    struct velocity : component::data
    {
        constexpr static auto   name_v              { xconst_universal_str("velocity") };
        constexpr static auto   type_guid_v         { component::type_guid("velocity tag") };
        constexpr static auto   type_data_access_v  { component::type_data_access::DOUBLE_BUFFER };

        xcore::vector3 m_Value;
    };

    struct select : component::tag
    {
        constexpr static component::type_guid   type_guid_v { "selected tag" };
        constexpr static auto                   name_v      { xconst_universal_str("Selected") };
    };

    //
    // Describing a hierarchy in MECS
    //
    struct parent : component::data
    {
        constexpr static auto   name_v              { xconst_universal_str("Parent") };
        constexpr static auto   type_guid_v         { component::type_guid("Parent to another component") };

        component::entity::map::entry*              m_pValue;
    };

    struct children : component::data
    {
        constexpr static auto   name_v              { xconst_universal_str("ChildrenList") };
        constexpr static auto   type_guid_v         { component::type_guid("List of Component Children") };
        constexpr static auto   type_data_access_v  { component::type_data_access::QUANTUM };

        xcore::lock::object< std::vector<component::entity::map::entry*>, xcore::lock::semaphore> m_List{};
    };

    struct special_group : children
    {
        constexpr static auto   name_v              { xconst_universal_str("SpecialGroup") };
        constexpr static auto   type_guid_v         { component::type_guid("SpecialGroup") };
    };
}