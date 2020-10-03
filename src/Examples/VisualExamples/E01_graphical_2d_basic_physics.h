namespace mecs::examples::E01_graphical_2d_basic_physics
{
    namespace example = mecs::examples::E01_graphical_2d_basic_physics;

    //-----------------------------------------------------------------------------------------
    // Example Menu
    //-----------------------------------------------------------------------------------------
    struct my_menu : xcore::property::base
    {
        int     m_EntitieCount;
        bool    m_bRenderGrid;
             my_menu() { reset(); }
        void reset()
        {
            m_EntitieCount = 250000 XCORE_CMD_DEBUG( / 100 );
            m_bRenderGrid  = false;
        }

        property_vtable()
    };
    static my_menu s_MyMenu;

    //----------------------------------------------------------------------------------------
    // COMPONENTS::
    //----------------------------------------------------------------------------------------
    namespace component
    {
        struct position : mecs::component::data
        {
            xcore::vector2 m_Value;
        };

        struct velocity : mecs::component::data
        {
            xcore::vector2 m_Value;
        };

        struct collider : mecs::component::data
        {
            float m_Radius;
        };
    }

    //----------------------------------------------------------------------------------------
    // PHYSICS::
    //----------------------------------------------------------------------------------------
    namespace physics
    {
        //----------------------------------------------------------------------------------------
        // WORLD GRID:: EVENTS::
        //----------------------------------------------------------------------------------------
        namespace system { struct advance_cell; }
        namespace event
        {
            struct collision : mecs::system::event::exclusive
            {
                using                       system_t        = system::advance_cell;
                using                       real_event_t    = define_real_event< const mecs::component::entity&
                                                                               , const mecs::component::entity& >;
            };

            struct render : mecs::system::event::exclusive
            {
                using                       system_t        = system::advance_cell;
                using                       real_event_t    = define_real_event<  const mecs::component::entity&
                                                                                , const xcore::vector2&         // Position
                                                                                , const xcore::vector2&         // Velocity
                                                                                , float >;                      // Radius
            };
        }

        //----------------------------------------------------------------------------------------
        // WORLD GRID:: TOOLS::
        //----------------------------------------------------------------------------------------
        namespace tools
        {
            constexpr static int    grid_width_v        = 32;
            constexpr static int    grid_height_v       = 32;

            constexpr static int    grid_width_half_v   = grid_width_v/2;
            constexpr static int    grid_height_half_v  = grid_height_v/2;

            constexpr static float  world_width_v       = 5500.0f;
            constexpr static float  world_height_v      = 3200.0f; 
            
            struct vector2
            {
                int m_X, m_Y;

                vector2 operator - ( const vector2& A ) const noexcept
                {
                    return { m_X - A.m_X, m_Y - A.m_Y };
                }

                vector2 operator + ( const vector2& A ) const noexcept
                {
                    return { m_X + A.m_X, m_Y + A.m_Y };
                }
            };

            struct box
            {
                tools::vector2 m_Min;
                tools::vector2 m_Max;
            };

            // https://stackoverflow.com/questions/17035464/a-fast-method-to-round-a-double-to-a-32-bit-int-explained
            xforceinline
            int double2int( double d ) noexcept
            {
                d += 6755399441055744.0;
                return reinterpret_cast<int&>(d);
            }

            xforceinline 
            int WorldToGridX( const float X ) noexcept { return double2int(X / grid_width_v); }

            xforceinline
            int WorldToGridY( const float Y ) noexcept { return double2int(Y / grid_height_v); }

            xforceinline
            tools::vector2 WorldToGrid( xcore::vector2 V ) noexcept
            {
                return{ WorldToGridX(V.m_X), WorldToGridY(V.m_Y) };
            }

            xforceinline
            box SearchBox( const tools::vector2 Pos ) noexcept
            {
                return box{ {Pos.m_X-1, Pos.m_Y-1}, {Pos.m_X+1, Pos.m_Y+1} };
            }

            xforceinline
            box MakeBox( const xcore::vector2& A, const xcore::vector2& B, const float Radius ) noexcept
            {
                const float d = (B - A).getLength() + Radius;
                return
                {
                      WorldToGrid(xcore::vector2{ A.m_X - d, A.m_Y - d })
                    , WorldToGrid(xcore::vector2{ A.m_X + d, A.m_Y + d })
                };
            }

            xforceinline constexpr
            std::uint64_t ComputeKeyFromPosition( const int X, const int Y ) noexcept
            {
                const auto x = (18446744073709551557ull ^ 9223372036854775ull) ^ (static_cast<std::uint64_t>(X)<<32) ^ static_cast<std::uint64_t>(Y);
                xassert(x);
                return x;
                
            }

            xforceinline constexpr
            std::uint64_t ComputeKeyFromPosition( const vector2 A ) noexcept
            {
                return ComputeKeyFromPosition( A.m_X, A.m_Y );
            }
        }
    
        //----------------------------------------------------------------------------------------
        // WORLD GRID:: COMPONENTS::
        //----------------------------------------------------------------------------------------
        namespace component
        {
            constexpr static std::size_t max_entity_count = 64;

            struct lists : mecs::component::data 
            {
                static constexpr auto type_data_access_v = data::type_data_access::QUANTUM_DOUBLE_BUFFER;

                struct entry
                {
                    xcore::vector2  m_Position;
                    xcore::vector2  m_Velocity;
                    float           m_Radius;
                };

                std::array< entry,                      max_entity_count >  m_lEntry;
                std::array< mecs::component::entity,    max_entity_count >  m_lEntity;
            };

            struct count : mecs::component::data
            {
                static constexpr auto type_data_access_v = data::type_data_access::QUANTUM;

                std::atomic< std::uint8_t >                             m_MutableCount  { 0 };
                std::uint8_t                                            m_ReadOnlyCount { 0 };        // Note this variable is naked/unprotected and should only be written by a single system 
            };
                
            struct id : mecs::component::data
            {
                tools::vector2 m_Value;
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
                constexpr static auto   entities_per_job_v  = 50;

                using mecs::system::instance::instance;

                using events_t = std::tuple
                <
                        event::collision
                    ,   event::render
                >;

                mecs::archetype::specialized_pool* m_pSpecializedPoolCell;

                void msgGraphInit( mecs::world::instance& ) noexcept
                {
                    auto& Archetype = getOrCreateArchetype<   physics::component::lists
                                                          ,   physics::component::count
                                                          ,   physics::component::id >();

                    m_pSpecializedPoolCell = &Archetype.getOrCreateSpecializedPool(*this);
                }

                xforceinline
                void operator() ( const component::lists&   ComListT0
                                ,       component::lists&   ComListT1
                                , const component::id&      ID
                                ,       component::count&   ComCount 
                                ) noexcept
                {
                    std::array< std::array< const component::lists*,  3 >, 3 > T0CellMap    ;
                    std::array< std::array<       component::lists*,  3 >, 3 > T1CellMap    ;
                    std::array< std::array<       component::count*,  3 >, 3 > CellMapCount ;
                    std::array< std::array<       std::uint64_t,      3 >, 3 > CellGuids    ;

                    //
                    // Cache all relevant cells
                    //
                    const auto SearchBoxA       = tools::SearchBox( ID.m_Value );
                    {
                        //XCORE_PERF_ZONE_SCOPED_N("CacheCells")
                        T0CellMap[1][1]             = &ComListT0;
                        T1CellMap[1][1]             = &ComListT1;
                        CellMapCount[1][1]          = &ComCount;
                        CellGuids[1][1]             = physics::tools::ComputeKeyFromPosition(ID.m_Value);

                        for( int y = SearchBoxA.m_Min.m_Y; y<=SearchBoxA.m_Max.m_Y; ++y )
                        for( int x = SearchBoxA.m_Min.m_X; x<=SearchBoxA.m_Max.m_X; ++x )
                        {
                            if( x == ID.m_Value.m_X && y == ID.m_Value.m_Y ) continue;
                                
                            const auto RelativeGridPos  = tools::vector2{ 1 + x, 1 + y } - ID.m_Value;

                            if( false == findComponents( mecs::component::entity::guid{ CellGuids[RelativeGridPos.m_X][RelativeGridPos.m_Y] = physics::tools::ComputeKeyFromPosition(x,y) }
                                , [&]   ( component::count&         Count
                                        , const component::lists&   T0Lists
                                        , component::lists&         T1Lists ) noexcept
                                {
                                    T0CellMap[RelativeGridPos.m_X][RelativeGridPos.m_Y]     = &T0Lists;
                                    T1CellMap[RelativeGridPos.m_X][RelativeGridPos.m_Y]     = &T1Lists;
                                    CellMapCount[RelativeGridPos.m_X][RelativeGridPos.m_Y]  = &Count;
                                })
                            )
                            {
                                T0CellMap[RelativeGridPos.m_X][RelativeGridPos.m_Y]    = nullptr;
                                CellMapCount[RelativeGridPos.m_X][RelativeGridPos.m_Y] = nullptr;
                            }
                        }
                    }

                    //
                    // Advance physics for our cell
                    //
                    //XCORE_PERF_ZONE_SCOPED_N("AdvancePhysics")
                    const float DT      = getTime().m_DeltaTime;
                    for( std::uint8_t CountT = CellMapCount[1][1]->m_ReadOnlyCount, i=0; i<CountT; ++i )
                    {
                        //
                        // Integrate
                        //
                        auto           Entity     = T0CellMap[1][1]->m_lEntity[i];
                        const auto&    T0Entry    = T0CellMap[1][1]->m_lEntry[i];
                        xcore::vector2 T1Velocity = T0Entry.m_Velocity;
                        xcore::vector2 T1Position = T0Entry.m_Position + T1Velocity * DT;

                        //
                        // Check for collisions with other objects
                        //
                        {
                            //XCORE_PERF_ZONE_SCOPED_N("CheckCollisions")
                            const auto     SearchBoxB = tools::MakeBox( T0Entry.m_Position, T1Position, T0Entry.m_Radius );
                            for( int  y     = SearchBoxB.m_Min.m_Y; y<=SearchBoxB.m_Max.m_Y; y++ )
                            for( int  x     = SearchBoxB.m_Min.m_X; x<=SearchBoxB.m_Max.m_X; x++ )
                            {
                                const auto RelativeGridPos  = tools::vector2{ 1 + x, 1 + y } - ID.m_Value;

                                if( T0CellMap[RelativeGridPos.m_X][RelativeGridPos.m_Y] ) 
                                    for( std::uint8_t Count = CellMapCount[RelativeGridPos.m_X][RelativeGridPos.m_Y]->m_ReadOnlyCount, c = 0; c < Count; ++c )
                                    {
                                        xassert( T0CellMap[RelativeGridPos.m_X][RelativeGridPos.m_Y]->m_lEntity[c].getGUID().isValid() );
                                        auto& T0Other = T0CellMap[RelativeGridPos.m_X][RelativeGridPos.m_Y]->m_lEntry[c];
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
                                            EventNotify<event::collision>( T0CellMap[1][1]->m_lEntity[i], T0CellMap[RelativeGridPos.m_X][RelativeGridPos.m_Y]->m_lEntity[c] );
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
                        const auto WorldPosition    = tools::WorldToGrid( T1Position );
                        const auto RelativeGridPos  = tools::vector2{1,1} + (WorldPosition - ID.m_Value);
                        int C;
                        {
                            //XCORE_PERF_ZONE_SCOPED_N("MoveToT1")
                            if( CellMapCount[RelativeGridPos.m_X][RelativeGridPos.m_Y] == nullptr ) getOrCreateEntity
                            (  mecs::component::entity::guid{ CellGuids[RelativeGridPos.m_X][RelativeGridPos.m_Y] }
                                , *m_pSpecializedPoolCell
                                // Get entry
                                , [&]( component::count& Count, component::lists& T1Lists ) noexcept
                                {
                                    CellMapCount[RelativeGridPos.m_X][RelativeGridPos.m_Y]          = &Count;
                                    T1CellMap[RelativeGridPos.m_X][RelativeGridPos.m_Y]             = &T1Lists;
                                }
                                // Create entry
                                , [&]( component::count& Count, component::id& ID, component::lists& T1Lists ) noexcept
                                {
                                    ID.m_Value = WorldPosition;
                                    xassert( physics::tools::ComputeKeyFromPosition(ID.m_Value) == CellGuids[RelativeGridPos.m_X][RelativeGridPos.m_Y] );

                                    CellMapCount[RelativeGridPos.m_X][RelativeGridPos.m_Y]  = &Count;
                                    T1CellMap[RelativeGridPos.m_X][RelativeGridPos.m_Y]     = &T1Lists;
                                }
                            );

                            //
                            // Copy the entry in to the cell
                            //
                                        C               = CellMapCount[RelativeGridPos.m_X][RelativeGridPos.m_Y]->m_MutableCount++;
                            auto&       Entry           = T1CellMap[RelativeGridPos.m_X][RelativeGridPos.m_Y]->m_lEntry[C];
                            Entry.m_Position            = T1Position;
                            Entry.m_Velocity            = T1Velocity;
                            Entry.m_Radius              = T0Entry.m_Radius;
                            T1CellMap[RelativeGridPos.m_X][RelativeGridPos.m_Y]->m_lEntity[C] = Entity;
                        }

                        //
                        // Notify whoever cares about the info
                        //
                        EventNotify<event::render>( Entity, T1Position, T1Velocity, T0Entry.m_Radius );
                    }

                    //
                    // Render grid
                    //
                    if(s_MyMenu.m_bRenderGrid)
                    {
                        xcore::vector2 Position;
                        Position.m_X = static_cast<float>((physics::tools::grid_width_v  ) * ID.m_Value.m_X );
                        Position.m_Y = static_cast<float>((physics::tools::grid_height_v ) * ID.m_Value.m_Y );

                        Draw2DQuad( 
                          Position
                        , xcore::vector2{ physics::tools::grid_width_half_v -1, physics::tools::grid_height_half_v -1 }
                        , 0xaa0000aa );
                    }
                }
            };

            //----------------------------------------------------------------------------------------
            // WORLD GRID:: SYSTEM:: RESET COUNTS
            //----------------------------------------------------------------------------------------
            struct reset_counts : mecs::system::instance
            {
                constexpr static auto   entities_per_job_v  = 200;

                using mecs::system::instance::instance;

                // On the start of the game we are going to update all the Counts for the first frame.
                // Since entities have been created outside the system so our ReadOnlyCounter is set to zero.
                // But After this update we should be all up to date.
                void msgGraphInit ( world::instance& ) noexcept
                {
                    query Query;

                    DoQuery< reset_counts >( Query );

                    ForEach( Query, *this, entities_per_job_v );


                }

                // This system is reading T0 and it does not need to worry about people changing its value midway.
                xforceinline
                void operator() ( mecs::component::entity& Entity, component::count& Count ) noexcept
                {
                    Count.m_ReadOnlyCount = Count.m_MutableCount.load(std::memory_order_relaxed);
                    if( Count.m_ReadOnlyCount == 0 )
                    {
                    //    deleteEntity(Entity);
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
                    const auto              GridPosition   = tools::WorldToGrid(Position.m_Value);
                    const entity::guid      Guid           { physics::tools::ComputeKeyFromPosition( GridPosition ) };

                    if( m_pSpecializedPoolCell == nullptr )
                    {
                        auto& Archetype = System.getOrCreateArchetype< physics::component::lists
                                                                     , physics::component::count
                                                                     , physics::component::id >();

                        m_pSpecializedPoolCell = &Archetype.getOrCreateSpecializedPool(System);
                    }

                    System.getOrCreateEntity
                    (
                        Guid
                        , *m_pSpecializedPoolCell
                        // Get entry
                        , [&]( component::lists& Lists, component::count& Count ) noexcept
                        {
                            const auto C = Count.m_MutableCount++;

                            auto& Entry = Lists.m_lEntry[C];
                            Entry.m_Position    = Position.m_Value;
                            Entry.m_Velocity    = Velocity.m_Value;
                            Entry.m_Radius      = Collider.m_Radius;
                            Lists.m_lEntity[C]  = Entity;
                        }
                        // Create Entry
                        , [&]( component::lists& Lists, component::count& Count, component::id& ID ) noexcept
                        {
                            ID.m_Value = GridPosition;

                            Count.m_MutableCount.store( 1, std::memory_order_relaxed );

                            auto& Entry = Lists.m_lEntry[0];
                            Entry.m_Position    = Position.m_Value;
                            Entry.m_Velocity    = Velocity.m_Value;
                            Entry.m_Radius      = Collider.m_Radius;
                            Lists.m_lEntity[0]  = Entity;
                        }
                    );
                }
            };

            //----------------------------------------------------------------------------------------
            // WORLD GRID:: DELEGATE:: DESTROY SPATIAL ENTITY
            //----------------------------------------------------------------------------------------
            struct destroy_spatial_entity : mecs::archetype::delegate::instance< mecs::archetype::event::destroy_entity >
            {
                // This function will be call ones per entity. Since we only will have one entity it will only be call ones per frame.
                void operator() (         mecs::system::instance&       System
                                ,   const mecs::component::entity&      Entity
                                ,   const example::component::position& Position ) const noexcept
                {
                    /*
                    const auto               WorldPosition = tools::WorldToGrid(Position);
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
            Universe.registerTypes< component::lists, component::count, component::id >();
            Universe.registerTypes< delegate::create_spatial_entity, delegate::destroy_spatial_entity >();
        }
    }

    //----------------------------------------------------------------------------------------
    // SYSTEMS::
    //----------------------------------------------------------------------------------------
    namespace system
    {
        //----------------------------------------------------------------------------------------
        // SYSTEM:: RENDER_OBJECTS
        //----------------------------------------------------------------------------------------
        struct render_objects : mecs::system::instance
        {
            constexpr static auto   entities_per_job_v  = 1000;
            using mecs::system::instance::instance;

            xforceinline
            void operator() ( const component::position& Position, const component::collider& Collider ) noexcept
            {
                Draw2DQuad( Position.m_Value, xcore::vector2{ Collider.m_Radius }, ~0 );
            }
        };

        //----------------------------------------------------------------------------------------
        // SYSTEM:: RENDER_PAGEFLIP
        //----------------------------------------------------------------------------------------
        struct render_pageflip : mecs::system::instance
        {
            render_pageflip(instance::construct&& Construct)
                : mecs::system::instance{ [&] { auto C = Construct; C.m_JobDefinition.m_Priority = xcore::scheduler::priority::ABOVE_NORMAL; return C; }() }
            {
                s_bContinue = true;
            }

            inline
            void msgUpdate( void ) noexcept
            {
                s_bContinue = m_Menu();
                PageFlip();
            }

            xcore::func<bool(void)> m_Menu;
            static inline bool s_bContinue;
        };
    }

    //----------------------------------------------------------------------------------------
    // DELEGATE::
    //----------------------------------------------------------------------------------------
    namespace delegate
    {
        struct my_collision : mecs::system::delegate::instance< physics::event::collision >
        {
            xforceinline 
            void operator() ( system& System, const entity& E1, const entity& E2 ) const noexcept
            {
                //printf( ">>>>>>>>>>Collision \n");
            }
        };

        struct my_render : mecs::system::delegate::instance< physics::event::render >
        {
            xforceinline 
            void operator() ( mecs::system::instance&, const entity&, const xcore::vector2& Position, const xcore::vector2&, float R ) const noexcept
            {
                Draw2DQuad( Position, xcore::vector2{ R }, ~0 );
            }
        };

    }

    //-----------------------------------------------------------------------------------------
    // Test
    //-----------------------------------------------------------------------------------------
    inline
    void Test( bool(*Menu)(mecs::world::instance&,xcore::property::base& ) )
    {
        printf( "--------------------------------------------------------------------------------\n");
        printf( "E01_graphical_2d_basic_physics\n");
        printf( "--------------------------------------------------------------------------------\n");

        auto upUniverse = std::make_unique<mecs::universe::instance>();
        upUniverse->Init();

        //------------------------------------------------------------------------------------------
        // Registration
        //------------------------------------------------------------------------------------------

        // Import that world_grid system
        physics::ImportModule( *upUniverse );

        //
        // Register Components
        //
        upUniverse->registerTypes<component::position,component::collider,component::velocity>();

        //
        // Register the game graph.
        //
        auto& DefaultWorld  = *upUniverse->m_WorldDB[0];
        auto& SyncPhysics   = DefaultWorld.m_GraphDB.CreateSyncPoint();
        auto& System        = DefaultWorld.m_GraphDB.CreateGraphConnection<physics::system::advance_cell>  ( DefaultWorld.m_GraphDB.m_StartSyncPoint, SyncPhysics             );
                              DefaultWorld.m_GraphDB.CreateGraphConnection<physics::system::reset_counts>  ( SyncPhysics,                             DefaultWorld.m_GraphDB.m_EndSyncPoint);
                              DefaultWorld.m_GraphDB.CreateGraphConnection<system::render_pageflip>        ( SyncPhysics,                             DefaultWorld.m_GraphDB.m_EndSyncPoint).m_Menu = [&] { return Menu(DefaultWorld, s_MyMenu); };

        //
        // Create the delegates.
        //
        DefaultWorld.m_GraphDB.CreateArchetypeDelegate< physics::delegate::create_spatial_entity >();
        DefaultWorld.m_GraphDB.CreateArchetypeDelegate< physics::delegate::destroy_spatial_entity >();

        DefaultWorld.m_GraphDB.CreateSystemDelegate< delegate::my_render    >();

        //------------------------------------------------------------------------------------------
        // Initialization
        //------------------------------------------------------------------------------------------

        //
        // Create an entity group
        //
        auto& Archetype = DefaultWorld.m_ArchetypeDB.getOrCreateArchitype<component::position,component::collider,component::velocity>();

        //
        // Create Entities
        //
        xcore::random::small_generator Rnd;
        Archetype.CreateEntities(System, 1 + 0*s_MyMenu.m_EntitieCount, {} )
            ([&](   component::position&  Position
                ,   component::velocity&  Velocity
                ,   component::collider&  Collider )
            {
                Position.m_Value.setup( Rnd.RandF32(-(physics::tools::world_width_v/2.0f), (physics::tools::world_width_v/2.0f) )
                              , Rnd.RandF32(-(physics::tools::world_height_v/2.0f), (physics::tools::world_height_v/2.0f) ) );
                
                Velocity.m_Value.setup( Rnd.RandF32(-1.0f, 1.0f ), Rnd.RandF32(-1.0f, 1.0f ) );
                Velocity.m_Value.NormalizeSafe();
                Velocity.m_Value *= 10.0f;

                Collider.m_Radius = Rnd.RandF32( 1.5f, 3.0f );
            });

        //------------------------------------------------------------------------------------------
        // Running
        //------------------------------------------------------------------------------------------

        //
        // Start executing the world
        //
        DefaultWorld.Start();

        //
        // run 100 frames
        //
        while (system::render_pageflip::s_bContinue)
        {
            DefaultWorld.Resume();
        }
    }
}

property_begin_name( mecs::examples::E01_graphical_2d_basic_physics::my_menu, "Settings" )
{
    property_var( m_EntitieCount )
        .Help(  "This specifies how many entities are going to get spawn.\n"
                "You must use the upper button call 'Use Settings' to \n"
                "activate it.")
,   property_var( m_bRenderGrid )
        .Help(  "Tells the system to render the spatial grid used for the entities.\n"
                "This spatial grid is used for the broad phase of the collision.\n"
                "This is done in real time.")
,   property_var_fnbegin( "Settings", string_t )
    {
        if( isRead ) xcore::string::Copy( InOut, "Reset To Defaults" );
        else         mecs::examples::E01_graphical_2d_basic_physics::s_MyMenu.reset();
    }
    property_var_fnend()
        .EDStyle(property::edstyle<string_t>::Button())
        .Help(  "Will reset these settings menu back to the original values. \n"
                "Please note that to respawn the entities you need to use \n"
                "the 'Use Settings' button at the top.")
}
property_vend_h(mecs::examples::E01_graphical_2d_basic_physics::my_menu);

