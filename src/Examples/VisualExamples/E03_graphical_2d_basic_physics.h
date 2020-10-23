
namespace mecs::examples::E03_graphical_2d_basic_physics
{
    namespace example = mecs::examples::E03_graphical_2d_basic_physics;

    //-----------------------------------------------------------------------------------------
    // Example Menu
    //-----------------------------------------------------------------------------------------
    using my_menu = mecs::examples::E02_graphical_2d_basic_physics::my_menu;
    static my_menu s_MyMenu;

    //----------------------------------------------------------------------------------------
    // COMPONENTS::
    //----------------------------------------------------------------------------------------
    namespace component = mecs::examples::E02_graphical_2d_basic_physics::component;

    //----------------------------------------------------------------------------------------  
    // PHYSICS::
    //----------------------------------------------------------------------------------------
    namespace physics
    {
        namespace system { struct advance_cell; }

        //----------------------------------------------------------------------------------------
        // WORLD GRID:: EVENT::
        //----------------------------------------------------------------------------------------
        namespace event
        {
            struct collision : mecs::examples::E02_graphical_2d_basic_physics::physics::event::collision
            {
                using system_t = system::advance_cell;
            };

            struct render : mecs::examples::E02_graphical_2d_basic_physics::physics::event::render
            {
                using system_t = system::advance_cell;
            };
        }

        //----------------------------------------------------------------------------------------
        // WORLD GRID:: TOOLS::
        //----------------------------------------------------------------------------------------
        using tools = mecs::examples::E02_graphical_2d_basic_physics::physics::tools;

        //----------------------------------------------------------------------------------------
        // WORLD GRID:: COMPONENTS::
        //----------------------------------------------------------------------------------------
        namespace component
        {
            constexpr static std::size_t max_entity_count = 128;
            struct lists : mecs::component::data
            {
                static constexpr auto type_data_access_v = data::type_data_access::QUANTUM_DOUBLE_BUFFER;

                struct vel
                {
                    std::int16_t m_X, m_Y;  // fixed point (1)6:9
                };

                std::array< mecs::component::entity,    max_entity_count >  m_lEntity;
                std::array< xcore::vector2,             max_entity_count >  m_lPosition;
                std::array< vel,                        max_entity_count >  m_lVelocity;        
                std::array< std::uint16_t,              max_entity_count >  m_lRadius;          // fixed point 8:8
            };

            using count = mecs::examples::E02_graphical_2d_basic_physics::physics::component::count;
        }

        //----------------------------------------------------------------------------------------
        // WORLD GRID:: SYSTEM::
        //----------------------------------------------------------------------------------------
        namespace system
        {
            //----------------------------------------------------------------------------------------
            inline
            auto& getDefaultCellPool(mecs::system::instance& System) noexcept
            {
                auto& Archetype = System.getOrCreateArchetype
                    < physics::component::lists
                    , physics::component::count
                    >();

                return Archetype.getOrCreateSpecializedPool(System, 1, 1000000);
            }

            //----------------------------------------------------------------------------------------
            // WORLD GRID:: SYSTEM:: ADVANCE CELL
            //----------------------------------------------------------------------------------------
            struct advance_cell : mecs::system::instance
            {
                constexpr static auto   type_name_v         = xconst_universal_str("advance_cell");
                constexpr static auto   entities_per_job_v  = 50;

                using mecs::system::instance::instance;

                using events_t = std::tuple
                <
                        event::collision
                    ,   event::render
                >;

                mecs::archetype::specialized_pool* m_pSpecializedPoolCell;

                void msgGraphInit(mecs::world::instance&) noexcept
                {
                    m_pSpecializedPoolCell = &getDefaultCellPool(*this);
                }

