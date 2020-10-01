
namespace mecs::examples::E05_graphical_2d_virus_spread
{
    namespace example = mecs::examples::E05_graphical_2d_virus_spread;

    //-----------------------------------------------------------------------------------------
    // Example Menu
    //-----------------------------------------------------------------------------------------
    struct my_menu2 : mecs::examples::E02_graphical_2d_basic_physics::my_menu
    {
        int     m_RecoveringPeriod;
        int     m_MinimunRecoveringPeriod;
        int     m_ImmunityPeriod;
        bool    m_bStart;
        int     m_WhenLockDown;
        int     m_LockDownTime;
        float   m_ChanceOfTesting;
        float   m_ChanceOfLockdown;

        my_menu2() { reset(); }
        void reset()
        {
            m_EntitieCount              = 200000 XCORE_CMD_DEBUG( / 100 );
            m_RecoveringPeriod          = 512;
            m_MinimunRecoveringPeriod   = 100;
            m_ImmunityPeriod            = 512;
            m_bStart                    = false;
            m_LockDownTime              = m_RecoveringPeriod/2;
            m_WhenLockDown              = m_RecoveringPeriod/2;
            m_ChanceOfTesting           = 1.0f;
            m_ChanceOfLockdown          = 0.0f;
        }

        property_vtable()
    };
    static my_menu2 s_MyMenu;


    //----------------------------------------------------------------------------------------
    // COMPONENTS::
    //----------------------------------------------------------------------------------------
    namespace component
    {
        using namespace mecs::examples::E02_graphical_2d_basic_physics::component;

        struct health
        {
            constexpr static mecs::type_guid type_guid_v    {"mecs::examples::E05_graphical_2d_virus_spread::component::health"};
            constexpr static auto            name_v         = xconst_universal_str("health");

            std::uint16_t       m_InfectionCountdown    {0};
            std::uint16_t       m_InHouseCountdown      {0};
            union
            {
                std::uint8_t    m_Flags                 {0};
                struct
                {
                    bool        m_Inmune:1
                    ,           m_InHouse:1;
                };
            };
        };
    }

    //----------------------------------------------------------------------------------------  
    // WORLD GRID::
    //----------------------------------------------------------------------------------------
    namespace world_grid
    {
        namespace system { struct advance_cell; }

        //----------------------------------------------------------------------------------------
        // WORLD GRID:: EVENT::
        //----------------------------------------------------------------------------------------
        namespace event
        {
            struct collision : mecs::examples::E02_graphical_2d_basic_physics::world_grid::event::collision
            {
                using system_t = system::advance_cell;
            };

            struct render : mecs::event::instance
            {
                using                       event_t         = mecs::event::type< mecs::system::instance&        // System
                                                                                , const mecs::entity::instance& // Entity
                                                                                , const xcore::vector2&         // T0Position
                                                                                ,       xcore::vector2&         // T1Position
                                                                                , float >;                      // Radius
                constexpr static auto       guid_v          = mecs::event::guid     { "world_grid::event::render" };
                constexpr static auto       name_v          = xconst_universal_str  ( "world_grid::event::render" );
                using                       system_t        = system::advance_cell;
            };
        }

        //----------------------------------------------------------------------------------------
        // WORLD GRID:: TOOLS::
        //----------------------------------------------------------------------------------------
        using tools = mecs::examples::E02_graphical_2d_basic_physics::world_grid::tools;

        //----------------------------------------------------------------------------------------
        // WORLD GRID:: COMPONENTS::
        //----------------------------------------------------------------------------------------
        namespace component
        {
            constexpr static std::size_t max_entity_count = 64;
            struct lists : mecs::component::quantum_double_buffer
            {
                constexpr static mecs::type_guid type_guid_v{"mecs::examples::E03_graphical_2d_basic_physics::world_grid::component::lists"};

                struct vel
                {
                    std::int16_t m_X, m_Y;  // fixed point (1)6:9
                };

