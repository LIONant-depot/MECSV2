
namespace mecs::examples::E02_graphical_2d_basic_physics
{
    namespace example = mecs::examples::E02_graphical_2d_basic_physics;

    //-----------------------------------------------------------------------------------------
    // Example Menu
    //-----------------------------------------------------------------------------------------
    using my_menu = mecs::examples::E01_graphical_2d_basic_physics::my_menu;
    static my_menu s_MyMenu;

    //----------------------------------------------------------------------------------------
    // COMPONENTS::
    //----------------------------------------------------------------------------------------
    namespace component = mecs::examples::E01_graphical_2d_basic_physics::component;

    //----------------------------------------------------------------------------------------
    // WORLD GRID::
    //----------------------------------------------------------------------------------------
    namespace world_grid
    {
        //----------------------------------------------------------------------------------------
        // WORLD GRID:: EVENTS::
        //----------------------------------------------------------------------------------------
        namespace system { struct advance_cell; }
        namespace event
        {
            struct collision : mecs::examples::E01_graphical_2d_basic_physics::world_grid::event::collision
            {
                using system_t = system::advance_cell;
            };

            struct render : mecs::examples::E01_graphical_2d_basic_physics::world_grid::event::render
            {
                using system_t = system::advance_cell;
            };
        }

        //----------------------------------------------------------------------------------------
        // WORLD GRID:: TOOLS::
        //----------------------------------------------------------------------------------------
        template< int T_GRID_WIDTH_V, int T_GRID_HEIGHT_V, int T_WORLD_WIDTH = 5500, int T_WORLD_HEIGHT = 3200 >
        struct tools_kit
        {
            constexpr static int    grid_width_v  = T_GRID_WIDTH_V; static_assert( xcore::bits::isDivBy2PowerX(grid_width_v, 1));
            constexpr static int    grid_height_v = T_GRID_HEIGHT_V; static_assert( xcore::bits::isDivBy2PowerX(grid_height_v, 1));

            constexpr static int    grid_width_half_v  = grid_width_v/2;
            constexpr static int    grid_height_half_v = grid_height_v/2;


            constexpr static float  world_width_v  = static_cast<float>(T_WORLD_WIDTH);
            constexpr static float  world_height_v = static_cast<float>(T_WORLD_HEIGHT);


            // https://stackoverflow.com/questions/17035464/a-fast-method-to-round-a-double-to-a-32-bit-int-explained
            xforceinline static
            int double2int(double d)
            {
                d += 6755399441055744.0;
                return reinterpret_cast<int&>(d);
            }

            xforceinline static
            int WorldToImaginarySpaceX( float X ) noexcept { return double2int( X / grid_width_half_v ); }

            xforceinline static
            int WorldToImaginarySpaceY( float Y ) noexcept { return double2int( Y / grid_height_v ); }

            xforceinline static
            int WorldToGridSpaceY( float Y ) noexcept { return double2int( Y / grid_height_v ); }


            //   +---+---+     | Grid-Space is a brick like layout of cells
            //   | 0 | 1 |     |
            // +-+-+-+-+-+-+   |
            // | 2 | 3 | 4 |   |
            // +-+-+-+-+-+-+   |
            //   | 5 | 6 |     |
            //   +---+---+     |
            struct grid_vector
            {
                int m_X, m_Y;
            };

            // +-+-+-+-+       | Imaginary Space (a cell here is half the width of a Grid-Space cell but the the same height)
            // |0|1|2|3|       |
            // +-+-+-+-+       |
            // |0|1|2|3|       |
            // +-+-+-+-+       |
            // |0|1|2|3|       |
            // +-+-+-+-+       |
            struct im_vector
            {
                int m_X, m_Y;

                xforceinline constexpr 
                im_vector operator - ( const im_vector& A ) const noexcept
                {
                    return { m_X - A.m_X, m_Y - A.m_Y };
                }

                xforceinline constexpr 
                im_vector operator + ( const im_vector& A ) const noexcept
                {
                    return { m_X + A.m_X, m_Y + A.m_Y };
                }

