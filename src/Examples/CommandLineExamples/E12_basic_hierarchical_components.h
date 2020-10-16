namespace mecs::examples::E12_basic_hierarchical_components
{
    // Describing a basic hierarchy in MECS.
    // This hierarchy won't be very fast, it is mainly to show that is possible and how to think about it
    // Other examples will cover must faster ways of doing it.

    //-----------------------------------------------------------------------------------------
    // Components
    //-----------------------------------------------------------------------------------------

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
                // in MECS there are two ways to query and compute. Systems is the fastest most efficient way
                // because it goes in order and does not jump around memory. However sometimes you need to jump around
                // memory for that you have this function. This function will get the components from the entity
                // but because is not an entity that we are visiting right now it will create a cache miss per component.
                // MECS does its best to cache as much as it can to minimize misses but it won't be perfect.
                // Also the components that you are asking for may or may not exists. So the function parameters
                // are creating a query that must be matched against the entity itself. If the query fails then
                // it the lambda won't get call. Please also be careful of passing an entity which is a zombie.
                // If it is a zombie it will assert so you should check before calling it.
                getEntityComponents(E, [&](local_position& LocalPos, position& WorldPos, children* pChildren)
                {
                    WorldPos.m_Value = LocalPos.m_Value + ParentWorldPos.m_Value;

                    printf("%*sChild Pos: %d\n", Level * 4, "", WorldPos.m_Value );

                    if (pChildren)
                    {
                        if(pChildren->m_List.size()) Recursive(*pChildren, WorldPos, Level+1 );
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
        printf("E12_hierarchy_components\n");
        printf("--------------------------------------------------------------------------------\n");

        auto    upUniverse      = std::make_unique<mecs::universe::instance>();
        auto&   DefaultWorld    = *upUniverse->m_WorldDB[0];

        //------------------------------------------------------------------------------------------
        // Registration
        //------------------------------------------------------------------------------------------

        //
        // Register Components
        //
        upUniverse->registerTypes<position, velocity, parent, children, local_position>();

        //
        // Create the game graph.
        //
        auto& Syncpoint = DefaultWorld.m_GraphDB.CreateSyncPoint();
        DefaultWorld.CreateGraphConnection<local_move_system>       (DefaultWorld.getStartSyncpoint(),  Syncpoint );
        DefaultWorld.CreateGraphConnection<root_move_system>        (DefaultWorld.getStartSyncpoint(),  Syncpoint );
        DefaultWorld.CreateGraphConnection<update_hierarchy_system> (Syncpoint,                         DefaultWorld.getEndSyncpoint());
                       
        //------------------------------------------------------------------------------------------
        // Initialization
        //------------------------------------------------------------------------------------------

        //
        // Create an entity group
        //
        auto& ChildrenArchetype   = DefaultWorld.getOrCreateArchitype<position, velocity, parent, children, local_position >();
        auto& ParentArchetype     = DefaultWorld.getOrCreateArchitype<position, velocity, children>();

        //
        // Create entities
        //
        DefaultWorld.CreateEntity(ParentArchetype, [&]( mecs::component::entity& ParentEntity, children& ChildrenComponent ) noexcept
        {
            DefaultWorld.CreateEntity(ChildrenArchetype, [&](mecs::component::entity& Entity ) noexcept
            {
                ChildrenComponent.m_List.push_back(Entity);
            }, parent{ {}, ParentEntity } );
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