                std::array< mecs::entity::instance, max_entity_count >  m_lEntity;
                std::array< xcore::vector2,         max_entity_count >  m_lPosition;
                std::array< vel,                    max_entity_count >  m_lVelocity;        
                std::array< std::uint16_t,          max_entity_count >  m_lRadius;          // fixed point 8:8
            };

            using count = mecs::examples::E02_graphical_2d_basic_physics::world_grid::component::count;
        }

        //----------------------------------------------------------------------------------------
        // WORLD GRID:: SYSTEM::
        //----------------------------------------------------------------------------------------
        namespace system
        {
            //----------------------------------------------------------------------------------------
            // WORLD GRID:: SYSTEM:: ADVANCE CELL
            //----------------------------------------------------------------------------------------
            struct advance_cell : mecs::system::instance
            {
                constexpr static auto   name_v              = xconst_universal_str("advance_cell");
                constexpr static auto   guid_v              = mecs::system::guid{ "world_grid::advance_cell" };
                constexpr static auto   entities_per_job_v  = 50;

                using mecs::system::instance::instance;

                using access_t = mecs::query::access
                <       const component::lists&
                    ,         component::lists&
                    ,         component::count&
                >;

                using events_t = std::tuple
                <
                        event::collision
                    ,   event::render
                >;

