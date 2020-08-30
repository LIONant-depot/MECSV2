namespace mecs::examples::E08_quantum_mutable_components
{
    //-----------------------------------------------------------------------------------------
    // Components
    //-----------------------------------------------------------------------------------------
    // Quantum mutable components are components that are meant to be read and write at the same time.
    // As you can imagine only that is possible if you have atomic variables or a way to linearize such as locks.
    // For such components there is no protection at all. So be very careful when you use them.
    struct value : mecs::component::data
    {
        static constexpr auto type_data_access_v = type_data_access::QUANTUM;
        std::atomic<int> m_Value{ 0 };
    };

    //-----------------------------------------------------------------------------------------
    // Systems
    //-----------------------------------------------------------------------------------------
    struct counter_up: mecs::system::instance
    {
        using instance::instance;

        constexpr static auto type_guid_v = type_guid{ "counter_up" };

        // We will be incrementing the value of value. Note that while the syntax looks like a simple
        // increment in reality is an atomic increment. So there is magic behind that add.
        void operator() ( value& Value ) noexcept
        {
            Value.m_Value++;
        }
    };

    //-----------------------------------------------------------------------------------------

    struct counter_down : mecs::system::instance
    {
        using instance::instance;

        // Here mecs doesn't care that there is another system trying to write to this variable
        // at the same time because it is a quantum component. As a user you better
        // know what you are doing
        void operator() (value& Value) noexcept
        {
            Value.m_Value--;
        }
    };

    //-----------------------------------------------------------------------------------------
    // Delegate 
    //-----------------------------------------------------------------------------------------
    
    struct mutated_value_delegate : mecs::archetype::delegate::instance< mecs::archetype::event::updated_entity >
    {
        void operator() ( system& System, const entity& Entity, const value& Value ) const noexcept
        {
            // The value of value will be undetermined since it lives in the quantum world.
            // However we know that the range will be from -1 to 1 because for a given frame it is been incremented and decremented 
            printf("System: %s, Entity: %s, New Value %d\n"
                , System.getDescriptor().m_Guid == counter_down::type_guid_v ? "CountDown"
                                                                             : "CountUp  "
                , Entity.getGUID().getStringHex<char>().c_str()
                , Value.m_Value.load( std::memory_order_relaxed ) );
        }
    };

    //-----------------------------------------------------------------------------------------
    // Test
    //-----------------------------------------------------------------------------------------
    void Test()
    {
        printf("--------------------------------------------------------------------------------\n");
        printf("E08_quantum_mutable_components\n");
        printf("--------------------------------------------------------------------------------\n");

        auto upUniverse = std::make_unique<mecs::universe::instance>();
        upUniverse->Init();

        //------------------------------------------------------------------------------------------
        // Registration
        //------------------------------------------------------------------------------------------

        //
        // Register Components
        //
        upUniverse->registerTypes<value>();

        //
        // Create the game graph.
        // 
        auto& DefaultWorld = *upUniverse->m_WorldDB[0];
        auto& System = DefaultWorld.m_GraphDB.CreateGraphConnection<counter_up>(DefaultWorld.m_GraphDB.m_StartSyncPoint, DefaultWorld.m_GraphDB.m_EndSyncPoint);
                       DefaultWorld.m_GraphDB.CreateGraphConnection<counter_down>(DefaultWorld.m_GraphDB.m_StartSyncPoint, DefaultWorld.m_GraphDB.m_EndSyncPoint);

        //
        // Create Delegates
        //
        DefaultWorld.m_GraphDB.CreateDelegate<mutated_value_delegate>();

       //------------------------------------------------------------------------------------------
       // Initialization
       //------------------------------------------------------------------------------------------

       //
       // Create an entity group
       //
       auto& Archetype = DefaultWorld.m_ArchetypeDB.getOrCreateArchitype<value>();

       //
       // Create one entity
       //
       Archetype.CreateEntity(System);

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
           DefaultWorld.Resume();

       xassert(true);
    }
}