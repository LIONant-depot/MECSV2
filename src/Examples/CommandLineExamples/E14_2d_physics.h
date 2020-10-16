namespace mecs::examples::E14_2d_physics
{
    //-----------------------------------------------------------------------------------------
    // Components
    //-----------------------------------------------------------------------------------------

    struct position : mecs::component::data
    {
        static constexpr auto type_data_access_v = type_data_access::QUANTUM_DOUBLE_BUFFER;
        xcore::vector2 m_Value;
    };

    struct velocity : mecs::component::data
    {
        static constexpr auto type_data_access_v = type_data_access::QUANTUM_DOUBLE_BUFFER;
        xcore::vector2 m_Value;
    };

    struct collider : mecs::component::share
    {
        float m_Radius;
    };

    struct cell_group : mecs::component::share
    {
        // one key to one entity (one to one)
        constexpr static auto type_map_v = type_map::ONE_TO_ONE;
        static constexpr std::uint64_t getKey( const void* p ) noexcept
        {
            auto& R = *static_cast<const cell_group*>(p);
            return (18446744073709551557ull ^ 9223372036854775ull) ^ (static_cast<std::uint64_t>(R.m_X) << 32) ^ static_cast<std::uint64_t>(R.m_Y);
        }

        std::uint32_t m_X;
        std::uint32_t m_Y;
    };

    struct cell_list : mecs::component::singleton
    {
        std::vector<mecs::archetype::specialized_pool*> m_List;
    };

    //-----------------------------------------------------------------------------------------
    // Delegates
    //-----------------------------------------------------------------------------------------

    struct create_cell_entity : mecs::archetype::delegate::instance< mecs::archetype::event::create_pool >
    {
        void operator()( system& System, pool& Pool, const cell_group& Cell )
        {
            int a = 22;
        }
    };

    struct destroy_cell_entity : mecs::archetype::delegate::instance< mecs::archetype::event::destroy_pool >
    {
        void operator()( system& System, pool& Pool, const cell_group& Cell )
        {
            int a = 22;
        }
    };

    //-----------------------------------------------------------------------------------------
    // Systems
    //-----------------------------------------------------------------------------------------
    struct update_hierarchy_system : mecs::system::instance
    {
        using instance::instance;

        void msgUpdate( void ) noexcept
        {
            // Collect all the archetypes that have a cell
            m_World.m_ArchetypeDB.template DoQuery
            < 
                void            // We don't have any function that provides additional filters
            ,   std::tuple      // We want all archetypes with cell components
                <
                    all< cell_group >
                >
            >( instance::m_Query );

            // 

        }
    };

    //-----------------------------------------------------------------------------------------
    // Test
    //-----------------------------------------------------------------------------------------
    void Test()
    {
        printf("--------------------------------------------------------------------------------\n");
        printf("E14_physics\n");
        printf("--------------------------------------------------------------------------------\n");

        auto    upUniverse      = std::make_unique<mecs::universe::instance>();
        auto&   DefaultWorld    = *upUniverse->m_WorldDB[0];

        //------------------------------------------------------------------------------------------
        // Registration
        //------------------------------------------------------------------------------------------

        //
        // Register Components
        //
        upUniverse->registerTypes<position, velocity, collider, cell_group >();

        //
        // Create the game graph.
        //
        auto& Syncpoint = DefaultWorld.m_GraphDB.CreateSyncPoint();
        DefaultWorld.CreateGraphConnection<update_hierarchy_system> ( DefaultWorld.getStartSyncpoint(), Syncpoint );
        DefaultWorld.CreateGraphConnection<update_hierarchy_system> ( DefaultWorld.getStartSyncpoint(), Syncpoint );

        //DefaultWorld.CreateDelegate<destroy_cell_entity>();


        /*

        //------------------------------------------------------------------------------------------
        // Initialization
        //------------------------------------------------------------------------------------------

        //
        // Create an entity group
        //
        auto& Archetype = DefaultWorld.m_ArchetypeDB.getOrCreateArchitype<position, velocity, collider, cell >();

        //
        // Create entities
        //
        Archetype.CreateEntity( System, [&]( mecs::component::entity& ParentEntity, children& ChildrenComponent ) noexcept
        {
        });
        */

    }
}