                xforceinline
                void operator() ( const access_t& Component ) noexcept
                {
                    std::array< const component::lists*, 7 > T0CellMap    ;
                    std::array<       component::lists*, 7 > T1CellMap    ;
                    std::array<       component::count*, 7 > CellMapCount ;
                    std::array<       std::uint64_t,     7 > CellGuids    ;

                    //   +---+---+     | New grid distribution. Note that the center is cell 3.
                    //   | 0 | 1 |     |
                    // +-+-+-+-+-+-+   |
                    // | 2 | 3 | 4 |   |
                    // +-+-+-+-+-+-+   |
                    //   | 5 | 6 |     |
                    //   +---+---+     |

                    //
                    // Cache all relevant cells
                    //
                    //xassert( Component.get< component::count >().m_ReadOnlyCount );
                    auto ID = tools::ToImaginarySpace(Component.get< const component::lists& >().m_lPosition[0]);
                    {
                        //XCORE_PERF_ZONE_SCOPED_N("CacheCells")
                        const auto p = [&]( const tools::im_vector Delta, const int i ) noexcept 
                        {
                            if( false == findEntity (  mecs::entity::guid{ CellGuids[i] = world_grid::tools::ComputeKeyFromPosition( ID + Delta ) }
                                , [&]( component::count& Count, const component::lists& T0Lists, component::lists& T1Lists ) constexpr noexcept
                                {
                                    T0CellMap[i]    = &T0Lists;
                                    T1CellMap[i]    = &T1Lists;
                                    CellMapCount[i] = &Count;
                                })
                            )
                            {
                                T0CellMap[i]    = nullptr;
                                CellMapCount[i] = nullptr;
                            }
                        };

                        T0CellMap[3]            = &Component.get< const component::lists& >  ();
                        T1CellMap[3]            = &Component.get<       component::lists& >  ();
                        CellMapCount[3]         = &Component.get<       component::count& >  ();
                        CellGuids[3]            = world_grid::tools::ComputeKeyFromPosition( ID );

                        p({ -1,-1 }, 0 );
                        p({  1,-1 }, 1 );
                        p({ -2, 0 }, 2 );
                        p({  2, 0 }, 4 );
                        p({ -1, 1 }, 5 );
                        p({  1, 1 }, 6 );
                    }

                    //
                    // Advance physics for our cell
                    //
                    constexpr static std::array imgTable  = 
                    {
                        0, 0, 1, 1, 
                        2, 3, 3, 4,
                        5, 5, 6, 6,
                    };

                    //XCORE_PERF_ZONE_SCOPED_N("AdvancePhysics")
                    const float DT      = getTime().m_DeltaTime;
                    for( std::uint8_t CountT = CellMapCount[3]->m_ReadOnlyCount, i=0; i<CountT; ++i )
                    {
                        //
                        // Integrate
                        //
                        const auto&     T0Cell          = *T0CellMap[3];
                        xcore::vector2  T1Velocity      = xcore::vector2{ static_cast<float>(T0Cell.m_lVelocity[i].m_X) / static_cast<float>(1<<9)
                                                                        , static_cast<float>(T0Cell.m_lVelocity[i].m_Y) / static_cast<float>(1<<9) };
                        xcore::vector2  T1Position      = T0Cell.m_lPosition[i];
                        const auto      T0ImgPosition   = tools::ToImaginarySpace(T1Position);

                        // Integrate 
                        T1Position += T1Velocity * DT;

                        xassert( auto xx = ((T0ImgPosition.m_Y&1)?(1-(T0ImgPosition.m_X&1)):(T0ImgPosition.m_X&1)); xx == (1&(T0ImgPosition.m_Y^T0ImgPosition.m_X)) );
                        const auto  TableOrigenOffsets  = tools::im_vector
                        {
                              1 + (1&(T0ImgPosition.m_Y^T0ImgPosition.m_X)) 
                            , 1
                        };

                        const auto Radius = T0Cell.m_lRadius[i] / static_cast<float>(1<<8);

                        //
                        // Check for collisions with other objects
                        //
                        {
                            //XCORE_PERF_ZONE_SCOPED_N("CheckCollisions")
                            auto                    SearchBoxB = tools::MakeBox( T0Cell.m_lPosition[i], T1Position, Radius );
                            int                     iCache     = -1;

                            //
                            // Make sure to map from the SearchBox to our imaginary box
                            //
                            SearchBoxB.m_Max = SearchBoxB.m_Max - T0ImgPosition;
                            SearchBoxB.m_Min = SearchBoxB.m_Min - T0ImgPosition;

                            for( int y = SearchBoxB.m_Min.m_Y; y<=SearchBoxB.m_Max.m_Y; y++ )
                            for( int x = SearchBoxB.m_Min.m_X; x<=SearchBoxB.m_Max.m_X; x++ )
                            {
                                const auto  TableRelativeImgPos = tools::im_vector{ x, y } + TableOrigenOffsets;
                                xassert( TableRelativeImgPos.m_X >= 0 );
                                xassert( TableRelativeImgPos.m_X <= 4 );
                                xassert( TableRelativeImgPos.m_Y >= 0 );
                                xassert( TableRelativeImgPos.m_Y <= 2 );

                                const auto Index = imgTable[ TableRelativeImgPos.m_X + TableRelativeImgPos.m_Y * 4 ];
                                if( iCache == Index || T0CellMap[Index] == nullptr ) continue;
                                iCache = Index;

                                for( std::uint8_t Count = CellMapCount[Index]->m_ReadOnlyCount, c = 0; c < Count; ++c )
                                {
                                    const auto& T0Other = *T0CellMap[Index];
                                    if( i == c && &T0Cell == &T0Other ) continue;
                                    
                                    // Check for collision
                                    const auto V                    = (T0Other.m_lPosition[c] - T1Position);
                                    const auto OtherR               = T0Other.m_lRadius[c]/static_cast<float>(1<<8);
                                    const auto MinDistanceSquare    = xcore::math::Sqr(OtherR + Radius);
                                    const auto DistanceSquare       = V.Dot(V);
                                    if( DistanceSquare >= 0.001f && DistanceSquare <= MinDistanceSquare )
                                    {
                                        // Do hacky collision response
                                        const auto Normal           = V * -xcore::math::InvSqrt(DistanceSquare);
                                        T1Velocity = xcore::math::vector2::Reflect( Normal, T1Velocity );
                                        T1Position = T0Other.m_lPosition[c] + Normal * (OtherR + Radius+0.01f);

                                        // Notify entities about the collision
                                        EventNotify<event::collision>( T0Cell.m_lEntity[i], T0Other.m_lEntity[c] );
                                    }
                                }
                            }
                        }

                        //
                        // Collide with the world
                        //
                        {
                            if( (T1Position.m_X - Radius) <= (tools::world_width_v/-2) )
                            {
                                T1Position.m_X = (tools::world_width_v/-2) + Radius;
                                T1Velocity.m_X = -T1Velocity.m_X;
                            }
                            else if( (T1Position.m_X + Radius) >= (tools::world_width_v/2) )
                            {
                                T1Position.m_X = (tools::world_width_v/2) - Radius;
                                T1Velocity.m_X = -T1Velocity.m_X;
                            }

                            if( (T1Position.m_Y - Radius) <= (tools::world_height_v/-2) )
                            {
                                T1Position.m_Y = (tools::world_height_v/-2) + Radius;
                                T1Velocity.m_Y = -T1Velocity.m_Y;
                            }
                            else if( (T1Position.m_Y + Radius) >= (tools::world_height_v/2) )
                            {
                                T1Position.m_Y = (tools::world_height_v/2) - Radius;
                                T1Velocity.m_Y = -T1Velocity.m_Y;
                            }
                        }

                        //
                        // Notify whoever cares about the info
                        //
                        EventNotify<event::render>( T0CellMap[3]->m_lEntity[i], T0Cell.m_lPosition[i], T1Position, Radius );

                        //
                        // Make sure we have the cells in question
                        //
                        const auto  T1ImgPosition       = tools::ToImaginarySpace( T1Position );
                        const auto  RelativeImgPos      = T1ImgPosition - T0ImgPosition + TableOrigenOffsets;
                        const auto  Index               = imgTable[ RelativeImgPos.m_X + RelativeImgPos.m_Y * 4 ];
                        xassert( auto xxx = tools::ComputeKeyFromPosition(T1ImgPosition); CellGuids[Index] == xxx );

                        int C;
                        {
                            // XCORE_PERF_ZONE_SCOPED_N("MoveToT1")
                            //
                            // Get the buffers
                            //
                            if( CellMapCount[Index] == nullptr )
                            {
                                const auto CallBack = [&]( component::count& Count, component::lists& T1Lists ) constexpr noexcept
                                {
                                    CellMapCount[Index]          = &Count;
                                    T1CellMap[Index]             = &T1Lists;
                                };
                                
                                getOrCreatEntity<component::lists, component::count>( mecs::entity::guid{ CellGuids[Index] }, CallBack, CallBack );
                            }

                            //
                            // Copy the entry in to the cell
                            //
                            xassert( T1ImgPosition.m_Y == tools::WorldToGridSpaceY(T1Position.m_Y) );
                                        C               = CellMapCount[Index]->m_MutableCount++;
                            auto&       Entry           = *T1CellMap[Index];
                            Entry.m_lPosition[C]        = T1Position;
                            Entry.m_lVelocity[C].m_X    = static_cast<std::int16_t>(T1Velocity.m_X * (1<<9));
                            Entry.m_lVelocity[C].m_Y    = static_cast<std::int16_t>(T1Velocity.m_Y * (1<<9));
                            Entry.m_lRadius[C]          = T0Cell.m_lRadius[i];
                            Entry.m_lEntity[C]          = T0Cell.m_lEntity[i];
                        }
                    }

                    //
                    // Render grid
                    //
                    if(s_MyMenu.m_bRenderGrid)
                    {
                        const auto ImCellPos = tools::ToImaginarySpace( T0CellMap[3]->m_lPosition[0] );
                        const auto Position  = ImCellPos.getWorldPosition();

                        Draw2DQuad( Position
                        , xcore::vector2{ world_grid::tools::grid_width_half_v-1, world_grid::tools::grid_height_half_v -1 }
                        , (ImCellPos.m_Y&1)?0x88ffffaa:0x88ffffff );
                    }
                }
            };

