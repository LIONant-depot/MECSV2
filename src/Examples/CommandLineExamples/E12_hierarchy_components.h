namespace mecs::examples::E12_hierarchy_components
{
    //
    // Describing a hierarchy in MECS
    //
    struct parent : mecs::component::share
    {
        mecs::component::entity m_Entity;
    };

    struct children : mecs::component::data
    {
        std::vector<mecs::component::entity> m_List{};
    };

    struct position : mecs::component::data
    {
        int m_Value{0};
    };

    struct local_position : mecs::component::data
    {
        int m_Value{ 0 };
    };

    struct velocity : mecs::component::data
    {
        int m_Value{ 1 };
    };

    //-----------------------------------------------------------------------------------------
    // Systems
    //-----------------------------------------------------------------------------------------
    struct update_hierarchy_system : mecs::system::instance
    {
        using instance::instance;

        // This should give us the roots
        using query_t = std::tuple
        <
            none<parent>
        >;

        void Recursive( const children& Children, const position& ParentWorldPos, int Level ) noexcept
        {
            // Loop thought out all our children
            for (auto& E : Children.m_List)
            {
                getComponents(E, [&](local_position& LocalPos, position& WorldPos, children* pChildren)
                {
                    WorldPos.m_Value = LocalPos.m_Value + ParentWorldPos.m_Value;

                    printf("%*sChild Pos: %d\n", Level * 4, "", WorldPos.m_Value );

                    if (pChildren)
                    {
                        Recursive(*pChildren, WorldPos, Level+1 );
                    }
                });
            }
        }

        void operator()( const children& Children, const position& ParentWorldPos ) noexcept
        {
            printf("Root Position: %d\n", ParentWorldPos.m_Value );
            Recursive(Children, ParentWorldPos, 1);
        }
    };

    // Move all the parents
    struct root_move_system : mecs::system::instance
    {
        using instance::instance;

        using query_t = std::tuple
        <
            none<local_position>
        >;

        static void Update( int& Pos, int& Vel, int DT ) noexcept
        {
            Pos += Vel * DT;
        }

        void operator() (position& Position, velocity& Velocity) const noexcept
        {
            Update(Position.m_Value, Velocity.m_Value, 1 );
        }
    };

    // Move all the children
    struct local_move_system : mecs::system::instance
    {
        using instance::instance;

        void operator() (local_position& LocalPosition, velocity& Velocity) const noexcept
        {
            root_move_system::Update(LocalPosition.m_Value, Velocity.m_Value, 1);
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
        upUniverse->registerTypes<position, velocity, parent, children, local_position>();

        //
        // Which world we are building?
        //
        auto& DefaultWorld = *upUniverse->m_WorldDB[0];

        //
        // Create the game graph.
        //
        auto& Syncpoint = DefaultWorld.m_GraphDB.CreateSyncPoint();
        auto& System    = DefaultWorld.m_GraphDB.CreateGraphConnection<local_move_system>       (DefaultWorld.m_GraphDB.m_StartSyncPoint, Syncpoint );
                          DefaultWorld.m_GraphDB.CreateGraphConnection<root_move_system>        (DefaultWorld.m_GraphDB.m_StartSyncPoint, Syncpoint );
                          DefaultWorld.m_GraphDB.CreateGraphConnection<update_hierarchy_system> (Syncpoint, DefaultWorld.m_GraphDB.m_EndSyncPoint);
                       
        //------------------------------------------------------------------------------------------
        // Initialization
        //------------------------------------------------------------------------------------------

        //
        // Create an entity group
        //
        auto& ChildrenArchetype   = DefaultWorld.m_ArchetypeDB.getOrCreateArchitype<position, velocity, parent, children, local_position >();
        auto& ParentArchetype     = DefaultWorld.m_ArchetypeDB.getOrCreateArchitype<position, velocity, children>();

        //
        // Create entities
        //
        ParentArchetype.CreateEntity( System, [&]( mecs::component::entity& ParentEntity, children& ChildrenComponent ) noexcept
        {
            ChildrenArchetype.CreateEntity( System, [&](mecs::component::entity& Entity ) noexcept
            {
                ChildrenComponent.m_List.push_back(Entity);
            }, parent{ {}, ParentEntity } );
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