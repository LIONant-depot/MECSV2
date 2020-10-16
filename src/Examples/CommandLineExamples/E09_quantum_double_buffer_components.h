namespace mecs::examples::E09_quantum_double_buffer_components
{
    //-----------------------------------------------------------------------------------------
    // Components
    //-----------------------------------------------------------------------------------------
    // Quantum double buffer components are the most complex type of access pattern in mecs.
    // They are double buffer which means the reads are linear. Thread safe similar to the
    // regular double buffer. The difference is in the writes. The writes can be done by
    // multiple systems at a time. The writes are assumed to be in the quantum world.
    // Similarly to the quantum components. So you will expect to see std::atomic variables here.
    // However in this example there are none. There are two linear arrays. So inserting
    // entries in these arrays would be very ragerous then? The answer ofcourse is not because
    // while the memory is consecutive the access to each entry is determine via quantum variable.
    struct position : mecs::component::data
    {
        static constexpr auto type_data_access_v = type_data_access::QUANTUM_DOUBLE_BUFFER;

        std::array<int, 6>          m_lPositions;
        std::array<const char*, 6>  m_lNames;
    };

    //-----------------------------------------------------------------------------------------
    // These component is used to determine which entries are allocated where in the position arrays
    // Note how the variable is atomic, which makes it thread safe to increment its value.
    // This interns makes the individual entries from Position and Names also thread safe.
    struct mutable_count : mecs::component::data
    {
        static constexpr auto type_data_access_v = type_data_access::QUANTUM;

        std::atomic<int>            m_Value;
    };

    //-----------------------------------------------------------------------------------------
    // This component is used to read the count for the arrays in T0 (read only state) of position.
    // You can see that is a regular component which means it has all the expected thread safety.
    struct readonly_count : mecs::component::data
    {
        int m_Value;
    };

    //-----------------------------------------------------------------------------------------
    // Systems
    //-----------------------------------------------------------------------------------------
    struct odd_system : mecs::system::instance
    {
        using instance::instance;

        // This system will increment all the position which values are odd and reinsert them back
        // into the array of positions. Note that we can access the different states of the double buffer
        // as we knew from before. We are also accessing the quantum component MutableCount to determine
        // where to insert the entries. Finally we are reading the previous frame state using the ReadOnlyCount.
        void operator() ( position& T1Position, const position& T0Position, mutable_count& MutableCount, const readonly_count& ReadOnlyCount ) noexcept
        {
            for( int i = 0, end = ReadOnlyCount.m_Value; i<end; ++i )
            {
                if( !!(T0Position.m_lPositions[i]&1) )
                {
                    int iNew = MutableCount.m_Value++;
                    T1Position.m_lPositions[iNew] = T0Position.m_lPositions[i] + 1;
                    T1Position.m_lNames[iNew]     = T0Position.m_lNames[i];
                }
            }
        }
    };

    //-----------------------------------------------------------------------------------------

    struct even_system : mecs::system::instance
    {
        using instance::instance;

        // Note here how we are accessing T1 Position as the same time as the other system.
        // Thanks to the fact that they writes are quantum. For the same reason that we can
        // access MutableCount as a write.
        void operator() ( position& T1Position, const position& T0Position, mutable_count& MutableCount, const readonly_count& ReadOnlyCount ) noexcept
        {
            for( int i = 0, end = ReadOnlyCount.m_Value; i<end; ++i )
            {
                if( !(T0Position.m_lPositions[i]&1)  )
                {
                    int iNew = MutableCount.m_Value++;
                    T1Position.m_lPositions[iNew] = T0Position.m_lPositions[i] + 1;
                    T1Position.m_lNames[iNew]     = T0Position.m_lNames[i];
                }
            }
        }
    };

    //-----------------------------------------------------------------------------------------

    struct print_position : mecs::system::instance
    {
        using instance::instance;

        // This system will print all the data as well as to prepares the respective counts for
        // the next frame.
        void operator() ( const entity& Entity, const position& T0Position, mutable_count& MutableCount, readonly_count& ReadOnlyCount ) noexcept
        {
            // Copy the count to our read-only variable (This means is ready for the next frame)
            ReadOnlyCount.m_Value = MutableCount.m_Value.load(std::memory_order_relaxed);

            // Now go throught all the entries 
            for( int i = 0, end = ReadOnlyCount.m_Value; i<end; ++i )
            {
                printf( "%s [%s] moved to {.x = %d}\n"
                    , Entity.getGUID().getStringHex<char>().c_str()
                    , T0Position.m_lNames[i]
                    , T0Position.m_lPositions[i] );
            }

             printf( "-----------------------------------------\n");

            // Reset the count so that next frame we can simply add to it
            MutableCount.m_Value.store(0, std::memory_order_relaxed );
        }
    };

    //-----------------------------------------------------------------------------------------
    // Test
    //-----------------------------------------------------------------------------------------
    void Test()
    {
        printf("--------------------------------------------------------------------------------\n");
        printf("E09_quantum_double_buffer_components\n");
        printf("--------------------------------------------------------------------------------\n");

        auto    upUniverse      = std::make_unique<mecs::universe::instance>();
        auto&   DefaultWorld    = *upUniverse->m_WorldDB[0];

        //------------------------------------------------------------------------------------------
        // Registration
        //------------------------------------------------------------------------------------------

        //
        // Register Components
        //
        upUniverse->registerTypes<position, mutable_count, readonly_count>();

        //
        // Create a Syncpoint. Sync-Points are there to help schedule our systems. In this example
        // we want to add (odd, and even) positions and ones both of these are done then we want
        // to run print position. This sync_point allow us to do just that.
        //
        auto& Syncpoint = DefaultWorld.CreateSyncPoint();

        //
        // Create the game graph.
        // The odd and even systems can run in parallel with safety ones both of them finish
        // the print system will run ones it finish we will have reach the end of our game graph.
        // Now our game graph looks a bit more complex:
        //
        //                            +-------------+                                    
        //                            |             |                                    
        //                     +----> | odd_system  | ------+                            
        //    +----------+    /       |             |        \    +----------+        +----------------+        +----------+   
        //  /              \ /        +-------------+         \ /              \      |                |      /              \ 
        // | StartSyncPoint |\        +-------------+         /|    SyncPrint   |---->| print_position |---->|  EndSyncPoint  |       
        //  \              /  \       |             |        /  \              /      |                |      \              / 
        //    +----------+     +----> | even_system | ------+     +----------+        +----------------+        +----------+
        //                            |             |                                    
        //                            +-------------+                                    
        //
        // Ideally we want our graph to be as short as possible which means that our graph should
        // be as fat as possible. Which it will mean that as many systems as possible will be running
        // in parallel.
        auto& System = DefaultWorld.m_GraphDB.CreateGraphConnection<odd_system>    (DefaultWorld.m_GraphDB.m_StartSyncPoint, Syncpoint);
                       DefaultWorld.m_GraphDB.CreateGraphConnection<even_system>   (DefaultWorld.m_GraphDB.m_StartSyncPoint, Syncpoint);
                       DefaultWorld.m_GraphDB.CreateGraphConnection<print_position>(Syncpoint, DefaultWorld.m_GraphDB.m_EndSyncPoint);
                       
        //------------------------------------------------------------------------------------------
        // Initialization
        //------------------------------------------------------------------------------------------

        //
        // Create an entity group
        //
        auto& Archetype = DefaultWorld.m_ArchetypeDB.getOrCreateArchitype<position, mutable_count, readonly_count>();

        //
        // Create one entity
        //
        DefaultWorld.CreateEntity(Archetype, [&](position& Position, mutable_count& MutableCount, readonly_count& ReadOnlyCount)
        {
            xcore::random::small_generator Rnd;
            for (auto& E : Position.m_lPositions)
            {
                E = Rnd.RandS32(0, 100);
            }

            Position.m_lNames[0] = "pepe";
            Position.m_lNames[1] = "jack";
            Position.m_lNames[2] = "ant";
            Position.m_lNames[3] = "bird";
            Position.m_lNames[4] = "mama";
            Position.m_lNames[5] = "papa";

            ReadOnlyCount.m_Value = static_cast<int>(Position.m_lNames.size());
            MutableCount.m_Value.store(0, std::memory_order_relaxed);
        });

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