            //----------------------------------------------------------------------------------------
            // WORLD GRID:: SYSTEM:: RESET COUNTS
            //----------------------------------------------------------------------------------------
            struct reset_counts : mecs::system::instance
            {
                constexpr static auto   name_v = xconst_universal_str("reset_counts");
                constexpr static auto   guid_v = mecs::system::guid{ "reset_counts" };
                constexpr static auto   entities_per_job_v  = 200;

                using mecs::system::instance::instance;

                // On the start of the game we are going to update all the Counts for the first frame.
                // Since entities have been created outside the system so our ReadOnlyCounter is set to zero.
                // But After this update we should be all up to date.
                void msgWorldStart ( world::instance& ) noexcept
                {
                    query::instance Query;
                    DoQuery<reset_counts>( Query );
                    if( false == ForEach<entities_per_job_v>(Query,*this) )
                    {
                        xassert(false);
                    }
                }

                // This system is reading T0 and it does not need to worry about people changing its value midway.
                xforceinline
                void operator() ( mecs::entity::instance& Entity, component::count& Count ) noexcept
                {
                    Count.m_ReadOnlyCount = Count.m_MutableCount.load(std::memory_order_relaxed);
                    if( Count.m_ReadOnlyCount == 0 )
                    {
                        deleteEntity(Entity);
                    }
                    else
                    {
                        Count.m_MutableCount.store(0,std::memory_order_relaxed);
                    }
                }
            };
        }

