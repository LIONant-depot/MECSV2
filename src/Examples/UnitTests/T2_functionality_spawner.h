namespace mecs::unit_test::functionality::spawner
{
    namespace life
    {
        struct component : mecs::component::data
        {
            int m_Value{ 10 };
        };

        namespace system
        {
            struct decrement : mecs::system::instance
            {
                using mecs::system::instance::instance;
                void operator() (mecs::component::entity& Entity, life::component& Life) noexcept
                {
                    Life.m_Value--;
                    if (Life.m_Value == 0)
                    {
                        deleteEntity(Entity);
                    }
                }
            };
        }
    }

    namespace spawner
    {
        struct component : mecs::component::data
        {
            std::uint32_t                                           m_Count;
            mecs::archetype::instance::guid                         m_gArchetype;
            xcore::func<void(mecs::archetype::entity_creation&&)>   m_fnCreated;
        };

        struct system : mecs::system::instance
        {
            using instance::instance;
            void operator() ( mecs::component::entity& Entity, spawner::component& S ) noexcept
            {
                if (S.m_fnCreated.isValid()) S.m_fnCreated( createEntities( S.m_gArchetype, S.m_Count, {} ) );
                else                                        createEntities( S.m_gArchetype, S.m_Count, {} )();

                deleteEntity(Entity);
            }
        };
    }

    void Test()
    {
        mecs::universe::instance Universe;

        Universe.Init();

        // Register components
        Universe.registerTypes
        <
            life::component
            , spawner::component
        >();

        //
        // Create graph
        //
        auto& DefaultWorld = *Universe.m_WorldDB[0];
        auto& System1 = DefaultWorld.m_GraphDB.CreateGraphConnection<spawner::system>(DefaultWorld.m_GraphDB.m_StartSyncPoint, DefaultWorld.m_GraphDB.m_EndSyncPoint);
        auto& System2 = DefaultWorld.m_GraphDB.CreateGraphConnection<life::system::decrement>(DefaultWorld.m_GraphDB.m_StartSyncPoint, DefaultWorld.m_GraphDB.m_EndSyncPoint);

        //
        // Create a few entities
        //
        auto& SpawnerArchetype = DefaultWorld.m_ArchetypeDB.getOrCreateArchitype<spawner::component>();
        auto& LifeArchetype    = DefaultWorld.m_ArchetypeDB.getOrCreateArchitype<life::component>();

        SpawnerArchetype.CreateEntity( System1, [&]( spawner::component& Spawner )
        {
            Spawner.m_Count         = 10;
            Spawner.m_gArchetype    = LifeArchetype.getGuid();
            Spawner.m_fnCreated     = []( mecs::archetype::entity_creation&& EntityCreation )
            {
                EntityCreation( []( life::component& LifeComponent )
                {
                    LifeComponent.m_Value = 23;
                });
            };
        });

        //
        // Test systems
        //
        DefaultWorld.Start();
        for (int i = 0; i < 100; i++)
        {
            DefaultWorld.Resume();
        }

        int a = 0;
    }
}