namespace mecs::examples::E03_system_moving_entities
{
    //-----------------------------------------------------------------------------------------
    // Components
    //-----------------------------------------------------------------------------------------
    // In this example we show how you can derived components from other structures.
    // Also note how we created the type_guid_v. We initialize with a unique string.
    struct position : mecs::component::data
    {
        static constexpr auto type_guid_v = type_guid{ "position component" };
        xcore::vector2 m_Value;
    };

    // Note that if you are not going to save the data you can ignore the guid.
    // But it is a bad practice 
    struct velocity : mecs::component::data
    {
        xcore::vector2 m_Value;
    };

    //-----------------------------------------------------------------------------------------
    // Systems
    //-----------------------------------------------------------------------------------------
    struct move_system : mecs::system::instance
    {
        using mecs::system::instance::instance;

        // in this example we show how you can access the entity component itself. Just like
        // any other type of component we should use const property. The entity component
        // is not used very often and its main job is to hold a Global Unique Identifier.
        // This GUID is how you can differentiate amount entities. 
        void operator()( const entity& Entity, position& Position, const velocity& Velocity ) const noexcept
        {
            Position.m_Value += Velocity.m_Value * getTime().m_DeltaTime;
            
            printf( "%I64X moved to {.x = %f, .y = %f}\n"
            , Entity.getGuid().m_Value
            , Position.m_Value.m_X
            , Position.m_Value.m_Y );
        }
    };

    //-----------------------------------------------------------------------------------------
    // Test
    //-----------------------------------------------------------------------------------------
    void Test()
    {
        printf( "--------------------------------------------------------------------------------\n");
        printf( "E03_system_moving_entities\n");
        printf( "--------------------------------------------------------------------------------\n");

        auto upUniverse = std::make_unique<mecs::universe::instance>();
        upUniverse->Init();

        //------------------------------------------------------------------------------------------
        // Registration
        //------------------------------------------------------------------------------------------

        //
        // Register Components
        //
        upUniverse->registerTypes<position, velocity>();

        //
        // Create the game graph.
        // 
        auto& DefaultWorld = *upUniverse->m_WorldDB[0];
        auto& System       = DefaultWorld.m_GraphDB.CreateGraphConnection<move_system>(DefaultWorld.m_GraphDB.m_StartSyncPoint, DefaultWorld.m_GraphDB.m_EndSyncPoint);

        //------------------------------------------------------------------------------------------
        // Initialization
        //------------------------------------------------------------------------------------------

        //
        // Create an archetype. The entities in this archetype will have two components, position and velocity.
        //
        auto& Archetype = DefaultWorld.m_ArchetypeDB.getOrCreateArchitype<position, velocity>();

        //
        // Create an entity and initialize its components
        //
        Archetype.CreateEntity( System, []( position& Position, velocity& Velocity )
        {
            Position.m_Value.setup( 0.0f, 0.0f );
            Velocity.m_Value.setup( 1.0f, 1.0f );
        });

        //------------------------------------------------------------------------------------------
        // Running
        //------------------------------------------------------------------------------------------

        //
        // Start executing the world
        //
        DefaultWorld.Start();

        //
        // run 10 frames
        //
        for (int i = 0; i < 10; i++)
            DefaultWorld.Resume();

        xassert(true);
    }

}