        //----------------------------------------------------------------------------------------
        // WORLD GRID:: DELEGATE::
        //----------------------------------------------------------------------------------------
        namespace delegate
        {
            //----------------------------------------------------------------------------------------
            // WORLD GRID:: SYSTEM:: CREATE SPATIAL ENTITY
            //----------------------------------------------------------------------------------------
            struct create_spatial_entity : mecs::delegate::instance< mecs::event::create_entity >
            {
                constexpr static mecs::delegate::guid guid_v{ "world_grid::delegate::create_spatial_entity" };

                // This function will be call ones per entity. Since we only will have one entity it will only be call ones per frame.
                void operator() (       mecs::system::instance&         System
                                , const mecs::entity::instance&         Entity
                                , const example::component::position&   Position
                                , const example::component::velocity&   Velocity
                                , const example::component::collider&   Collider ) const noexcept
                {
                    const auto               ImgPosition    = tools::ToImaginarySpace(Position);
                    const mecs::entity::guid Guid           { world_grid::tools::ComputeKeyFromPosition( ImgPosition ) };

                    xassert_block_basic()
                    {
                        xassert( world_grid::tools::ComputeKeyFromPosition( tools::im_vector{0,0} ) == world_grid::tools::ComputeKeyFromPosition( tools::im_vector{1,0} ) );
                        xassert( world_grid::tools::ComputeKeyFromPosition( tools::im_vector{1,1} ) == world_grid::tools::ComputeKeyFromPosition( tools::im_vector{2,1} ) );

                        auto NewWorld   = ImgPosition.getWorldPosition();
                        auto ImgToG     = ImgPosition.getGridSpace();
                        xassert( world_grid::tools::ComputeKeyFromPosition( ImgPosition ) == world_grid::tools::ComputeKeyFromPosition( Position ) );
                    }

                    const auto CallBack = [&]( component::lists& Lists, component::count& Count ) noexcept
                    {
                        const auto C = Count.m_MutableCount++;
                        Lists.m_lPosition[C]        = Position;
                        Lists.m_lVelocity[C].m_X    = static_cast<std::int16_t>(Velocity.m_X * (1<<9));
                        Lists.m_lVelocity[C].m_Y    = static_cast<std::int16_t>(Velocity.m_Y * (1<<9));
                        Lists.m_lRadius[C]          = static_cast<std::uint16_t>(Collider.m_Radius * (1<<8));
                        Lists.m_lEntity[C]          = Entity;
                    };

                    System.getOrCreatEntity< component::lists, component::count >( Guid, CallBack, CallBack );
                }
            };

