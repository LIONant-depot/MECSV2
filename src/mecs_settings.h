
namespace mecs::settings
{
    static constexpr std::uint32_t max_entities_v                   = xcore::bits::RoundToNextPowOfTwo(10000000ull)
                                                                        XCORE_CMD_ASSERT(-xcore::bits::RoundToNextPowOfTwo(10000000ull)
                                                                                         + xcore::bits::RoundToNextPowOfTwo(1000000ull));
    static constexpr std::uint32_t max_component_types_v            = 10000;
    static constexpr std::uint32_t max_data_components_per_entity   = 64;
    static constexpr std::uint32_t max_tag_components_per_entity    = 32;
    static constexpr std::uint32_t max_archetype_types              = 100;
    static constexpr std::uint32_t max_syncpoints_per_system        = 4;
    static constexpr std::uint32_t max_default_entities_per_pool    = 100000;
    static constexpr std::uint32_t max_systems                      = 1000;
    static constexpr std::uint32_t max_graph_connections            = 8;
    static constexpr std::uint32_t max_sync_points                  = 512;
    static constexpr std::uint32_t max_event_types                  = 1024;
    static constexpr std::uint32_t max_archetype_delegates          = 1024;
    static constexpr std::uint32_t max_sytem_delegates              = 1024;
    static constexpr std::uint32_t max_specialized_pools_v          = 10000;

    constexpr static std::uint32_t fixed_deltatime_microseconds_v   = 16 * 1000;
}
