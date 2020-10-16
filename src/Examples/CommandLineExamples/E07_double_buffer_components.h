namespace mecs::examples::E07_double_buffer_components
{
    //-----------------------------------------------------------------------------------------
    // Components
    //-----------------------------------------------------------------------------------------
    // Double buffer components are design to made modifications while another system reads its
    // previous state in a safe way. Which means that there are two copies of the component.
    // One used for writing and the other used for reading. This can be thought of having to
    // different times [T0 - what is the current state] [T1 - What will be the future state]
    // T0 is the read only state and it must have a const. T1 is the writing only state and
    // it must not have a const. Only one system can have access to T1, but many systems can
    // have access to T0 as it does not change. T0 will become T1 and vice versa at the syncpoint. 
    struct position : mecs::component::data
    {
        static constexpr auto type_data_access_v = type_data_access::DOUBLE_BUFFER;
        int m_X{ 0 };
    };

    //-----------------------------------------------------------------------------------------
    // Systems
    //-----------------------------------------------------------------------------------------
    struct move_system : mecs::system::instance
    {
        using instance::instance;

        // This system can modify T1 position safely without worrying if there are other systems
        // reading T0.
        void operator() ( position& T1Position, const position& T0Position ) noexcept
        {
            T1Position.m_X = T0Position.m_X + 1;
        }
    };

    //-----------------------------------------------------------------------------------------

    struct print_position : mecs::system::instance
    {
        using instance::instance;

        // This system is reading T0 and it does not need to worry about people changing its value midway.
        void operator() ( const entity& Entity, const position& T0Position ) noexcept
        {
            printf( "%s moved to {.x = %d}\n"
                , Entity.getGUID().getStringHex<char>().c_str()
                , T0Position.m_X );
        }
    };

    //-----------------------------------------------------------------------------------------
    // Test
    //-----------------------------------------------------------------------------------------
    void Test()
    {
        printf( "--------------------------------------------------------------------------------\n");
        printf( "E07_double_buffer_components\n");
        printf( "--------------------------------------------------------------------------------\n");

        auto    upUniverse      = std::make_unique<mecs::universe::instance>();
        auto&   DefaultWorld    = *upUniverse->m_WorldDB[0];

        //------------------------------------------------------------------------------------------
        // Registration
        //------------------------------------------------------------------------------------------

        //
        // Register Components
        //
        upUniverse->registerTypes<position>();

        //
        // Create the game graph.
        // 
        DefaultWorld.CreateGraphConnection<move_system>     ( DefaultWorld.getStartSyncpoint(), DefaultWorld.getEndSyncpoint() );
        DefaultWorld.CreateGraphConnection<print_position>  ( DefaultWorld.getStartSyncpoint(), DefaultWorld.getEndSyncpoint() );

       //------------------------------------------------------------------------------------------
       // Initialization
       //------------------------------------------------------------------------------------------

       //
       // Create an entity group
       //
       auto& Archetype = DefaultWorld.m_ArchetypeDB.getOrCreateArchitype<position>();

       //
       // Create one entity
       //
       DefaultWorld.CreateEntity(Archetype);

       //------------------------------------------------------------------------------------------
       // Running
       //------------------------------------------------------------------------------------------

       //
       // run 10 frames
       //
       for (int i = 0; i < 10; i++)
           DefaultWorld.Play();

       xassert(true);
    }
}