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

    struct group1 : mecs::component::share
    {
        int m_Value{ 100 };
    };

    struct group2 : mecs::component::share
    {
        int m_Value{ 100 };
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
        void operator()( const entity& Entity, const position& Position, const mesh_render& MeshRender, const group1& Group1, const group2& Group2) noexcept
        {
            printf("%s nVerts:%d, Val1=%d, Val2=%d \n"
                , Entity.getGUID().getStringHex<char>().c_str()
                , MeshRender.m_nVertices
                , Group1.m_Value
                , Group2.m_Value );
        }
    };

    struct updating_share_system : mecs::system::instance
    {
        using instance::instance;
        void operator()( const mesh_render& MeshRender, group1& Group1, group2& Group2 ) noexcept
        {
            Group1.m_Value++;
            Group2.m_Value++;
        }
    };

    //-----------------------------------------------------------------------------------------
    // Test
    //-----------------------------------------------------------------------------------------
    void Test()
    {
        printf("--------------------------------------------------------------------------------\n");
        printf("E10_shared_components2_updating\n");
        printf("--------------------------------------------------------------------------------\n");

        auto    upUniverse      = std::make_unique<mecs::universe::instance>();
        auto&   DefaultWorld    = *upUniverse->m_WorldDB[0];

        //------------------------------------------------------------------------------------------
        // Registration
        //------------------------------------------------------------------------------------------

        //
        // Register Components
        //
        upUniverse->registerTypes<position, velocity, mesh_render, group1, group2>();

        //
        // Create the game graph.
        //
        auto& Syncpoint = DefaultWorld.m_GraphDB.CreateSyncPoint();
        DefaultWorld.CreateGraphConnection<updating_share_system>   ( DefaultWorld.getStartSyncpoint(), Syncpoint );
        DefaultWorld.CreateGraphConnection<render_system>           ( Syncpoint,                        DefaultWorld.getEndSyncpoint() );

        //------------------------------------------------------------------------------------------
        // Initialization
        //------------------------------------------------------------------------------------------

        //
        // Create an entity group
        //
        auto& Archetype = DefaultWorld.getOrCreateArchitype<position, velocity, mesh_render, group1, group2>();

        //
        // Create entities
        //
        xcore::random::small_generator Rnd;
        DefaultWorld.CreateEntities( Archetype, 2, {},
            [&](position& Position, velocity& Velocity)
            {
                Position.m_Value.m_X = Rnd.RandF32(-100.0f, 100.0f);
                Position.m_Value.m_Y = Rnd.RandF32(-100.0f, 100.0f);

                Velocity.m_Value.m_X = Rnd.RandF32(-1.0f, 1.0f);
                Velocity.m_Value.m_Y = Rnd.RandF32(-1.0f, 1.0f);
                Velocity.m_Value.NormalizeSafe();
            }
            , mesh_render   { {}, 10, 101, 102 }
            , group1        { {},   0 }
            , group2        { {}, 100 } 
            );

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