            //----------------------------------------------------------------------------------------
            // WORLD GRID:: DELEGATE:: DESTROY SPATIAL ENTITY
            //----------------------------------------------------------------------------------------
            struct destroy_spatial_entity : mecs::delegate::instance< mecs::event::destroy_entity >
            {
                constexpr static mecs::delegate::guid guid_v{ "world_grid::delegate::destroy_spatial_entity" };

                // This function will be call ones per entity. Since we only will have one entity it will only be call ones per frame.
                void operator() (         mecs::system::instance&       System
                                ,   const mecs::entity::instance&       Entity
                                ,   const example::component::position& Position ) const noexcept
                {
                    /*
                    const auto               WorldPosition = tools::ToGridSpace(Position);
                    const mecs::entity::guid Guid{ world_grid::tools::ComputeKeyFromPosition( WorldPosition ) };

                    // remove entity from the spatial grid
                    System.getEntity( Guid,
                        [&]( mecs::query::define<mecs::query::access
                        <
                                component::countentity
                        >>::access& Access )
                        {
                            auto& lList = Access.get<component::countentity>().m_lEntity;
                            for( std::uint16_t end = Access.get<component::countentity>().m_Count.load( std::memory_order_relaxed ), i=0u; i<end; ++i )
                            {
                                if( &lList[i].getPointer() == &Entity.getPointer() )
                                {
                                    lList[i].MarkAsZombie();
                                    return;
                                }
                            }
                            xassert(false);
                        });
                    */                        
                }
            };
        }

        //----------------------------------------------------------------------------------------
        // WORLD GRID:: IMPORT
        //----------------------------------------------------------------------------------------
        void ImportModule( mecs::world::instance& World ) noexcept
        {
            World.registerComponents< component::lists, component::count >();
            World.registerDelegates< delegate::create_spatial_entity, delegate::destroy_spatial_entity >();
        }
    }

    //----------------------------------------------------------------------------------------
    // SYSTEMS::
    //----------------------------------------------------------------------------------------
    namespace system = mecs::examples::E02_graphical_2d_basic_physics::system;

    //----------------------------------------------------------------------------------------
    // DELEGATE::
    //----------------------------------------------------------------------------------------
    namespace delegate
    {
        struct my_collision : mecs::delegate::instance< world_grid::event::collision >
        {
            constexpr static mecs::delegate::guid guid_v{ "my_collision_delegate" };

            xcore::random::small_generator Rnd;
            xforceinline 
            void operator() ( mecs::system::instance&               System
                            , const mecs::entity::instance&         E1
                            , const mecs::entity::instance&         E2 ) const noexcept
            {
                System.getEntity( E1, [&]( example::component::health& Health1)
                {
                    if( Health1.m_Inmune || Health1.m_InHouse ) return;

                    System.getEntity( E2, [&]( example::component::health& Health2)
                    {
                        if( Health2.m_Inmune || Health2.m_InHouse || (Health2.m_InfectionCountdown && Health1.m_InfectionCountdown) ) return;
                        if( Health1.m_InfectionCountdown )          Health2.m_InfectionCountdown = static_cast<std::uint16_t>(E1.getGuid().m_Value%s_MyMenu.m_RecoveringPeriod + s_MyMenu.m_MinimunRecoveringPeriod);
                        else if( Health2.m_InfectionCountdown )     Health1.m_InfectionCountdown = static_cast<std::uint16_t>(E2.getGuid().m_Value%s_MyMenu.m_RecoveringPeriod + s_MyMenu.m_MinimunRecoveringPeriod);
                    });
                });
            }
        };

        struct my_render : mecs::delegate::instance< world_grid::event::render >
        {
            constexpr static mecs::delegate::guid guid_v{ "my_render" };

