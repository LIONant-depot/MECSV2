
namespace mecs::unit_test::functionality::simple_v2
{
    struct simple : mecs::component::data
    {
        constexpr static auto       type_name_v         = xconst_universal_str("simple");
        int m_Value{ 22 };
    };

    struct double_buff : mecs::component::data
    {
        static constexpr auto       type_name_v         = xconst_universal_str("double_buff");
        static constexpr auto       type_data_access_v  = type_data_access::DOUBLE_BUFFER;
        int m_Value{ 123 };
    };

    struct my_tag : mecs::component::tag 
    {
        constexpr static auto       type_name_v         = xconst_universal_str("my_tag");
    };

    struct some_share : mecs::component::share
    {
        constexpr static auto       type_name_v         = xconst_universal_str("share_component");
        int m_Value{ 22 };
    };

    struct my_system : mecs::system::instance
    {
        using instance::instance;
        using query_t = std::tuple
        <
            all<simple>           
        ,   none<my_tag>          
        >;

        xcore::random::small_generator R;

        xforceinline
        void operator() ( mecs::component::entity& Entity, const double_buff& DB ) noexcept
        {
            // This is the equivalent of removing a component (double_buff) from the entity
            if( (R.RandU32() % 10) == 0 )
                getArchetypeBy< std::tuple<my_tag>, std::tuple<> >( Entity, [&]( mecs::archetype::instance& Archetype ) constexpr noexcept
                {
                    moveEntityToArchetype( Entity, Archetype );
                });
            else if ((R.RandU32() % 10) == 0)
                getArchetypeBy< std::tuple<my_tag>, std::tuple<double_buff> >(Entity, [&](mecs::archetype::instance& Archetype) constexpr noexcept
                {
                    moveEntityToArchetype(Entity, Archetype, []( mecs::component::entity& Entity ) constexpr noexcept
                    {
                        int a = 0;
                    });
                });
            else if ((R.RandU32() % 10) == 0)
                getArchetypeBy< std::tuple<my_tag, some_share>, std::tuple<double_buff> >(Entity, [&](mecs::archetype::instance& Archetype) constexpr noexcept
                {
                    moveEntityToArchetype(Entity, Archetype, [](mecs::component::entity& Entity) constexpr noexcept
                    {
                        int a = 0;
                    }, some_share{ {}, {44} } );
                });

        }
    };

    struct my_system2 : mecs::system::instance
    {
        using instance::instance;
        using query_t = std::tuple
        <
            all<simple, my_tag>
        >;

        xforceinline
        void operator() ( const mecs::component::entity& Entity, const double_buff& DB ) noexcept
        {
            printf("Simple, double_buff and Tag\n");
        }
    };

    struct my_system3 : mecs::system::instance
    {
        using instance::instance;
        using query_t = std::tuple
        <
            all<simple, my_tag>
        >;

        xforceinline
        void operator() ( const mecs::component::entity& Entity ) noexcept
        {
            printf("Simple and Tag\n");
        }
    };

    struct my_system4 : mecs::system::instance
    {
        using instance::instance;
        using query_t = std::tuple
        <
            all<simple, my_tag>
        >;

        xforceinline
        void operator() (const mecs::component::entity& Entity, const some_share& Share ) noexcept
        {
            printf("Simple, Share and Tag\n");
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
        ,   some_share
        >();

        //
        // Create graph
        //
        auto& DefaultWorld  = *Universe.m_WorldDB[0];
        auto& System        = DefaultWorld.m_GraphDB.CreateGraphConnection<my_system>(DefaultWorld.m_GraphDB.m_StartSyncPoint, DefaultWorld.m_GraphDB.m_EndSyncPoint);
        auto& System2       = DefaultWorld.m_GraphDB.CreateGraphConnection<my_system2>(DefaultWorld.m_GraphDB.m_StartSyncPoint, DefaultWorld.m_GraphDB.m_EndSyncPoint);
        auto& System3       = DefaultWorld.m_GraphDB.CreateGraphConnection<my_system3>(DefaultWorld.m_GraphDB.m_StartSyncPoint, DefaultWorld.m_GraphDB.m_EndSyncPoint);
        auto& System4       = DefaultWorld.m_GraphDB.CreateGraphConnection<my_system4>(DefaultWorld.m_GraphDB.m_StartSyncPoint, DefaultWorld.m_GraphDB.m_EndSyncPoint);

        //
        // Create a few entities
        //
        DefaultWorld.m_ArchetypeDB.getOrCreateArchitype<simple, double_buff>()
            .CreateEntities(System, 10000000ull XCORE_CMD_ASSERT(/ 10000), {})([](simple& Simple, double_buff& Double)
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
