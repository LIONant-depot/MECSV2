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
    // System Global Events
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

        // Tells MECS which events this system can send
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
            printf("Entity[%s] Walking (%d) \n"
            , Entity.getGUID().getStringHex<char>().c_str()
            , Position.m_X );

            // Send general events
            EventNotify< my_sound_event, move_system >( "Step sound.wav" );
        }
    };

    //-----------------------------------------------------------------------------------------
    // a pretend global API for audio
    //-----------------------------------------------------------------------------------------
    static void PlaySound(const char* pSoundFile) noexcept
    {
        printf("Playing a sound: %s \n", pSoundFile);
    }

    //-----------------------------------------------------------------------------------------
    // General Custom Delegates
    //-----------------------------------------------------------------------------------------
    // These delegates are exactly the same as the custom delegates
    //-----------------------------------------------------------------------------------------
    struct my_sound_delegate : mecs::system::delegate::instance< my_sound_event >
    {
        void operator() ( system& System, const char* pSoundFile ) const noexcept
        {
            PlaySound(pSoundFile);
        }
    };

    //-----------------------------------------------------------------------------------------
    // Archetype Delegates
    //-----------------------------------------------------------------------------------------
    struct spawn_delegate : mecs::archetype::delegate::instance< mecs::archetype::event::create_entity >
    {
        // Any entity will trigger this sound right now... we could filter which kinds of entities we want.
        void operator() ( system& System ) const noexcept
        {
            // Route all sound playing to the sound_delegate
            PlaySound("Spawn Sound.wav");
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
        DefaultWorld.CreateGraphConnection<move_system>( DefaultWorld.getStartSyncpoint(), DefaultWorld.getEndSyncpoint() );

        //
        // Create the delegates.
        //
        DefaultWorld.CreateSystemDelegate< my_sound_delegate >();
        DefaultWorld.CreateArchetypeDelegate< spawn_delegate >();

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