            xforceinline 
            void operator() ( mecs::system::instance&       System
                            , const mecs::entity::instance& Entity
                            , const xcore::vector2&         T0Position
                            ,       xcore::vector2&         T1Position
                            , float                         R ) const noexcept
            {
                System.getEntity( Entity, [&]( example::component::health& Health )
                {
                    if( Health.m_InHouse )
                    {
                        T1Position       = T0Position;
                        Health.m_InHouseCountdown--;
                        if( Health.m_InHouseCountdown == 0 )
                        {
                            Health.m_InHouse = false;
                        }
                    }
                    else
                    {
                        if( (Entity.getGuid().m_Value%100) >= static_cast<int>((1.0f-s_MyMenu.m_ChanceOfLockdown)*100) )
                        {
                            Health.m_InHouse = true;
                            Health.m_InHouseCountdown = s_MyMenu.m_LockDownTime;
                        }
                    }

                    if( Health.m_InfectionCountdown )
                    {
                        --Health.m_InfectionCountdown;

                        if( Health.m_InfectionCountdown == s_MyMenu.m_WhenLockDown && Health.m_Inmune == false )
                        {
                            if( (Entity.getGuid().m_Value%100) >= static_cast<int>((1.0f-s_MyMenu.m_ChanceOfTesting)*100) )
                            {
                                Health.m_InHouse = true;
                                Health.m_InHouseCountdown = s_MyMenu.m_LockDownTime;
                            }
                        }

                        if( Health.m_InfectionCountdown == 0 ) 
                        {
                            if( Health.m_Inmune )
                            {
                                Health.m_Inmune = false;
                                Health.m_InfectionCountdown = 0;
                            }
                            else
                            {
                                Health.m_Inmune = true;
                                Health.m_InfectionCountdown = s_MyMenu.m_ImmunityPeriod;
                            }
                        }
                    }
                        
                    if( Health.m_InHouse                 ) Draw2DQuad( T1Position, xcore::vector2{ R+1.0f }, 0xff000000 );
                    if( Health.m_Inmune                  ) Draw2DQuad( T1Position, xcore::vector2{ R }, 0xff00ff00 );
                    else if( Health.m_InfectionCountdown ) Draw2DQuad( T1Position, xcore::vector2{ R }, 0xff0000ff );
                    else                                   Draw2DQuad( T1Position, xcore::vector2{ R }, ~0 );
                });
            }
        };

    }

    //-----------------------------------------------------------------------------------------
    // Test
    //-----------------------------------------------------------------------------------------

