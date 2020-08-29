namespace mecs::examples::E05_custom_delegate_updated_entity
{
    //-----------------------------------------------------------------------------------------
    // Components
    //-----------------------------------------------------------------------------------------
    struct position : mecs::component::data
    {
        xcore::vector2 m_Value = xcore::vector2{0, 0};
    };

    //-----------------------------------------------------------------------------------------
    // Systems
    //-----------------------------------------------------------------------------------------
    struct move_system : mecs::system::instance
    {
        using instance::instance;

        // System can define their own custom events. They work similar to the standard MECS events.
        // Note that events need to have a GUID and also a name. The name is used for debugging and
        // for visual editors. Also you need to specify which system this event belongs.
        // This event could be defined outside the scope of the structure if we wanted to.
        struct my_event : exclusive_event
        {
            using   real_event_t    = define_real_event< int >;
            using   system_t        = move_system;
        };

        // This is how MECS knows which events this system have.
        using events_t = std::tuple
        <
            my_event
        >;

        // This function will automatically trigger the (updated_entity) event, since we have a mutable variable as a parameter.
        // if the variable will be 'const position& Position' then it won't trigger that event since it is mark as read only.
        void operator() ( position& Position ) noexcept
        {
            Position.m_Value.m_X += 1.0f;
            Position.m_Value.m_Y += 1.0f;

            // Let anyone listening to my_event receive 5... because why not?
            EventNotify<my_event>( 5 );
        }
    };

    //-----------------------------------------------------------------------------------------
    // Custom Delegates
    //-----------------------------------------------------------------------------------------
    // Custom delegates are different from regular delegates. One of the main differences are
    // that they don't support 'query_t'. Stead they will received the event given by the system
    // no question asked. Also the parameters of the function is also different. The parameters
    // are given by custom event. In this example an integer. Since the custom events are issue
    // by only specific system their execution is less chaotic and that may allow certain flexibilities. 
    //-----------------------------------------------------------------------------------------
    struct my_delegate : mecs::system::delegate::instance< move_system::my_event >
    {
        void operator() ( system& System, int X ) const noexcept
        {
            printf("First User Base Event: %d\n", X );
        }
    };

    //-----------------------------------------------------------------------------------------
    // Delegate 
    //-----------------------------------------------------------------------------------------
    // This delegate will be call one an entity which has position changes.
    /*
    struct mutated_position_delegate : mecs::delegate::instance< mecs::event::updated_entity >
    {
        constexpr static mecs::delegate::guid guid_v{ "mutated_position_delegate" }; 

        void operator() ( mecs::system::instance& System, const mecs::entity::instance& Entity, const position& Position ) const noexcept
        {
            printf("Entity: %I64X, New Position (%f, %f)\n", Entity.getGuid().m_Value, Position.m_X, Position.m_Y );
        }
    };
    */

    //-----------------------------------------------------------------------------------------
    // Test
    //-----------------------------------------------------------------------------------------
    void Test()
    {
        printf("--------------------------------------------------------------------------------\n");
        printf("E05_custom_delegate_updated_entity\n");
        printf("--------------------------------------------------------------------------------\n");

        auto upUniverse = std::make_unique<mecs::universe::instance>();
        upUniverse->Init();

        //------------------------------------------------------------------------------------------
        // Registration
        //------------------------------------------------------------------------------------------

        //
        // Register Components
        //
        upUniverse->registerTypes<position>();

        //
        // Create the game graph.
        // 
        auto& DefaultWorld = *upUniverse->m_WorldDB[0];
        auto& System = DefaultWorld.m_GraphDB.CreateGraphConnection<move_system>(DefaultWorld.m_GraphDB.m_StartSyncPoint, DefaultWorld.m_GraphDB.m_EndSyncPoint);

        //
        // Create the delegates.
        //
        DefaultWorld.m_GraphDB.CreateSystemDelegate< my_delegate >();
        //DefaultWorld.m_GraphDB.CreateArchetypeDelegate< remove_position_delegate >();

        //------------------------------------------------------------------------------------------
        // Initialization
        //------------------------------------------------------------------------------------------

        //
        // Create an entity group
        //
        auto& Archetype = DefaultWorld.m_ArchetypeDB.getOrCreateArchitype<position>();

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