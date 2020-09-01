namespace mecs::examples::E11_complex_system_queries
{
    //-----------------------------------------------------------------------------------------
    // Components
    //-----------------------------------------------------------------------------------------
    struct position : mecs::component::data
    {
        xcore::vector2 m_Value;
    };

    struct velocity : mecs::component::data
    {
        xcore::vector2 m_Value;
    };

    struct health : mecs::component::data
    {
        float m_Value;
    };

    struct dynamic_tag : mecs::component::tag
    {};

    struct dead_tag : mecs::component::tag
    {};

    //-----------------------------------------------------------------------------------------
    // Systems
    //-----------------------------------------------------------------------------------------
    struct explosion_system : mecs::system::instance
    {
        using instance::instance;

        using query_t = std::tuple
        <
            all<dynamic_tag>
        ,   none<dead_tag>
        >;

        xcore::random::small_generator Rnd;

        void operator() ( entity& Entity, position& Position, const velocity* pVelocity, health* pHealth ) noexcept
        {
            xcore::vector2 RndVel = { Rnd.RandF32(-1, 1 ), Rnd.RandF32(-1, 1) };
            std::array<char,256> String;
            int                  L = 0;

            RndVel.NormalizeSafe();

            // if we have a velocity component we will take it into consideration
            if(pVelocity)
            {
                Position.m_Value += RndVel * pVelocity->m_Value * getTime().m_DeltaTime;
                L += sprintf_s( &String[L], String.size() - L, "Shaking Movable Entity %s ", Entity.getGUID().getStringHex<char>().c_str());
            }
            else
            {
                Position.m_Value += RndVel * getTime().m_DeltaTime;
                L += sprintf_s( &String[L], String.size() - L, "Shaking unMovable Entity %s ", Entity.getGUID().getStringHex<char>().c_str());
            }

            // Entities with health will get damage
            if( pHealth ) 
            {
                pHealth->m_Value -= 1.0f;
                if( pHealth->m_Value <= 0 )
                {
                    L += sprintf_s( &String[L], String.size() - L, "Killing Entity...");
                    getArchetypeBy<std::tuple<dead_tag>, std::tuple<>>(Entity, [&](archetype& Archetype)
                    {
                        moveEntityToArchetype(Entity, Archetype);
                    });
                }
                else
                {
                    L += sprintf_s( &String[L], String.size() - L, "With health %f ", pHealth->m_Value );
                }
            }

            printf("%s\n", String.data() );
        }
    };

    //-----------------------------------------------------------------------------------------
    // Test
    //-----------------------------------------------------------------------------------------
    void Test()
    {
        printf("--------------------------------------------------------------------------------\n");
        printf("E11_complex_system_queries\n");
        printf("--------------------------------------------------------------------------------\n");

        auto upUniverse = std::make_unique<mecs::universe::instance>();
        upUniverse->Init();

        //------------------------------------------------------------------------------------------
        // Registration
        //------------------------------------------------------------------------------------------

        //
        // Register Components
        //
        upUniverse->registerTypes<position, velocity, health, dead_tag, dynamic_tag>();

        //
        // Which world we are building?
        //
        auto& DefaultWorld = *upUniverse->m_WorldDB[0];

        //
        // Create the game graph.
        //
        auto& System    = DefaultWorld.m_GraphDB.CreateGraphConnection<explosion_system>(DefaultWorld.m_GraphDB.m_StartSyncPoint, DefaultWorld.m_GraphDB.m_EndSyncPoint);
                       
        //------------------------------------------------------------------------------------------
        // Initialization
        //------------------------------------------------------------------------------------------

        //
        // Create an entity group
        //
        auto& DynamicArchetypeV = DefaultWorld.m_ArchetypeDB.getOrCreateArchitype<position, health, velocity, dynamic_tag>();
        auto& DynamicArchetype  = DefaultWorld.m_ArchetypeDB.getOrCreateArchitype<position, health, dynamic_tag>();
        auto& StaticArchetype   = DefaultWorld.m_ArchetypeDB.getOrCreateArchitype<position>();

        //
        // Create entities
        //
        xcore::random::small_generator Rnd;

        DynamicArchetypeV.CreateEntity( System, []( health& Health, velocity& Velocity )
        {
            Health.m_Value = 4;
            Velocity.m_Value.m_X = 1;
        });

        DynamicArchetypeV.CreateEntity( System, []( health& Health, velocity& Velocity)
        {
            Health.m_Value = 7;
            Velocity.m_Value.m_Y = 1;
        });

        DynamicArchetype.CreateEntity(System, [](health& Health)
        {
            Health.m_Value = 8;
        });

        StaticArchetype.CreateEntity(System, []( position& Position )
        {
            Position.m_Value.m_X = 1;
            Position.m_Value.m_Y = 1;
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
        {
            printf("\n");
            DefaultWorld.Resume();
        }

        xassert(true);
    }

}