    void Test( bool(*Menu)(mecs::world::instance&,xcore::property::base&) )
    {
        printf( "--------------------------------------------------------------------------------\n");
        printf( "E03_graphical_2d_basic_physics\n");
        printf( "--------------------------------------------------------------------------------\n");

        auto upWorld = std::make_unique<mecs::world::instance>();

        //------------------------------------------------------------------------------------------
        // Registration
        //------------------------------------------------------------------------------------------

        // Import that world_grid system
        world_grid::ImportModule( *upWorld );

        //
        // Register Components
        //
        upWorld->registerComponents<component::position,component::collider,component::velocity,component::health>();

        //
        // Register Delegates
        //
        upWorld->registerDelegates<delegate::my_collision,delegate::my_render>();

        //
        // Synpoint before the physics
        //
        mecs::sync_point::instance SyncPhysics;

        //
        // Register the game graph.
        // 
        upWorld->registerGraphConnection<world_grid::system::advance_cell>  ( upWorld->m_StartSyncPoint,    SyncPhysics );
        upWorld->registerGraphConnection<system::render_pageflip>           ( SyncPhysics,                  upWorld->m_EndSyncPoint ).m_Menu = [&] { return Menu(*upWorld, s_MyMenu); };
        upWorld->registerGraphConnection<world_grid::system::reset_counts>  ( SyncPhysics,                  upWorld->m_EndSyncPoint );

        //
        // Done with all the registration
        //
        upWorld->registerFinish();

        //------------------------------------------------------------------------------------------
        // Initialization
        //------------------------------------------------------------------------------------------

        //
        // Create an entity group
        //
        auto& Group = upWorld->getOrCreateGroup<component::position,component::collider,component::velocity,component::health>();

        //
        // Create Entities
        //
        xcore::random::small_generator Rnd;
        for( int i=0; i<s_MyMenu.m_EntitieCount; i++ )
            upWorld->createEntity( Group, [&](  component::position&  Position
                                            ,   component::velocity&  Velocity
                                            ,   component::collider&  Collider )
            {

                Position.setup( Rnd.RandF32(-(world_grid::tools::world_width_v/2.0f), (world_grid::tools::world_width_v/2.0f) )
                              , Rnd.RandF32(-(world_grid::tools::world_height_v/2.0f), (world_grid::tools::world_height_v/2.0f) ) );

                Velocity.setup( Rnd.RandF32(-1.0f, 1.0f ), Rnd.RandF32(-1.0f, 1.0f ) );
                Velocity.NormalizeSafe();
                Velocity *= 10.0f;

                Collider.m_Radius = Rnd.RandF32( 1.5f, 3.0f );
            });


        mecs::entity::guid Guid;
        Guid.Reset();
        upWorld->createEntity( Group, [&](  component::position&  Position
                                        ,   component::velocity&  Velocity
                                        ,   component::collider&  Collider )
        {
            Position.setup( 0, 0);

            Velocity.setup( Rnd.RandF32(-1.0f, 1.0f ), Rnd.RandF32(-1.0f, 1.0f ) );
            Velocity.NormalizeSafe();
            Velocity *= 10.0f;

            Collider.m_Radius = Rnd.RandF32( 1.5f, 3.0f );

        }, Guid);

        //------------------------------------------------------------------------------------------
        // Running
        //------------------------------------------------------------------------------------------

        //
        // Start executing the world
        //
        upWorld->Start();

        //
        // run 100 frames
        //
        while (system::render_pageflip::s_bContinue)
        {
            if(s_MyMenu.m_bStart)
            {
                s_MyMenu.m_bStart = false;
                upWorld->getEntity( Guid, [&]( component::health& Health )
                {
                    Health.m_InfectionCountdown = s_MyMenu.m_RecoveringPeriod;    
                });
            }
            upWorld->Resume();
        }
            
    }
}

property_begin_name( mecs::examples::E05_graphical_2d_virus_spread::my_menu2, "Settings" )
{
    property_parent(my_menu)
,   property_var( m_RecoveringPeriod )
        .Help("Added time that a person will be sick. This range is assign base on a random chance \n"
              "So the final value assign to the person is a random number from 0 to the Recovering Period Value.\n")
,   property_var( m_MinimunRecoveringPeriod )
        .Help("This is how long a person will be sick for sure. This + Chance of 'RecoveringPeriod' will be the final\n"
              "Value on how long an individual person will be sick.\n")
,   property_var( m_ImmunityPeriod )
        .Help("This is how long a person will be immune from the virus.")
,   property_var( m_WhenLockDown )
        .Help("When a person is sick how long before they lock themselves in their home.")
,   property_var( m_LockDownTime )
        .Help("How long a person stay home after they realized that they got sick or the government ask them.")
,   property_var( m_ChanceOfTesting )
        .EDStyle(property::edstyle<float>::ScrollBar( 0.0f, 1.0f ) )
        .Help("Chance a person will get tested when they are sick. (When a person is sick and get tested they will stay at home).")
,   property_var( m_ChanceOfLockdown )
        .EDStyle(property::edstyle<float>::ScrollBar( 0.0f, 1.0f ) )
        .Help("This is a random probability that the government will lock down a person even when not sick.")
,   property_var_fnbegin( "StartVirus", string_t )
    {
        if( isRead ) xcore::string::Copy( InOut, xcore::string::constant{"Click To Start"} );
        else         Self.m_bStart = true;
    }
    property_var_fnend()
        .EDStyle(property::edstyle<string_t>::Button())
        .Help("Gives the virus to the first person.")
}
property_vend_h(mecs::examples::E05_graphical_2d_virus_spread::my_menu2);