                xforceinline constexpr 
                xcore::vector2 getWorldPosition( void ) const noexcept
                {
                    xcore::vector2 V;
                    V.m_Y = static_cast<float>(grid_height_v * m_Y );
                    V.m_X = (m_Y&1) ? static_cast<float>( -grid_width_v/4 + (m_X + (m_X&1)) * 2 * grid_width_v/4 )
                                    : static_cast<float>(  grid_width_v/4 + (m_X - (m_X&1)) * 2 * grid_width_v/4 );
                    return V;
                }

                xforceinline 
                grid_vector getGridSpace( void ) const noexcept
                {
                    const auto x = 1&((m_X^m_Y)&m_Y);
                    xassert( auto xx = ((m_Y&1)? (1-(m_X&1)) : 0); x == xx );
                    return { (m_X - x)>>1, m_Y };
                }
            };

            xforceinline static
            im_vector ToImaginarySpace( xcore::vector2 A ) noexcept
            {
                return { WorldToImaginarySpaceX(A.m_X), WorldToImaginarySpaceY( A.m_Y ) };
            }

            xforceinline static
            grid_vector ToGridSpace( xcore::vector2 V ) noexcept
            {
                return ToImaginarySpace(V).getGridSpace();
            }

            struct box
            {
                im_vector m_Min;
                im_vector m_Max;
            };

            xforceinline static
            box MakeBox( const xcore::vector2& A, const xcore::vector2& B, const float Radius ) noexcept
            {
                const float d = (B - A).getLength() + Radius;
                return
                {
                      ToImaginarySpace( xcore::vector2{ A.m_X - d, A.m_Y - d } )
                    , ToImaginarySpace( xcore::vector2{ A.m_X + d, A.m_Y + d } )
                };
            }

            xforceinline constexpr static
            std::uint64_t ComputeKeyFromPosition( const grid_vector V ) noexcept
            {
                const auto x = (18446744073709551557ull ^ 9223372036854775ull) ^ (static_cast<std::uint64_t>(V.m_X)<<32) ^ static_cast<std::uint64_t>(V.m_Y);
                xassert(x);
                return x;
                //return xcore::bits::MurmurHash3( x );
            }

            xforceinline constexpr static
            std::uint64_t ComputeKeyFromPosition( const im_vector A ) noexcept
            {
                return ComputeKeyFromPosition( A.getGridSpace() );
            }

            xforceinline constexpr static
            std::uint64_t ComputeKeyFromPosition( const xcore::vector2 A ) noexcept
            {
                return ComputeKeyFromPosition( ToGridSpace(A) );
            }
        };

        using tools = tools_kit<32,32>;
    
        //----------------------------------------------------------------------------------------
        // WORLD GRID:: COMPONENTS::
        //----------------------------------------------------------------------------------------
        namespace component
        {
            using lists = mecs::examples::E01_graphical_2d_basic_physics::world_grid::component::lists;
            using count = mecs::examples::E01_graphical_2d_basic_physics::world_grid::component::count;
                
