namespace mecs::examples::E04_delegate_moveinout
{
    //-----------------------------------------------------------------------------------------
    // Components
    //-----------------------------------------------------------------------------------------
    struct position : mecs::component::data
    {
        xcore::vector2 m_Value {0, 0};
    };

    //-----------------------------------------------------------------------------------------
    // Systems
    //-----------------------------------------------------------------------------------------
    struct will_add_position_system : mecs::system::instance
    {
        using mecs::system::instance::instance;

        // In MECS you can create filters/queries which identify which kind of entities will be
        // executed by the system. MECS specifies its queries via a type 'query_t' as shown below.
        // In its template arguments you can specified 3 types of filters:
        //      1. none< components... > == Entities with these components won't execute the our function.
        //      2. all < components... > == Entities must have all these components or else won't execute.
        //      3. any < components... > == Entities that have at least one of these component will execute.
        //
        // The system's function parameters will complement the query. All parameters with references '&' will be added to
        // the 'all' automatically. All parameters mark as a pointer '*' will be added to 'any'
        // In this example we want entities that don't have a position component. 
        using query_t = std::tuple
        <
            none<position>
        >;

        // This function will add the position component to entities
        void operator() ( entity& Entity ) noexcept
        {
            getArchetypeBy< std::tuple<position>, std::tuple<>>( Entity, [&]( archetype& Archetype ) constexpr noexcept
            {
                moveEntityToArchetype( Entity, Archetype );
            });
        }
    };

    //-----------------------------------------------------------------------------------------

    struct will_remove_position_system : mecs::system::instance
    {
        using mecs::system::instance::instance;

        // This system wants all entities which have a position component
        using query_t = std::tuple
        <
            all<position>
        >;

        // Here we are going to remove the position component
        void operator() ( entity& Entity ) noexcept
        {
            getArchetypeBy<std::tuple<>,std::tuple<position>>( Entity, [&]( archetype& Archetype ) constexpr noexcept
            {
                moveEntityToArchetype(Entity, Archetype);
            });
        }
    };

    //-----------------------------------------------------------------------------------------
    // Group Delegates
    //-----------------------------------------------------------------------------------------
    // Delegates are used to extend systems. MECS's Systems generate events base on things happening to them.
    // One event could be when a system moves an 'entity into another group' (event::moved_in). When this event happen
    // we can add additional functionally. Unlike systems which are connected to a graph delegates are not.
    // They can be trigger by any system at any time. So similarly to systems they don't have state.
    // If you are any non atomic variables here the probability that something will go wrong is close to 100%.
    // Similar to systems delegates have a standard function that will be executed when the event happens.
    // The delegates standard functions is a little different from system function. The key difference is the first
    // parameter in delegates. The first parameter is the system which is calling the delegate.
    // All delegate functions must have at least this parameter.
    // Similar to systems delegates can filter out which types of entities will call the delegate using the query_t and
    // the parameters of its function.
    //-----------------------------------------------------------------------------------------
    struct add_position_delegate : mecs::archetype::delegate::instance< mecs::archetype::event::moved_in >
    {
        // This function will be call ones per entity. Since we only will have one entity it will only be call ones per frame.
        // Note that by putting the position component in the parameter list we are asking for groups which have this component in it.
        // Note that the first parameter is the system that created this event. Note that this parameter is require.
        void operator() ( mecs::system::instance& System, position& Position ) const noexcept
        {
            Position.m_Value.m_X = 10;
            Position.m_Value.m_Y = 20;
            printf("Position was Added\n");
        }
    };

    //-----------------------------------------------------------------------------------------
    
    // This delegate will be call for each entity which gets moved out of a group.
    struct remove_position_delegate : mecs::archetype::delegate::instance< mecs::archetype::event::moved_out >
    {
        // We want entities which have or had positions
        using query_t = std::tuple
        <
            all<position>
        >;

        // This function has the minimum number of parameters.
        void operator() ( mecs::system::instance& System ) const noexcept
        {
            printf("Position was Removed\n");
        }
    };
    
    //-----------------------------------------------------------------------------------------
    // Test
    //-----------------------------------------------------------------------------------------
    void Test()
    {
        printf( "--------------------------------------------------------------------------------\n");
        printf( "E04_delegate_moveinout\n");
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
        // Register the game graph.
        // Note that we are adding two system which could run in parallel.
        // Note how in this example an entity only executes in one system and not in both.
        // This is because of the change of state is "visible" at the end of the executing step
        // (ones all the systems connected to the same syncpoint are done).
        // In our example that syncpoint happens to be the last one.
        DefaultWorld.CreateGraphConnection<will_add_position_system   >(DefaultWorld.getStartSyncpoint(), DefaultWorld.getEndSyncpoint() );
        DefaultWorld.CreateGraphConnection<will_remove_position_system>(DefaultWorld.getStartSyncpoint(), DefaultWorld.getEndSyncpoint() );

        //
        // Create the delegates.
        //
        DefaultWorld.CreateArchetypeDelegate< add_position_delegate    >();
        DefaultWorld.CreateArchetypeDelegate< remove_position_delegate >();

        //------------------------------------------------------------------------------------------
        // Initialization
        //------------------------------------------------------------------------------------------

        //
        // Create an entity group
        //
        auto& Archetype = DefaultWorld.getOrCreateArchitype<position>();

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