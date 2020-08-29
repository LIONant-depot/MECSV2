namespace mecs::examples::E06_global_system_events 
{
    //-----------------------------------------------------------------------------------------
    // Components
    //-----------------------------------------------------------------------------------------
    struct position : mecs::component::data
    {
        int m_X{ 0 };
    };

    //-----------------------------------------------------------------------------------------
    // General Custom Events
    //-----------------------------------------------------------------------------------------
    struct my_sound_event : mecs::system::event::global
    {
        using   real_event_t = define_real_event<const char*>;
    };

    //-----------------------------------------------------------------------------------------
    // Systems
    //-----------------------------------------------------------------------------------------
    struct move_system : mecs::system::instance
    {
        using instance::instance;

        using events_t = std::tuple
        <
            my_sound_event
        >;

        void operator() ( const entity& Entity, position& Position ) noexcept
        {
            Position.m_X ++;

            if( Position.m_X == 10 )
            {
                // Spawn a new entity with position
               // createEntity<position>();
            }

            // Tell where is our entity
            printf("Entity[%I64X] Walking (%d) \n", Entity.getGuid().m_Value, Position.m_X );

            // Send general events
            EventNotify< my_sound_event, move_system >( "Step sound.wav" );
        }
    };

    //-----------------------------------------------------------------------------------------
    // General Custom Delegates
    //-----------------------------------------------------------------------------------------
    // These delegates are exactly the same as the custom delegates
    //-----------------------------------------------------------------------------------------
    struct my_sound_delegate : mecs::system::delegate::instance< my_sound_event >
    {
        static void PlaySound( const char* pSoundFile ) noexcept
        {
            printf("Playing a sound: %s \n", pSoundFile );
        }

        void operator() ( mecs::system::instance& System, const char* pSoundFile ) const noexcept
        {
            PlaySound(pSoundFile);
        }
    };

    //-----------------------------------------------------------------------------------------
    // Delegates
    //-----------------------------------------------------------------------------------------
    struct spawn_delegate : mecs::archetype::delegate::instance< mecs::archetype::event::create_entity >
    {
        // Any entity will trigger this sound right now... we could filter which kinds of entities we want.
        void operator() ( mecs::system::instance& System ) const noexcept
        {
            // Route all sound playing to the sound_delegate
            my_sound_delegate::PlaySound("Spawn Sound.wav");
        }
    };

    //-----------------------------------------------------------------------------------------
    // Test
    //-----------------------------------------------------------------------------------------
    void Test()
    {
        printf("--------------------------------------------------------------------------------\n");
        printf("E06_global_system_events\n");
        printf("--------------------------------------------------------------------------------\n");

        auto upUniverse = std::make_unique<mecs::universe::instance>();
        upUniverse->Init();

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
        auto& DefaultWorld = *upUniverse->m_WorldDB[0];
        auto& System = DefaultWorld.m_GraphDB.CreateGraphConnection<move_system>(DefaultWorld.m_GraphDB.m_StartSyncPoint, DefaultWorld.m_GraphDB.m_EndSyncPoint);

        //
        // Create the delegates.
        //
        DefaultWorld.m_GraphDB.CreateSystemDelegate< my_sound_delegate >();
        DefaultWorld.m_GraphDB.CreateArchetypeDelegate< spawn_delegate >();

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
        Archetype.CreateEntity(System);

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