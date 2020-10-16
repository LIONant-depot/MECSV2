namespace mecs::examples::E10_shared_components
{
    //-----------------------------------------------------------------------------------------
    // Components
    //-----------------------------------------------------------------------------------------

    struct mesh_render : mecs::component::share
    {
        int m_nIndices  { 100 };
        int m_nVertices { 20  };
        int m_nTextures { 66  };
    };

    struct velocity : mecs::component::data
    {
        xcore::vector2 m_Value;
    };

    struct position : mecs::component::data
    {
        xcore::vector2 m_Value;
    };

    //-----------------------------------------------------------------------------------------
    // Systems
    //-----------------------------------------------------------------------------------------
    struct render_system : mecs::system::instance
    {
        using instance::instance;
        void operator()( const entity& Entity, const position& Position, const mesh_render& MeshRender ) noexcept
        {
            printf("%s moved to {%f, %f} render with nVerts:%d\n"
                , Entity.getGUID().getStringHex<char>().c_str()
                , Position.m_Value.m_X
                , Position.m_Value.m_Y
                , MeshRender.m_nVertices );
        }
    };

    struct movement_system : mecs::system::instance
    {
        using instance::instance;
        void operator()( position& Position, const velocity& Velocity ) noexcept
        {
            Position.m_Value += Velocity.m_Value * getTime().m_DeltaTime;
        }
    };

    //-----------------------------------------------------------------------------------------
    // Test
    //-----------------------------------------------------------------------------------------
    void Test()
    {
        printf("--------------------------------------------------------------------------------\n");
        printf("E10_shared_components\n");
        printf("--------------------------------------------------------------------------------\n");

        auto    upUniverse      = std::make_unique<mecs::universe::instance>();
        auto&   DefaultWorld    = *upUniverse->m_WorldDB[0];

        //------------------------------------------------------------------------------------------
        // Registration
        //------------------------------------------------------------------------------------------

        //
        // Register Components
        //
        upUniverse->registerTypes<position, velocity, mesh_render>();

        //
        // Create the game graph.
        //
        auto& Syncpoint = DefaultWorld.CreateSyncPoint();
        DefaultWorld.CreateGraphConnection<movement_system>( DefaultWorld.getStartSyncpoint(),  Syncpoint );
        DefaultWorld.CreateGraphConnection<render_system>  ( Syncpoint,                         DefaultWorld.getEndSyncpoint() );
                       
        //------------------------------------------------------------------------------------------
        // Initialization
        //------------------------------------------------------------------------------------------

        //
        // Create an entity group
        //
        auto& Archetype = DefaultWorld.m_ArchetypeDB.getOrCreateArchitype<position, velocity, mesh_render>();

        //
        // Create entities
        //
        xcore::random::small_generator Rnd;
        DefaultWorld.CreateEntities( Archetype, 2, {}
            , [&]( position& Position, velocity& Velocity )
            {
                Position.m_Value.m_X = Rnd.RandF32(-100.0f, 100.0f );
                Position.m_Value.m_Y = Rnd.RandF32(-100.0f, 100.0f);

                Velocity.m_Value.m_X = Rnd.RandF32(-1.0f, 1.0f);
                Velocity.m_Value.m_Y = Rnd.RandF32(-1.0f, 1.0f);
                Velocity.m_Value.NormalizeSafe();
            }
            , mesh_render{ {}, 10, 101, 102 } );

        DefaultWorld.CreateEntities(Archetype, 2, {}
            , [&](position& Position, velocity& Velocity )
            {
                Position.m_Value.m_X = Rnd.RandF32(-100.0f, 100.0f );
                Position.m_Value.m_Y = Rnd.RandF32(-100.0f, 100.0f);

                Velocity.m_Value.m_X = Rnd.RandF32(-1.0f, 1.0f);
                Velocity.m_Value.m_Y = Rnd.RandF32(-1.0f, 1.0f);
                Velocity.m_Value.NormalizeSafe();
            }
            , mesh_render{ {}, 22, 22, 22 } );

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