            struct id : tools::im_vector
            {
                constexpr static mecs::type_guid type_guid_v{"mecs::examples::E02_graphical_2d_basic_physics::world_grid::component::id"};
                using tools::im_vector::operator =;
            };
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
                    ,   const component::id&
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
                    const auto ID = Component.get< const component::id& >();
                    {
                        //XCORE_PERF_ZONE_SCOPED_N("CacheCells")
                        const auto p = [&]( const tools::im_vector Delta, const int i ) noexcept 
                        {
                            if( false == findEntity (  mecs::entity::guid{ CellGuids[i] = world_grid::tools::ComputeKeyFromPosition( ID + Delta ) }
                                , [&]( component::count& Count, const component::lists& T0Lists, component::lists& T1Lists ) noexcept
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

                        T0CellMap[3]            = &Component.get< const component::lists& >     ();
                        T1CellMap[3]            = &Component.get< component::lists& >           ();
                        CellMapCount[3]         = &Component.get< component::count& >           ();
                        CellGuids[3]            = world_grid::tools::ComputeKeyFromPosition( Component.get< const component::id& >() );

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
                        const auto&     T0Entry         = T0CellMap[3]->m_lEntry[i];
                        xcore::vector2  T1Velocity      = T0Entry.m_Velocity;
                        xcore::vector2  T1Position      = T0Entry.m_Position + T1Velocity * DT;
                        const auto      T0ImgPosition   = tools::ToImaginarySpace(T0Entry.m_Position);

                        xassert( auto xx = ((T0ImgPosition.m_Y&1)?(1-(T0ImgPosition.m_X&1)):(T0ImgPosition.m_X&1)); xx == (1&(T0ImgPosition.m_Y^T0ImgPosition.m_X)) );
                        const auto  TableOrigenOffsets  = tools::im_vector
                        {
                              1 + (1&(T0ImgPosition.m_Y^T0ImgPosition.m_X)) 
                            , 1
                        };

                        //
                        // Check for collisions with other objects
                        //
                        {
                            //XCORE_PERF_ZONE_SCOPED_N("CheckCollisions")
                            auto                    SearchBoxB = tools::MakeBox( T0Entry.m_Position, T1Position, T0Entry.m_Radius );
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
                                    auto& T0Other = T0CellMap[Index]->m_lEntry[c];
                                    if( &T0Entry == &T0Other ) continue;
                                    
                                    // Check for collision
                                    const auto V                    = (T0Other.m_Position - T1Position);
                                    const auto MinDistanceSquare    = xcore::math::Sqr(T0Other.m_Radius + T0Entry.m_Radius);
                                    const auto DistanceSquare       = V.Dot(V);
                                    if( DistanceSquare >= 0.001f && DistanceSquare <= MinDistanceSquare )
                                    {
                                        // Do hacky collision response
                                        const auto Normal           = V * -xcore::math::InvSqrt(DistanceSquare);
                                        T1Velocity = xcore::math::vector2::Reflect( Normal, T1Velocity );
                                        T1Position = T0Other.m_Position + Normal * (T0Other.m_Radius + T0Entry.m_Radius+0.01f);

                                        // Notify entities about the collision
                                        EventNotify<event::collision>( T0CellMap[3]->m_lEntity[i], T0CellMap[Index]->m_lEntity[c] );
                                    }
                                }
                            }
                        }

                        //
                        // Collide with the world
                        //
                        {
                            if( (T1Position.m_X - T0Entry.m_Radius) <= (tools::world_width_v/-2) )
                            {
                                T1Position.m_X = (tools::world_width_v/-2) + T0Entry.m_Radius;
                                T1Velocity.m_X = -T1Velocity.m_X;
                            }

                            if( (T1Position.m_Y - T0Entry.m_Radius) <= (tools::world_height_v/-2) )
                            {
                                T1Position.m_Y = (tools::world_height_v/-2) + T0Entry.m_Radius;
                                T1Velocity.m_Y = -T1Velocity.m_Y;
                            }

                            if( (T1Position.m_X + T0Entry.m_Radius) >= (tools::world_width_v/2) )
                            {
                                T1Position.m_X = (tools::world_width_v/2) - T0Entry.m_Radius;
                                T1Velocity.m_X = -T1Velocity.m_X;
                            }

                            if( (T1Position.m_Y + T0Entry.m_Radius) >= (tools::world_height_v/2) )
                            {
                                T1Position.m_Y = (tools::world_height_v/2) - T0Entry.m_Radius;
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
                            //XCORE_PERF_ZONE_SCOPED_N("MoveToT1")
                            if( CellMapCount[Index] == nullptr ) 
                                getOrCreatEntity
                                <
                                        component::lists
                                    ,   component::count
                                    ,   component::id
                                >(  mecs::entity::guid{ CellGuids[Index] }
                                    // Get entry
                                    , [&]( component::count& Count, component::lists& T1Lists ) noexcept
                                    {
                                        CellMapCount[Index]          = &Count;
                                        T1CellMap[Index]             = &T1Lists;
                                    }
                                    // Create entry
                                    ,   [&]( component::count& Count, component::id& ID, component::lists& T1Lists ) noexcept
                                    {
                                        xassert( T1ImgPosition.m_Y == tools::WorldToGridSpaceY(T1Position.m_Y) );
                                        ID = T1ImgPosition;

                                        CellMapCount[Index]  = &Count;
                                        T1CellMap[Index]     = &T1Lists;
                                    }
                                );

                            //
                            // Copy the entry in to the cell
                            //
                            xassert( T1ImgPosition.m_Y == tools::WorldToGridSpaceY(T1Position.m_Y) );
                                        C               = CellMapCount[Index]->m_MutableCount++;
                            auto&       Entry           = T1CellMap[Index]->m_lEntry[C];
                            Entry.m_Position            = T1Position;
                            Entry.m_Velocity            = T1Velocity;
                            Entry.m_Radius              = T0Entry.m_Radius;
                            T1CellMap[Index]->m_lEntity[C] = T0CellMap[3]->m_lEntity[i];
                        }

                        //
                        // Notify whoever cares about the info
                        //
                        EventNotify<event::render>( T0CellMap[3]->m_lEntity[i], T1Position, T1Velocity, T0Entry.m_Radius );
                    }

                    //
                    // Render grid
                    //
                    if(s_MyMenu.m_bRenderGrid)
                    {
                        const auto Position  = ID.getWorldPosition();
                        Draw2DQuad( Position
                        , xcore::vector2{ world_grid::tools::grid_width_half_v-1, world_grid::tools::grid_height_half_v -1}
                        , (ID.m_Y&1)?0x88ffffaa:0x88ffffff );
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
                        auto NewWorld   = ImgPosition.getWorldPosition();
                        auto ImgToG     = ImgPosition.getGridSpace();
                        xassert( world_grid::tools::ComputeKeyFromPosition( ImgPosition ) == world_grid::tools::ComputeKeyFromPosition( Position ) );
                    }

                    System.getOrCreatEntity
                    < 
                            component::lists
                        ,   component::count
                        ,   component::id
                    >
                    (
                        Guid
                        // Get entry
                        , [&]( component::lists& Lists, component::count& Count ) noexcept
                        {
                            const auto C = Count.m_MutableCount++;

                            auto& Entry = Lists.m_lEntry[C];
                            Entry.m_Position    = Position;
                            Entry.m_Velocity    = Velocity;
                            Entry.m_Radius      = Collider.m_Radius;
                            Lists.m_lEntity[C]  = Entity;
                        }
                        // Create Entry
                        , [&]( component::lists& Lists, component::count& Count, component::id& ID ) noexcept
                        {
                            ID = ImgPosition;

                            Count.m_MutableCount.store( 1, std::memory_order_relaxed );

                            auto& Entry = Lists.m_lEntry[0];
                            Entry.m_Position    = Position;
                            Entry.m_Velocity    = Velocity;
                            Entry.m_Radius      = Collider.m_Radius;
                            Lists.m_lEntity[0]  = Entity;
                        }
                    );
                }
            };

