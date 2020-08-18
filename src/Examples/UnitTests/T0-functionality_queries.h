
namespace mecs::unit_test::functionality::queries
{
    //----------------------------------------------------------------------------------------------
    // Define some components
    //----------------------------------------------------------------------------------------------
    struct pepe : mecs::component::data
    {
        constexpr static auto       name_v              = xconst_universal_str("pepe");
        int m_Value = 11;
    };

    struct pepe2 : mecs::component::data
    {
        constexpr static auto       name_v              = xconst_universal_str("pepe2");
        static constexpr auto       type_data_access_v  = type_data_access::DOUBLE_BUFFER;
        int m_Value = 54;
    };

    struct first : mecs::component::share
    {
        constexpr static auto       name_v              = xconst_universal_str("first share component");
        int m_Value = 1;
    };

    struct second : mecs::component::share
    {
        constexpr static auto       name_v              = xconst_universal_str("second share component");
        int m_Value = 2;
    };

    //----------------------------------------------------------------------------------------------
    // This first test will only care to test the function parameters
    //----------------------------------------------------------------------------------------------
    struct sytem_test_01 : mecs::system::instance
    {
        using instance::instance;

        // This query involves 3 components:
        // entity - Which is the entity component, and we are asking as (MUST HAVE by using &, but all entities have this component), also as a read only (by using const)
        // first  - Which is share component, and we are asking as (MAY HAVE by using *), also as a read only (by using const)
        // pepe2  - Which is a double buffer, and we are asking as (MUST HAVE by using &), also we are asking for T0 (by using const) 
        void operator()( const entity& Entity, const first* pFirst, const pepe2& Pepe2 )
        {
            if(pFirst) xassert( pFirst->m_Value == 1 );
            xassert(Pepe2.m_Value == 54 );
            printf("Entity: %s, Hello!\n", Entity.getGuid().getStringHex<char>().c_str() );
        }
    };

    //----------------------------------------------------------------------------------------------
    // Now we are incorporating a more complex query by using "query_t"
    //----------------------------------------------------------------------------------------------
    struct sytem_test_02 : mecs::system::instance
    {
        using instance::instance;

        using query_t = std::tuple< all<second> >;

        void operator()(const entity& Entity, const first* pFirst, const pepe2& Pepe2)
        {
            xassert(pFirst);
            xassert(pFirst->m_Value == 1);
            xassert(Pepe2.m_Value == 54);
            printf("Entity: %s, Requires 'second'!\n", Entity.getGuid().getStringHex<char>().c_str());
        }
    };

    //----------------------------------------------------------------------------------------------
    // Test
    //----------------------------------------------------------------------------------------------
    void Test(void)
    {
        mecs::universe::instance Universe;

        Universe.Init();

        // Register components
        Universe.registerTypes
        <
              pepe
            , pepe2
            , first
            , second
        >();

        //
        // Create graph
        //
        auto& DefaultWorld = *Universe.m_WorldDB[0];
        auto& System = DefaultWorld.m_GraphDB.CreateGraphConnection<sytem_test_01>(DefaultWorld.m_GraphDB.m_StartSyncPoint, DefaultWorld.m_GraphDB.m_EndSyncPoint);
        DefaultWorld.m_GraphDB.CreateGraphConnection<sytem_test_02>(DefaultWorld.m_GraphDB.m_StartSyncPoint, DefaultWorld.m_GraphDB.m_EndSyncPoint);

        //
        // Create archetypes
        //
        DefaultWorld.m_ArchetypeDB.getOrCreateArchitype<pepe>()
            .CreateEntity(System, [](pepe& Pepe)
                {
                    int a = 0;
                });

        DefaultWorld.m_ArchetypeDB.getOrCreateArchitype<pepe, pepe2>()
            .CreateEntity(System, [](pepe2& Pepe2, pepe& Pepe)
                {
                    int a = 0;
                });

        DefaultWorld.m_ArchetypeDB.getOrCreateArchitype<pepe, first, second, pepe2>()
            .CreateEntity(System, [](pepe& Pepe, pepe2& Pepe2)
                {
                    int a = 0;

                }, first{}, second{ {}, 33 });

        //
        // Create 10 entities
        //
        DefaultWorld.m_ArchetypeDB.getOrCreateArchitype<pepe, pepe2>()
            .CreateEntities(System, 10000, {})([](pepe2& Pepe2, pepe& Pepe)
                {
                    Pepe.m_Value = 22;
                    int a = 0;
                });

        //
        // Test systems
        //
        DefaultWorld.Start();

        for(int i=0;i<2;i++) 
            DefaultWorld.Resume();

        int a = 0;
    }
}
