namespace mecs::unit_test::functionality::simple
{
    struct simple : mecs::component::data
    {
        constexpr static auto       name_v              = xconst_universal_str("simple");
        int m_Value{ 22 };
    };

    struct double_buff : mecs::component::data
    {
        constexpr static auto       name_v              = xconst_universal_str("double_buff");
        static constexpr auto       type_data_access_v  = type_data_access::DOUBLE_BUFFER;
        int m_Value{ 123 };
    };

    struct my_tag : mecs::component::tag 
    {
        constexpr static auto       name_v              = xconst_universal_str("my_tag");
    };

    struct my_system : mecs::system::instance
    {
        using instance::instance;
        using query_t = std::tuple
        <
            all<simple>           
        ,   none<my_tag>          
        >;

        constexpr xforceinline
        void operator() ( double_buff& Buff, const simple& Simple ) const noexcept
        {
            xassert(Simple.m_Value);
            Buff.m_Value += Simple.m_Value;
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
            simple
        ,   double_buff
        ,   my_tag
        >();

        //
        // Create graph
        //
        auto& DefaultWorld  = *Universe.m_WorldDB[0];
        auto& System        = DefaultWorld.m_GraphDB.CreateGraphConnection<my_system>(DefaultWorld.m_GraphDB.m_StartSyncPoint, DefaultWorld.m_GraphDB.m_EndSyncPoint);

        //
        // Create a few entities
        //
        DefaultWorld.m_ArchetypeDB.getOrCreateArchitype<simple, double_buff>()
            .CreateEntities(System, 10000000ull XCORE_CMD_ASSERT(/ 10), {})([](simple& Simple, double_buff& Double)
            {
                assert(Simple.m_Value == 22);
                assert(Double.m_Value == 123);
                Simple.m_Value = 1;
                Double.m_Value = 0;
            });

        DefaultWorld.m_ArchetypeDB.getOrCreateArchitype<simple, double_buff, my_tag>()
            .CreateEntities(System, 100 XCORE_CMD_ASSERT(/ 10), {})([](simple& Simple, double_buff& Double)
                {
                    assert(Simple.m_Value == 22);
                    assert(Double.m_Value == 123);
                    Simple.m_Value = Double.m_Value = 0;
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