                xforceinline
                void operator()
                ( 
                    const   component::lists& T0List
                ,           component::lists& T1List
                ,           component::count& CountList
                ) noexcept
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
                    auto ID = tools::ToImaginarySpace(T0List.m_lPosition[0]);
                    {
                        //XCORE_PERF_ZONE_SCOPED_N("CacheCells")
                        const auto p = [&]( const tools::im_vector Delta, const int i ) noexcept 
                        {
                            if( false == findEntityComponentsRelax(  entity::guid{ CellGuids[i] = physics::tools::ComputeKeyFromPosition( ID + Delta ) }
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

                        T0CellMap[3]            = &T0List;
                        T1CellMap[3]            = &T1List;
                        CellMapCount[3]         = &CountList;
                        CellGuids[3]            = physics::tools::ComputeKeyFromPosition( ID );

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
                        xcore::vector2  T1Velocity      = xcore::vector2{ static_cast<float>(T0Cell.m_lVelocity[i].m_X) * 1.0f / static_cast<float>(1<<9)
                                                                        , static_cast<float>(T0Cell.m_lVelocity[i].m_Y) * 1.0f / static_cast<float>(1<<9) };
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

                        const auto Radius = T0Cell.m_lRadius[i] * 1.0f / static_cast<float>(1<<8);

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
                                    const auto OtherR               = T0Other.m_lRadius[c] * 1.0f / static_cast<float>(1<<8);
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
                                
                                getOrCreateEntityRelax( entity::guid{ CellGuids[Index] }, *m_pSpecializedPoolCell, CallBack, CallBack );
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

                        //
                        // Notify whoever cares about the info
                        //
                        EventNotify<event::render>( T0CellMap[3]->m_lEntity[i], T1Position, T1Velocity, Radius );
                    }

                    //
                    // Render grid
                    //
                    if(s_MyMenu.m_bRenderGrid)
                    {
                        const auto ImCellPos = tools::ToImaginarySpace( T0CellMap[3]->m_lPosition[0] );
                        const auto Position  = ImCellPos.getWorldPosition();

                        Draw2DQuad( Position
                        , xcore::vector2{ physics::tools::grid_width_half_v-1, physics::tools::grid_height_half_v -1 }
                        , (ImCellPos.m_Y&1)?0x88ffffaa:0x88ffffff );
                    }
                }
            };

            //----------------------------------------------------------------------------------------
            // WORLD GRID:: SYSTEM:: RESET COUNTS
            //----------------------------------------------------------------------------------------
            struct reset_counts : mecs::system::instance
            {
                constexpr static auto   type_name_v         = xconst_universal_str("reset_counts");
                constexpr static auto   entities_per_job_v  = 200;

                using mecs::system::instance::instance;

                // On the start of the game we are going to update all the Counts for the first frame.
                // Since entities have been created outside the system so our ReadOnlyCounter is set to zero.
                // But After this update we should be all up to date.
                void msgGraphInit (world::instance&) noexcept
                {
                    query Query;
                    ForEach(DoQuery< reset_counts >(Query), *this, entities_per_job_v);
                }

                // This system is reading T0 and it does not need to worry about people changing its value midway.
                xforceinline
                void operator() ( entity& Entity, component::count& Count ) noexcept
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
            struct create_spatial_entity : mecs::archetype::delegate::instance< mecs::archetype::event::create_entity >
            {
                mecs::archetype::specialized_pool* m_pSpecializedPoolCell = nullptr;

                // This function will be call ones per entity. Since we only will have one entity it will only be call ones per frame.
                void operator() (       system&                         System
                                , const entity&                         Entity
                                , const example::component::position&   Position
                                , const example::component::velocity&   Velocity
                                , const example::component::collider&   Collider ) noexcept
                {
                    const auto              ImgPosition     = tools::ToImaginarySpace(Position.m_Value);
                    const entity::guid      Guid            { physics::tools::ComputeKeyFromPosition( ImgPosition ) };

                    xassert_block_basic()
                    {
                        xassert( physics::tools::ComputeKeyFromPosition( tools::im_vector{0,0} ) == physics::tools::ComputeKeyFromPosition( tools::im_vector{1,0} ) );
                        xassert( physics::tools::ComputeKeyFromPosition( tools::im_vector{1,1} ) == physics::tools::ComputeKeyFromPosition( tools::im_vector{2,1} ) );

                        auto NewWorld   = ImgPosition.getWorldPosition();
                        auto ImgToG     = ImgPosition.getGridSpace();
                        xassert( physics::tools::ComputeKeyFromPosition( ImgPosition ) == physics::tools::ComputeKeyFromPosition( Position.m_Value ) );
                    }

                    const auto CallBack = [&]( component::lists& Lists, component::count& Count ) noexcept
                    {
                        const auto C = Count.m_MutableCount++;
                        Lists.m_lPosition[C]        = Position.m_Value;
                        Lists.m_lVelocity[C].m_X    = static_cast<std::int16_t>(Velocity.m_Value.m_X * (1<<9));
                        Lists.m_lVelocity[C].m_Y    = static_cast<std::int16_t>(Velocity.m_Value.m_Y * (1<<9));
                        Lists.m_lRadius[C]          = static_cast<std::uint16_t>(Collider.m_Radius * (1<<8));
                        Lists.m_lEntity[C]          = Entity;
                    };

                    if (m_pSpecializedPoolCell == nullptr)
                    {
                        m_pSpecializedPoolCell = &physics::system::getDefaultCellPool(System);
                    }

                    System.getOrCreateEntityRelax( Guid, *m_pSpecializedPoolCell, CallBack, CallBack );
                }
            };

            //----------------------------------------------------------------------------------------
            // WORLD GRID:: DELEGATE:: DESTROY SPATIAL ENTITY
            //----------------------------------------------------------------------------------------
            struct destroy_spatial_entity : mecs::archetype::delegate::instance< mecs::archetype::event::destroy_entity >
            {
                // This function will be call ones per entity. Since we only will have one entity it will only be call ones per frame.
                void operator() (         system&                       System
                                ,   const entity&                       Entity
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
        inline
        void ImportModule( mecs::universe::instance& Universe ) noexcept
        {
            Universe.registerTypes< component::lists, component::count >();
            Universe.registerTypes< delegate::create_spatial_entity, delegate::destroy_spatial_entity >();
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
        struct my_collision : mecs::system::delegate::instance< physics::event::collision >
        {
            xforceinline
            void operator() (system& System, const entity& E1, const entity& E2) const noexcept
            {
                //printf( ">>>>>>>>>>Collision \n");
            }
        };

        struct my_render : mecs::system::delegate::instance< physics::event::render >
        {
            xforceinline
            void operator() (mecs::system::instance&, const entity& Entity, const xcore::vector2& Position, const xcore::vector2&, float R) const noexcept
            {
                Draw2DQuad(Position, xcore::vector2{ R }, ~0);
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

        auto    upUniverse = std::make_unique<mecs::universe::instance>();
        auto& DefaultWorld = *upUniverse->m_WorldDB[0];

        //------------------------------------------------------------------------------------------
        // Registration
        //------------------------------------------------------------------------------------------

        // Import that world_grid system
        physics::ImportModule(*upUniverse);

        //
        // Register Components
        //
        upUniverse->registerTypes<component::position, component::collider, component::velocity>();

        //
        // Register the game graph.
        // 
        auto& SyncPhysics = DefaultWorld.CreateSyncPoint();
        DefaultWorld.CreateGraphConnection<physics::system::advance_cell>  (DefaultWorld.getStartSyncpoint(),   SyncPhysics);
        DefaultWorld.CreateGraphConnection<system::render_pageflip>        (SyncPhysics,                        DefaultWorld.getEndSyncpoint()).m_Menu = [&] { return Menu(DefaultWorld, s_MyMenu); };
        DefaultWorld.CreateGraphConnection<physics::system::reset_counts>  (SyncPhysics,                        DefaultWorld.getEndSyncpoint());

        //
        // Create the delegates.
        //
        DefaultWorld.CreateArchetypeDelegate< physics::delegate::create_spatial_entity >();
        DefaultWorld.CreateArchetypeDelegate< physics::delegate::destroy_spatial_entity >();

        DefaultWorld.CreateSystemDelegate< delegate::my_render    >();

        //------------------------------------------------------------------------------------------
        // Initialization
        //------------------------------------------------------------------------------------------

        //
        // Create an entity group
        //
        auto& Archetype = DefaultWorld.getOrCreateArchitype<component::position, component::collider, component::velocity>();

        //
        // Create Entities
        //
        xcore::lock::object<xcore::random::small_generator, xcore::lock::spin> Random;
        DefaultWorld.CreateEntities(Archetype, s_MyMenu.m_EntitieCount, {}
            , [&](component::position& Position
                , component::velocity& Velocity
                , component::collider& Collider)
            {
                xcore::lock::scope Lk(Random);
                auto& Rnd = Random.get();

                Position.m_Value.setup(Rnd.RandF32(-(physics::tools::world_width_v / 2.0f), (physics::tools::world_width_v / 2.0f))
                    , Rnd.RandF32(-(physics::tools::world_height_v / 2.0f), (physics::tools::world_height_v / 2.0f)));

                Velocity.m_Value.setup(Rnd.RandF32(-1.0f, 1.0f), Rnd.RandF32(-1.0f, 1.0f));
                Velocity.m_Value.NormalizeSafe();
                Velocity.m_Value *= 10.0f;

                Collider.m_Radius = Rnd.RandF32(1.5f, 3.0f);
            });

        //------------------------------------------------------------------------------------------
        // Running
        //------------------------------------------------------------------------------------------

        while (system::render_pageflip::s_bContinue)
        {
            DefaultWorld.Play();
        }
    }
}