            //----------------------------------------------------------------------------------------
            // WORLD GRID:: DELEGATE:: DESTROY SPATIAL ENTITY
            //----------------------------------------------------------------------------------------
            struct destroy_spatial_entity : mecs::delegate::instance< mecs::event::destroy_entity >
            {
                constexpr static mecs::delegate::guid guid_v{ "world_grid::delegate::destroy_spatial_entity" };

                // This function will be call ones per entity. Since we only will have one entity it will only be call ones per frame.
                void operator() (         mecs::system::instance&           System
                                ,   const mecs::entity::instance&           Entity
                                ,   const example::component::position&     Position ) const noexcept
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
            World.registerComponents< component::lists, component::count, component::id >();
            World.registerDelegates< delegate::create_spatial_entity, delegate::destroy_spatial_entity >();
        }
    }

    //----------------------------------------------------------------------------------------
    // SYSTEMS::
    //----------------------------------------------------------------------------------------
    namespace system = mecs::examples::E01_graphical_2d_basic_physics::system;

    //----------------------------------------------------------------------------------------
    // DELEGATE::
    //----------------------------------------------------------------------------------------
    namespace delegate = mecs::examples::E01_graphical_2d_basic_physics::delegate;

    //-----------------------------------------------------------------------------------------
    // Test
    //-----------------------------------------------------------------------------------------

    void Test( bool(*Menu)( mecs::world::instance&,xcore::property::base&) )
    {
        printf( "--------------------------------------------------------------------------------\n");
        printf( "E02_graphical_2d_basic_physics\n");
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
        upWorld->registerComponents<component::position,component::collider,component::velocity>();

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
        auto& Group = upWorld->getOrCreateGroup<component::position,component::collider,component::velocity>();

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
            upWorld->Resume();
        }

    }
}
