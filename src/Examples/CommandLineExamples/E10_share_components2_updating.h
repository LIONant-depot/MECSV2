namespace mecs::examples::E10_shared_components2_updating
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

    struct updating_share_system : mecs::system::instance
    {
        using instance::instance;
        void operator()( mesh_render& MeshRender ) noexcept
        {
            MeshRender.m_nVertices++;
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

        auto upUniverse = std::make_unique<mecs::universe::instance>();
        upUniverse->Init();

        //------------------------------------------------------------------------------------------
        // Registration
        //------------------------------------------------------------------------------------------

        //
        // Register Components
        //
        upUniverse->registerTypes<position, velocity, mesh_render>();

        //
        // Which world we are building?
        //
        auto& DefaultWorld = *upUniverse->m_WorldDB[0];

        //
        // Create the game graph.
        //
        auto& Syncpoint = DefaultWorld.m_GraphDB.CreateSyncPoint();
        auto& System    = DefaultWorld.m_GraphDB.CreateGraphConnection<updating_share_system>(DefaultWorld.m_GraphDB.m_StartSyncPoint, Syncpoint);
                          DefaultWorld.m_GraphDB.CreateGraphConnection<render_system>  (Syncpoint, DefaultWorld.m_GraphDB.m_EndSyncPoint);
                       
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
        Archetype.CreateEntities(System, 2, {}, mesh_render{ {}, 10, 101, 102 } )
            ([&](position& Position, velocity& Velocity )
            {
                Position.m_Value.m_X = Rnd.RandF32(-100.0f, 100.0f );
                Position.m_Value.m_Y = Rnd.RandF32(-100.0f, 100.0f);

                Velocity.m_Value.m_X = Rnd.RandF32(-1.0f, 1.0f);
                Velocity.m_Value.m_Y = Rnd.RandF32(-1.0f, 1.0f);
                Velocity.m_Value.NormalizeSafe();
            });

        Archetype.CreateEntities(System, 2, {}, mesh_render{ {}, 22, 22, 22 } )
            ([&](position& Position, velocity& Velocity )
            {
                Position.m_Value.m_X = Rnd.RandF32(-100.0f, 100.0f );
                Position.m_Value.m_Y = Rnd.RandF32(-100.0f, 100.0f);

                Velocity.m_Value.m_X = Rnd.RandF32(-1.0f, 1.0f);
                Velocity.m_Value.m_Y = Rnd.RandF32(-1.0f, 1.0f);
                Velocity.m_Value.NormalizeSafe();
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