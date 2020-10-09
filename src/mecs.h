
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
    struct instance;
    using index = std::uint32_t;
}

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
#include "mecs_time.h"
#include "mecs_system.h"
#include "mecs_graph.h"
#include "mecs_world.h"
#include "mecs_universe.h"


#include "Implementation/mecs_entity_pool_inline.h"
#include "Implementation/mecs_archetype_inline.h"
#include "Implementation/mecs_archetype_query_inline.h"
#include "Implementation/mecs_system_inline.h"
#include "Implementation/mecs_graph_inline.h"
#include "Implementation/mecs_archetype_delegate_inline.h"
#include "Implementation/mecs_system_delegate_inline.h"
