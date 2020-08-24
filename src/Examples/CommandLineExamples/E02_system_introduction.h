namespace mecs::examples::E02_system_introduction
{
    //-----------------------------------------------------------------------------------------
    // Components
    //-----------------------------------------------------------------------------------------
    // Similar to a Component System. Entity Component System also have components which ultimately
    // defines an Entity or GameObject. But unlike a traditional component system ECS components
    // should have very little or not functions in them. Components are thought more like a container
    // of data and not a C++ traditional object oriented programming. ECS encourages functional
    // programming for many reasons.
    //-----------------------------------------------------------------------------------------
    struct message : mecs::component::data
    {
        // Note that all components must have a constexpr variable call type_guid_v.
        // This variable is the global unique identifier for this type of component.
        // The string been used to construct the type_guid can be any string.
        // In this case I generated a unique string from https://www.uuidgenerator.net/version4 
        // But literally can be anything as long as it is unique.
        constexpr static auto type_guid_v = type_guid{"682faa2b-25b9-4f44-a225-81f9c9ca58be"};

        // This component only has one variable which is a pointer to a string.
        // we default constructed to null. However you don't need to constructed if it doesn't make sense.
        const char* m_pValue{nullptr};
    };

    //-----------------------------------------------------------------------------------------
    // Systems
    //-----------------------------------------------------------------------------------------
    // Systems are the functions/functors which execute every frame to update components.
    // Systems in MECS should have not state. There are many reasons for it. However there is a
    // very clean and practical one. Systems run in parallel, so if you have any variable it
    // be access concurrently and most likely read or write trash. Other reasons why not to have
    // state in a ECS system can be found on other references.
    //-----------------------------------------------------------------------------------------
    struct print_system : mecs::system::instance
    {
        constexpr static auto   type_guid_v          = type_guid{ "print_system" };

        // This line is a requirement in c++ it says to use the parent's constructor.
        // We will talk more about constructor later.
        using instance::instance;

        // This is the standard system function which will be call ones per entity.
        // Since we only will have one entity in this example, it will only be call ones per frame.
        // The parameters of this function are the components of the entity.
        // Any entity which has those components will call this function.
        // Details to pay attention to. First is that are parameters are 'references' and not pointers.
        // Second that if we choose not to change the component we should mark it as 'const'.
        // Been const correct is very important because of threading issues,
        // MECS will change its behavior base on the access type to those components.
        void operator() ( const message& Message ) const noexcept
        {
            if( Message.m_pValue ) 
                printf("%s\n", Message.m_pValue );
        }
    };

    //-----------------------------------------------------------------------------------------
    // Test
    //-----------------------------------------------------------------------------------------
    void Test()
    {
        printf( "--------------------------------------------------------------------------------\n");
        printf( "E02_system_introduction\n");
        printf( "--------------------------------------------------------------------------------\n");

        auto upUniverse = std::make_unique<mecs::universe::instance>();
        upUniverse->Init();

        //------------------------------------------------------------------------------------------
        // Registration
        //------------------------------------------------------------------------------------------

        //
        // We must register all components that we are going to use.
        // This is important because this is how the system keeps track of the components.
        //
        // Register components, this function can also be used to registers
        // other core types like systems, delegates, etc.
        upUniverse->registerTypes<message>();

        //
        // Create the game graph.
        // A game graph is what a game does every frame. Things like running physics, render, input, etc...
        // Many engines have the game graph hardcoded, which is really unnecessary. This MECS lets you create
        // whichever game-graph you like. In this example we are creating the following graph.
        //
        //    +----------+            +-----------+            +----------+   
        //  /              \          |           |          /              \ 
        // | StartSyncPoint | ------> | my_system | ------> |  EndSyncPoint  |
        //  \              /          |           |          \              / 
        //    +----------+            +-----------+            +----------+
        //
        // We will discuss later what a syncpoint is. Right now you can think of these two syncpoints as the
        // begging of the frame and the end of the frame. Since MECS is a multicore all system connected to the same
        // two syncpoints will execute in parallel. Additionally each system also execute all its entities in parallel.
        auto& DefaultWorld = *upUniverse->m_WorldDB[0];
        auto& PrintSystem  = DefaultWorld.m_GraphDB.CreateGraphConnection<print_system>(DefaultWorld.m_GraphDB.m_StartSyncPoint, DefaultWorld.m_GraphDB.m_EndSyncPoint);

        //------------------------------------------------------------------------------------------
        // Initialization
        //------------------------------------------------------------------------------------------

        //
        // Create an entity group
        // An entity group is somewhat similar to an architype/prefab except those have specific values
        // Here a group just talk about which components a particular entity has. 
        // A "group of components" if you like to think it that way. 
        //
        auto& Archetype = DefaultWorld.m_ArchetypeDB.getOrCreateArchitype<message>();

        //
        // Create one entity
        // To create an entity you need to let the system know which group you are talking about.
        // Note that the creation function has a callback which allows you to initialize the value of the entity.
        Archetype.CreateEntity( PrintSystem, []( message& Message ) constexpr noexcept
        {
            Message.m_pValue = "Hello World!!";
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
        for( int i=0; i<10; i++) 
            DefaultWorld.Resume();

        xassert(true);
    }
}