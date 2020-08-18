
namespace mecs::unit_test::functionality::queries
{
    struct pepe : mecs::component::data
    {
        constexpr static auto       name_v = xconst_universal_str("pepe");
        int m_Value = 11;
    };

    struct pepe2 : mecs::component::data
    {
        int m_Value = 54;
        constexpr static auto       name_v = xconst_universal_str("pepe2");
        static constexpr auto       type_data_access_v = type_data_access::DOUBLE_BUFFER;
    };

    struct first : mecs::component::share
    {
        constexpr static auto       name_v = xconst_universal_str("first share component");
        int m_Value = 11;
    };

    struct second : mecs::component::share
    {
        constexpr static auto       name_v = xconst_universal_str("second share component");
        int m_Value = 21;
    };

    struct test : mecs::system::instance
    {
        constexpr static auto   type_guid_v = type_guid{ "test" };
        using instance::instance;

        void operator() (const first* pp, const pepe2& ppp)
        {
            printf("Hello");
        }
    };

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
        auto& System = DefaultWorld.m_GraphDB.CreateGraphConnection<test>(DefaultWorld.m_GraphDB.m_StartSyncPoint, DefaultWorld.m_GraphDB.m_EndSyncPoint);

        //
        // Create archetypes
        //
        DefaultWorld.m_ArchetypeDB.getOrCreateArchitype<pepe>()
            .CreateEntity(System, [](pepe& Pepe)
                {
                    Pepe.m_Value = 22;
                    int a = 0;
                });

        DefaultWorld.m_ArchetypeDB.getOrCreateArchitype<pepe, pepe2>()
            .CreateEntity(System, [](pepe2& Pepe2, pepe& Pepe)
                {
                    Pepe.m_Value = 22;
                    int a = 0;
                });

        DefaultWorld.m_ArchetypeDB.getOrCreateArchitype<pepe, first, second, pepe2>()
            .CreateEntity(System, [](pepe& Pepe, pepe2& Pepe2)
                {
                    Pepe.m_Value = 22;
                    int a = 0;

                }, first{}, second{ {}, 33 });

        //
        // Create 10 entities
        //
        {
            DefaultWorld.m_ArchetypeDB.getOrCreateArchitype<pepe, pepe2>()
                .CreateEntities(System, 10000, {})([](pepe2& Pepe2, pepe& Pepe)
                    {
                        Pepe.m_Value = 22;
                        int a = 0;
                    });
        }


        //
        // 
        //

        int a = 0;
        DefaultWorld.Start();

        int b = 0;
    }
}
