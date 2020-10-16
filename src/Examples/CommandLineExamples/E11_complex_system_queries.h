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

        // For complex queries we can use (query_t) this allows us to directly control which components
        // and entity should have in order for it to be process by the system. query_t should have a
        // std::tuple with the following templates:
        // all<...list of components >  will tell the query which component must an entity have.
        // any<...list of components >  will tell the query which components an entity could have.
        // none<...list of components > will tell the query which components an entity can not have.
        using query_t = std::tuple
        <
            all<dynamic_tag>
        ,   none<dead_tag>
        >;

        xcore::random::small_generator Rnd;

        // This example show cases how one system can handle entities that may or may nor have velocities
        // or Health components. Note that the way we indicate that a component is optional is by asking
        // for its pointer. This mirrors good practices of C++. This helps reduce the number of systems
        // that you need. This type of pointers are call tentative references because you may get nothing.
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

                    // add a dead_tag component to our entity
                    getArchetypeBy<std::tuple<dead_tag>, std::tuple<>>(Entity, [&](archetype& Archetype)
                    {
                        // Temp removed
                        // moveEntityToArchetype(Entity, Archetype);
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

        auto    upUniverse      = std::make_unique<mecs::universe::instance>();
        auto&   DefaultWorld    = *upUniverse->m_WorldDB[0];

        //------------------------------------------------------------------------------------------
        // Registration
        //------------------------------------------------------------------------------------------

        //
        // Register Components
        //
        upUniverse->registerTypes<position, velocity, health, dead_tag, dynamic_tag>();

        //
        // Create the game graph.
        //
        DefaultWorld.CreateGraphConnection<explosion_system>(DefaultWorld.getStartSyncpoint(), DefaultWorld.getEndSyncpoint());
                       
        //------------------------------------------------------------------------------------------
        // Initialization
        //------------------------------------------------------------------------------------------

        //
        // Create an entity group
        //
        auto& DynamicArchetypeV = DefaultWorld.getOrCreateArchitype<position, health, velocity, dynamic_tag>();
        auto& DynamicArchetype  = DefaultWorld.getOrCreateArchitype<position, health, dynamic_tag>();
        auto& StaticArchetype   = DefaultWorld.getOrCreateArchitype<position>();

        //
        // Create entities
        //
        xcore::random::small_generator Rnd;

        DefaultWorld.CreateEntity(DynamicArchetypeV, []( health& Health, velocity& Velocity )
        {
            Health.m_Value = 4;
            Velocity.m_Value.m_X = 1;
        });

        DefaultWorld.CreateEntity(DynamicArchetypeV, []( health& Health, velocity& Velocity)
        {
            Health.m_Value = 7;
            Velocity.m_Value.m_Y = 1;
        });

        DefaultWorld.CreateEntity(DynamicArchetype, [](health& Health)
        {
            Health.m_Value = 8;
        });

        DefaultWorld.CreateEntity(StaticArchetype, []( position& Position )
        {
            Position.m_Value.m_X = 1;
            Position.m_Value.m_Y = 1;
        });

        //------------------------------------------------------------------------------------------
        // Running
        //------------------------------------------------------------------------------------------

        //
        // run 10 frames
        //
        for (int i = 0; i < 10; i++)
        {
            printf("\n");
            DefaultWorld.Play();
        }

        xassert(true);
    }

}