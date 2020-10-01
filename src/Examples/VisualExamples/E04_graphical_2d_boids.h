
namespace mecs::examples::E04_graphical_2d_boids
{
    namespace example = mecs::examples::E04_graphical_2d_boids;

    //-----------------------------------------------------------------------------------------
    // Example Menu
    //-----------------------------------------------------------------------------------------
    struct my_menu : xcore::property::base
    {
        int     m_EntitieCount;
        bool    m_bRenderGrid;
        bool    m_bColorizeGroups;
        float   m_Separacion;
        float   m_Aligment;
        int     m_ThinkEveryNSteps;
        int     m_CollectNeighbors;

        my_menu() { reset(); }
        void reset()
        {
            m_EntitieCount      = 180000 XCORE_CMD_DEBUG( / 100 );
            m_bRenderGrid       = false;
            m_Separacion        = 30.0f;
            m_Aligment          = 140.0f;
            m_ThinkEveryNSteps  = 3;
            m_CollectNeighbors  = 3;
            m_bColorizeGroups   = false;
        }

        property_vtable()
    };
    static my_menu s_MyMenu;

    //----------------------------------------------------------------------------------------
    // COMPONENTS::
    //----------------------------------------------------------------------------------------
    namespace component
    {
        using position = mecs::examples::E03_graphical_2d_basic_physics::component::position;
        using collider = mecs::examples::E03_graphical_2d_basic_physics::component::collider;

        struct dynamics
        {
            constexpr static mecs::type_guid    type_guid_v {"mecs::examples::E04_graphical_2d_boids::component::dynamics"};
            constexpr static auto               name_v      = xconst_universal_str("dynamics");
            xcore::vector2                      m_Velocity;
            xcore::vector2                      m_Force;
        };

        struct rigid_body 
        {
            constexpr static mecs::type_guid    type_guid_v {"mecs::examples::E04_graphical_2d_boids::component::rigid_body"};
            constexpr static auto               name_v      = xconst_universal_str("rigid_body");
            float                               m_Mass;
        };
    }

    //----------------------------------------------------------------------------------------
    // WORLD GRID::
    //----------------------------------------------------------------------------------------
    namespace world_grid
    {
        //----------------------------------------------------------------------------------------
        // WORLD GRID:: TOOLS::
        //----------------------------------------------------------------------------------------
        using tools = mecs::examples::E02_graphical_2d_basic_physics::world_grid::tools_kit<32,32>;
        struct vel
        {
            std::int16_t m_X, m_Y;  // fixed point (1)6:9
        };

        //----------------------------------------------------------------------------------------
        // WORLD GRID:: QUERY::
        //----------------------------------------------------------------------------------------
        namespace spatial_query
        {
            struct params;

            struct base
            {
                virtual void Check( 
                                    float                           DistanceSquare
                                  , const mecs::entity::instance&   Entity
                                  , const xcore::vector2&           Position
                                  , const vel&                      Velocity
                                  , float                           Radius ) noexcept = 0;
            };
            struct none final  {};
            struct sorted_distance final : base
            {
                constexpr sorted_distance(
                            const params&                   Params
                          , const xcore::vector2&           Position
                          , float                           Radius ) noexcept : m_Params{Params}, m_Position{Position}, m_Radius{Radius}{}

                void Check( float                           DistanceSquare
                          , const mecs::entity::instance&   Entity
                          , const xcore::vector2&           Position
                          , const vel&                      Velocity
                          , float                           Radius ) noexcept override;

                struct details
                {
                    mecs::entity::instance  m_Entity;
                    xcore::vector2          m_Position;
                    xcore::vector2          m_Velocity;
                    float                   m_DistanceSquare;
                };

                using qlist = xcore::array<details, 8>;
                const params&               m_Params;
                const xcore::vector2&       m_Position;
                float                       m_Radius;
                qlist                       m_lEntities;
                std::uint8_t                m_nAllocated{0};
            };

            using the_query = std::variant<none,sorted_distance>;

            enum class type : std::uint8_t
            {
                NONE            = static_cast<std::uint8_t>(xcore::types::variant_t2i_v< none, the_query >)
            ,   SORTED_DISTANCE = static_cast<std::uint8_t>(xcore::types::variant_t2i_v< sorted_distance, the_query >)
            };

            struct params
            {
                type            m_Type      = type::NONE;
                std::uint8_t    m_MaxCount  = 0;
            };

            inline
            void sorted_distance::Check( 
                        float                           DistanceSquare
                      , const mecs::entity::instance&   Entity
                      , const xcore::vector2&           Position
                      , const vel&                      Velocity
                      , float                           Radius ) noexcept
            {
                // Do we have anything allocated so far?
                if( m_nAllocated )
                {
                    // If we have all the entities we need and the distance is too far we can not add it
                    if( m_nAllocated < static_cast<std::uint8_t>(m_Params.m_MaxCount) || DistanceSquare < m_lEntities[m_nAllocated-1].m_DistanceSquare )
                    {
                        // We already have entries and this entry should be inserted somewhere
                        for( int q = m_nAllocated-1; q>=0; --q )
                        {
                            if( DistanceSquare > m_lEntities[q].m_DistanceSquare )
                            {
                                q++;
                                if( q < m_nAllocated && q < (m_Params.m_MaxCount-1) )
                                {
                                    const auto count = (m_nAllocated==m_Params.m_MaxCount?(m_nAllocated-1):m_nAllocated);
                                    memmove(    &m_lEntities[q+1]
                                           // ,   sizeof(details)*(m_Params.m_MaxCount - (q+1))
                                            ,   &m_lEntities[q]
                                            ,   sizeof(details) * (count - q) );
                                }

                                if( m_nAllocated < m_Params.m_MaxCount ) m_nAllocated++;
                                m_lEntities[q].m_DistanceSquare = DistanceSquare;
                                m_lEntities[q].m_Entity         = Entity;
                                m_lEntities[q].m_Position       = Position;
                                m_lEntities[q].m_Velocity       = xcore::vector2{ Velocity.m_X * (1.0f/(1<<9))
                                                                                , Velocity.m_Y * (1.0f/(1<<9)) };
                                return;
                            }
                        }

                        // Added entry at location 0
                        memmove( &m_lEntities[1], &m_lEntities[0], sizeof(details) * (m_nAllocated==m_Params.m_MaxCount?(m_nAllocated-1):m_nAllocated) );

                        if( m_nAllocated < m_Params.m_MaxCount ) m_nAllocated++;
                        m_lEntities[0].m_DistanceSquare = DistanceSquare;
                        m_lEntities[0].m_Entity         = Entity;
                        m_lEntities[0].m_Position       = Position;
                        m_lEntities[0].m_Velocity       = xcore::vector2{ Velocity.m_X * (1.0f/(1<<9))
                                                                        , Velocity.m_Y * (1.0f/(1<<9)) };
                    }
                }
                else
                {
                    // We are inserting the first entry
                    m_lEntities[0].m_DistanceSquare = DistanceSquare;
                    m_lEntities[0].m_Entity   = Entity;
                    m_lEntities[0].m_Position = Position;
                    m_lEntities[0].m_Velocity = xcore::vector2{ Velocity.m_X * (1.0f/(1<<9))
                                                              , Velocity.m_Y * (1.0f/(1<<9)) };
                    m_nAllocated=1;
                }
            };
        }

        //----------------------------------------------------------------------------------------
        // WORLD GRID:: EVENTS::
        //----------------------------------------------------------------------------------------
        namespace system { struct advance_cell; }
        namespace event
        {
            struct collision : mecs::examples::E03_graphical_2d_basic_physics::world_grid::event::collision
            {
                using                       system_t        = system::advance_cell;
            };

            struct query : mecs::event::instance
            {
                using                       event_t         = mecs::event::type <   mecs::system::instance&
                                                                                ,   const mecs::entity::instance&
                                                                                ,   const spatial_query::the_query&
                                                                                ,   xcore::vector2&                     // Force
                                                                                ,   const xcore::vector2&               // position
                                                                                ,   const xcore::vector2&               // velocity
                                                                                >;
                constexpr static auto       guid_v          = mecs::event::guid     { "world_grid::event::query" };
                constexpr static auto       name_v          = xconst_universal_str  ( "world_grid::event::query" );
                using                       system_t        = system::advance_cell;
            };

            struct render : mecs::event::instance
            {
                using                       event_t         = mecs::event::type< mecs::system::instance&        // System
                                                                                , const mecs::entity::instance& // Entity
                                                                                , const xcore::vector2&         // Position
                                                                                , const xcore::vector2&         // Velocity
                                                                                , float                         // Radius
                                                                                , spatial_query::params& >;     // Query Parameters
                constexpr static auto       guid_v          = mecs::event::guid     { "world_grid::event::render" };
                constexpr static auto       name_v          = xconst_universal_str  ( "world_grid::event::render" );
                using                       system_t        = system::advance_cell;
            };
        }
    
        //----------------------------------------------------------------------------------------
        // WORLD GRID:: COMPONENTS::
        //----------------------------------------------------------------------------------------
        namespace component
        {
            constexpr static std::size_t max_entity_count =64;
            struct lists : mecs::component::quantum_double_buffer
            {
                constexpr static mecs::type_guid type_guid_v{"mecs::examples::E04_graphical_2d_boids::world_grid::component::lists"};
                constexpr static auto name_v = xconst_universal_str("lists");

                std::array< mecs::entity::instance, max_entity_count >  m_lEntity;
                std::array< xcore::vector2,         max_entity_count >  m_lPosition;
                std::array< vel,                    max_entity_count >  m_lVelocity;        // fixed point 6:9
                std::array< std::uint16_t,          max_entity_count >  m_lRadius;          // fixed point 8:8
                std::array< std::uint16_t,          max_entity_count >  m_lMass;            // fixed point 8:8
                std::array< vel,                    max_entity_count >  m_lForce;           // fixed point 6:9
                std::array< spatial_query::params,  max_entity_count >  m_lQuery;          
            };

            using count = mecs::examples::E03_graphical_2d_basic_physics::world_grid::component::count;
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
                constexpr static auto   entities_per_job_v  = 35;

                using mecs::system::instance::instance;

                using access_t = mecs::query::access
                <       const component::lists&
                    ,         component::lists&
                    ,         component::count&
                >;

                using events_t = std::tuple
                <
                        event::collision
                    ,   event::query
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
                            if( false == findEntityRelax(  mecs::entity::guid{ CellGuids[i] = world_grid::tools::ComputeKeyFromPosition( ID + Delta ) }
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
                              auto      Force           = xcore::vector2{   T0Cell.m_lForce[i].m_X     * 1.0f / static_cast<float>(1<<9)
                                                                        ,   T0Cell.m_lForce[i].m_Y     * 1.0f / static_cast<float>(1<<9) };
                        const auto      Radius          =                   T0Cell.m_lRadius[i]        * 1.0f / static_cast<float>(1<<8);
                        const auto      Acceleration    = Force /       (   T0Cell.m_lMass[i]          * 1.0f / static_cast<float>(1<<8) );
                        const auto      T0Velocity      = xcore::vector2{   T0Cell.m_lVelocity[i].m_X  * 1.0f / static_cast<float>(1<<9)
                                                                        ,   T0Cell.m_lVelocity[i].m_Y  * 1.0f / static_cast<float>(1<<9) };
                        xcore::vector2  T1Velocity      = T0Velocity + Acceleration * DT;
                        xcore::vector2  T1Position      = T0Cell.m_lPosition[i] + T1Velocity * DT;
                        const auto      T0ImgPosition   = tools::ToImaginarySpace(T0Cell.m_lPosition[i]);
                        const auto      TableOffsets    = tools::im_vector
                        {
                              1 + (1&(T0ImgPosition.m_Y^T0ImgPosition.m_X)) 
                            , 1
                        };

                        //
                        // Check for collisions with other objects
                        //
                        {
                            //XCORE_PERF_ZONE_SCOPED_N("CheckCollisions")

                            //
                            // Make sure to map from the SearchBox to our imaginary box
                            //
                            if( T0Cell.m_lQuery[i].m_Type != spatial_query::type::NONE )
                            {
                                int iCache = -1;

                                spatial_query::the_query TheQuery;
                                spatial_query::base&     QueryInstance = [&]() constexpr noexcept ->spatial_query::base&
                                {
                                    switch( T0Cell.m_lQuery[i].m_Type )
                                    {
                                        default:                                    xassume(false);
                                        case spatial_query::type::SORTED_DISTANCE:  return TheQuery.emplace<spatial_query::sorted_distance>
                                                                                    (
                                                                                          T0Cell.m_lQuery[i]
                                                                                        , T1Position
                                                                                        , Radius
                                                                                    );
                                    }
                                }();

                                for( int y = -1; y<=1; y++ )
                                for( int x = -1; x<=1; x++ )
                                {
                                    const auto  TableRelativeImgPos = tools::im_vector{ x, y } + TableOffsets;
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
                                        xassert( T0Other.m_lEntity[c].getGuid().isValid() );
                                        if( T0Cell.m_lEntity[i].m_pInstance == T0Other.m_lEntity[c].m_pInstance ) continue;

                                        // Check for collision
                                        const auto  V                   = (T0Other.m_lPosition[c] - T1Position);
                                        const auto  OtherR              = T0Other.m_lRadius[c] * 1.0f / static_cast<float>(1<<8);
                                        const auto  MinDistanceSquare   = xcore::math::Sqr( OtherR + Radius);
                                        const auto  DistanceSquare      = V.Dot(V);

                                        QueryInstance.Check(  DistanceSquare
                                                           ,  T0Other.m_lEntity[c]
                                                           ,  T0Other.m_lPosition[c]
                                                           ,  T0Other.m_lVelocity[c]
                                                           ,  OtherR );

                                        if( DistanceSquare <= MinDistanceSquare )
                                        {
                                            if( DistanceSquare >= 0.001f )
                                            {
                                                // Do hacky collision response
                                                const auto Normal           = V * -xcore::math::InvSqrt(DistanceSquare);
                                                T1Velocity = xcore::math::vector2::Reflect( Normal, T1Velocity );
                                                T1Position = T0Other.m_lPosition[c] + Normal * (OtherR + Radius+0.01f);
                                            }

                                            // Notify entities about the collision
                                            EventNotify<event::collision>( T0Cell.m_lEntity[i], T0Other.m_lEntity[c] );
                                        }
                                    }
                                }

                                // Notify subscribers 
                                EventNotify<event::query>( T0Cell.m_lEntity[i], TheQuery, Force, T1Position, T1Velocity );
                            }
                            else
                            {
                                Force.m_X = Force.m_Y = 0;

                                int  iCache      = -1;
                                auto SearchBoxB  = tools::MakeBox( T0Cell.m_lPosition[i], T1Position, Radius );
                                SearchBoxB.m_Max = SearchBoxB.m_Max - T0ImgPosition;
                                SearchBoxB.m_Min = SearchBoxB.m_Min - T0ImgPosition;

                                for( int y = SearchBoxB.m_Min.m_Y; y<=SearchBoxB.m_Max.m_Y; y++ )
                                for( int x = SearchBoxB.m_Min.m_X; x<=SearchBoxB.m_Max.m_X; x++ )
                                {
                                    const auto  TableRelativeImgPos = tools::im_vector{ x, y } + TableOffsets;
                                    xassert( TableRelativeImgPos.m_X >= 0 );
                                    xassert( TableRelativeImgPos.m_X <= 4 );
                                    xassert( TableRelativeImgPos.m_Y >= 0 );
                                    xassert( TableRelativeImgPos.m_Y <= 2 );

                                    const auto Index = imgTable[ TableRelativeImgPos.m_X + TableRelativeImgPos.m_Y * 4 ];
                                    if( iCache == Index || T0CellMap[Index] == nullptr ) continue;
                                    iCache = Index;

                                    for( std::uint8_t Count = CellMapCount[Index]->m_ReadOnlyCount, c = 0; c < Count; ++c )
                                    {
                                        // Check for collision
                                        const auto& T0Other             = *T0CellMap[Index];
                                        const auto  V                   = (T0Other.m_lPosition[c] - T1Position);
                                        const auto  OtherR              = T0Other.m_lRadius[c] * 1.0f / static_cast<float>(1<<8);
                                        const auto  MinDistanceSquare   = xcore::math::Sqr(OtherR + Radius);
                                        const auto  DistanceSquare      = V.Dot(V);
                                        
                                        if( DistanceSquare <= MinDistanceSquare )
                                        {
                                            if( T0Cell.m_lEntity[i].m_pInstance == T0Other.m_lEntity[c].m_pInstance ) continue;
                                            if( DistanceSquare >= 0.001f )
                                            {
                                                // Do hacky collision response
                                                const auto Normal = V * -xcore::math::InvSqrt(DistanceSquare);
                                                T1Velocity = xcore::math::vector2::Reflect( Normal, T1Velocity );
                                                T1Position = T0Other.m_lPosition[c] + Normal * (OtherR + Radius+0.01f);
                                            }

                                            // Notify entities about the collision
                                            EventNotify<event::collision>( T0Cell.m_lEntity[i], T0Other.m_lEntity[c] );
                                        }
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
                        const auto  RelativeImgPos      = T1ImgPosition - T0ImgPosition + TableOffsets;
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
                                
                                getOrCreatEntityRelax
                                <
                                    component::lists
                                ,   component::count
                                >( mecs::entity::guid{ CellGuids[Index] }, CallBack, CallBack );
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
                            Entry.m_lForce[C].m_X       = static_cast<std::int16_t>(Force.m_X      * (1<<9));
                            Entry.m_lForce[C].m_Y       = static_cast<std::int16_t>(Force.m_Y      * (1<<9));
                            Entry.m_lMass[C]            = T0Cell.m_lMass[i];
                            Entry.m_lQuery[C]           = T0Cell.m_lQuery[i];
                            xassert( Entry.m_lEntity[C].getGuid().isValid() );

                            //
                            // Notify whoever cares about the info
                            //
                            EventNotify<event::render>( Entry.m_lEntity[C], T1Position, T1Velocity, Radius, Entry.m_lQuery[C] );
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
                        , xcore::vector2{ world_grid::tools::grid_width_half_v - 1, world_grid::tools::grid_height_half_v - 1 }
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
                constexpr static auto   entities_per_job_v  = 5000;

                using mecs::system::instance::instance;

                xforceinline
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
                    //
                    // Updates counts
                    //
                    Count.m_ReadOnlyCount = Count.m_MutableCount.load(std::memory_order_relaxed);
                    if( Count.m_ReadOnlyCount == 0 )
                    {
                        deleteEntity(Entity);
                    }
                    else
                    {
                        // Reset the count for the next iteration
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
                xforceinline
                void operator() (       mecs::system::instance&         System
                                , const mecs::entity::instance&         Entity
                                , const example::component::position&   Position
                                , const example::component::dynamics&   Dynamics
                                , const example::component::collider&   Collider
                                , const example::component::rigid_body&    Physics ) const noexcept
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
                        Lists.m_lPosition[C]            = Position;
                        Lists.m_lVelocity[C].m_X        = static_cast<std::int16_t>(Dynamics.m_Velocity.m_X * (1<<9));
                        Lists.m_lVelocity[C].m_Y        = static_cast<std::int16_t>(Dynamics.m_Velocity.m_Y * (1<<9));
                        Lists.m_lForce[C].m_X           = static_cast<std::int16_t>(Dynamics.m_Force.m_X    * (1<<9));
                        Lists.m_lForce[C].m_Y           = static_cast<std::int16_t>(Dynamics.m_Force.m_Y    * (1<<9));
                        Lists.m_lMass[C]                = static_cast<std::int16_t>(Physics.m_Mass          * (1<<8));
                        Lists.m_lRadius[C]              = static_cast<std::uint16_t>(Collider.m_Radius      * (1<<8));
                        Lists.m_lEntity[C]              = Entity;
                    };

                    System.getOrCreatEntity
                    < 
                        component::lists
                    ,   component::count
                    >( Guid, CallBack, CallBack );
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
    namespace system = mecs::examples::E03_graphical_2d_basic_physics::system;

    //----------------------------------------------------------------------------------------
    // DELEGATE::
    //----------------------------------------------------------------------------------------
    namespace delegate
    {
        using my_collision = mecs::examples::E03_graphical_2d_basic_physics::delegate::my_collision;

        struct my_render : mecs::delegate::instance< world_grid::event::render >
        {
            constexpr static mecs::delegate::guid guid_v{ "my_render" };

            xforceinline 
            void operator() ( mecs::system::instance&                       System
                            , const mecs::entity::instance&                 Entity
                            , const xcore::vector2&                         Position
                            , const xcore::vector2&
                            , float                                         R
                            , example::world_grid::spatial_query::params&   Params ) const noexcept
            {
                if( ((System.getTime().m_iFrame + (reinterpret_cast<std::uint64_t>(Entity.m_pInstance)>>4) ) & s_MyMenu.m_ThinkEveryNSteps) == s_MyMenu.m_ThinkEveryNSteps )
                {
                    Params.m_Type     = example::world_grid::spatial_query::type::SORTED_DISTANCE;
                    Params.m_MaxCount = s_MyMenu.m_CollectNeighbors;
                }
                else
                {
                    Params.m_Type  = example::world_grid::spatial_query::type::NONE;
                }

                if( s_MyMenu.m_bColorizeGroups )
                {
                    constexpr static std::array colorArray =       {0xffFF6633u, 0xffFFB399u, 0xffFF33FFu, 0xffFFFF99u, 0xff00B3E6u, 
                                                                    0xffE6B333u, 0xff3366E6u, 0xff999966u, 0xff99FF99u, 0xffB34D4Du,
                                                                    0xff80B300u, 0xff809900u, 0xffE6B3B3u, 0xff6680B3u, 0xff66991Au, 
                                                                    0xffFF99E6u, 0xffCCFF1Au, 0xffFF1A66u, 0xffE6331Au, 0xff33FFCCu,
                                                                    0xff66994Du, 0xffB366CCu, 0xff4D8000u, 0xffB33300u, 0xffCC80CCu, 
                                                                    0xff66664Du, 0xff991AFFu, 0xffE666FFu, 0xff4DB3FFu, 0xff1AB399u,
                                                                    0xffE666B3u, 0xff33991Au, 0xffCC9999u, 0xffB3B31Au, 0xff00E680u, 
                                                                    0xff4D8066u, 0xff809980u, 0xffE6FF80u, 0xff1AFF33u, 0xff999933u,
                                                                    0xffFF3380u, 0xffCCCC00u, 0xff66E64Du, 0xff4D80CCu, 0xff9900B3u, 
                                                                    0xffE64D66u, 0xff4DB380u, 0xffFF4D4Du, 0xff99E6E6u, 0xff6666FFu};

                    const auto Mask = (1<<xcore::bits::Log2IntRoundUp(s_MyMenu.m_ThinkEveryNSteps))-1;
                    const auto M    = (reinterpret_cast<std::uint64_t>(Entity.m_pInstance))>>4;
                    Draw2DQuad( Position, xcore::vector2{ R }, colorArray[4 + (M&Mask)] );
                }
                else
                {
                    Draw2DQuad( Position, xcore::vector2{ R }, ~0 );
                }
            }
        };

        struct my_query : mecs::delegate::instance< world_grid::event::query >
        {
            constexpr static mecs::delegate::guid guid_v{ "my_query" };

            xforceinline
            void operator() ( mecs::system::instance&                               
                            , const mecs::entity::instance&                         
                            , const example::world_grid::spatial_query::the_query&  TheQuery
                            , xcore::vector2&                                       Force
                            , const xcore::vector2&                                 Position
                            , const xcore::vector2&                                 Velocity
                            ) const noexcept
            {
                auto& Query = std::get<example::world_grid::spatial_query::sorted_distance>(TheQuery);
                if( Query.m_nAllocated == 0 ) return;

                //
                // Align Force
                //
                xcore::vector2 AlignmentForce(0);
                xcore::vector2 CohesionForce(0);
                xcore::vector2 SeparationForce(0);
                for( int i=0; i<Query.m_nAllocated; ++i )
                {
                    AlignmentForce  += Query.m_lEntities[i].m_Velocity;
                    CohesionForce   += Query.m_lEntities[i].m_Position;
                    SeparationForce += (Position - Query.m_lEntities[i].m_Position);
                }
                const auto InvNAllocated = (1.0f/Query.m_nAllocated);

                AlignmentForce = AlignmentForce * InvNAllocated;
                AlignmentForce.setLength( s_MyMenu.m_Aligment );
                AlignmentForce -= Velocity;

                CohesionForce  = CohesionForce * InvNAllocated;
                CohesionForce -= Position;
                CohesionForce -= Velocity;

                SeparationForce  = SeparationForce * InvNAllocated;
                AlignmentForce.setLength( s_MyMenu.m_Separacion );
                SeparationForce -= Velocity;

                Force = (AlignmentForce + CohesionForce + SeparationForce);
            }
        };
    }

    //-----------------------------------------------------------------------------------------
    // Test
    //-----------------------------------------------------------------------------------------

    void Test( bool(*Menu)(mecs::world::instance&,xcore::property::base&) )
    {
        printf( "--------------------------------------------------------------------------------\n");
        printf( "E04_graphical_2d_boids\n");
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
        upWorld->registerComponents
        <
            component::position
        ,   component::collider
        ,   component::dynamics
        ,   component::rigid_body
        >();

        //
        // Register Delegates
        //
        upWorld->registerDelegates<delegate::my_collision,delegate::my_render>();
        upWorld->registerDelegates<delegate::my_query>();

        //
        // Synpoint before the physics
        //
        mecs::sync_point::instance SyncPhysics;

        //
        // Register the game graph.
        // 
        upWorld->registerGraphConnection<world_grid::system::advance_cell>  ( upWorld->m_StartSyncPoint,    SyncPhysics );
        upWorld->registerGraphConnection<world_grid::system::reset_counts>  ( SyncPhysics,                  upWorld->m_EndSyncPoint );
        upWorld->registerGraphConnection<system::render_pageflip>           ( SyncPhysics,                  upWorld->m_EndSyncPoint ).m_Menu = [&] { return Menu(*upWorld, s_MyMenu); };

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
        auto& Group = upWorld->getOrCreateGroup
        <
            component::position
        ,   component::collider
        ,   component::dynamics
        ,   component::rigid_body
        >();

        //
        // Create Entities
        //
        xcore::random::small_generator Rnd;
        for( int i=0; i<s_MyMenu.m_EntitieCount; i++ )
            upWorld->createEntity( Group, [&](  component::position&    Position
                                            ,   component::dynamics&    Dynamics
                                            ,   component::collider&    Collider
                                            ,   component::rigid_body&     Physics )
            {

                Position.setup( Rnd.RandF32(-(world_grid::tools::world_width_v/2.0f), (world_grid::tools::world_width_v/2.0f) )
                              , Rnd.RandF32(-(world_grid::tools::world_height_v/2.0f), (world_grid::tools::world_height_v/2.0f) ) );

                Dynamics.m_Velocity.setup( Rnd.RandF32(-1.0f, 1.0f ), Rnd.RandF32(-1.0f, 1.0f ) );
                Dynamics.m_Velocity.NormalizeSafe();
                Dynamics.m_Velocity *= 10.0f;

                Physics.m_Mass = 1.0f;
                Dynamics.m_Force.setZero();

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

property_begin_name( mecs::examples::E04_graphical_2d_boids::my_menu, "Settings" )
{
    property_var( m_EntitieCount )
        .Help(  "This specifies how many entities are going to get spawn.\n"
                "You must use the upper button call 'Use Settings' to \n"
                "activate it.")
,   property_var( m_bRenderGrid )
        .Help(  "Tells the system to render the spacial grid used for the entities.\n"
                "This spatial grid is used for the broad phase of the collision.\n"
                "This is done in real time.")
,   property_var( m_bColorizeGroups )
        .Help(  "Paints the entities different colors base on which frame each of them is thinking.\n"
                "This is done in real time.")
,   property_var_fnbegin( "Settings", string_t )
    {
        if( isRead ) xcore::string::Copy( InOut, xcore::string::constant{"Reset To Defaults"} );
        else         mecs::examples::E04_graphical_2d_boids::s_MyMenu.reset();
    }
    property_var_fnend()
        .EDStyle(property::edstyle<string_t>::Button())
        .Help(  "Will reset these settings menu back to the original values. \n"
                "Please note that to respawn the entities you need to use \n"
                "the 'Use Settings' button at the top.")
,   property_var( m_ThinkEveryNSteps )
        .EDStyle(property::edstyle<int>::ScrollBar( 0, 10 ))
        .Help(  "It will break all entities into groups and run the logic on\n"
                "each group every VALUE frames. So every group will be updated\n"
                "after VALUE frames. This should help reduce the work load.\n"
                "This option is done in real time.")
,   property_var( m_CollectNeighbors )
        .EDStyle(property::edstyle<int>::ScrollBar( 0
            , mecs::examples::E04_graphical_2d_boids::world_grid::spatial_query::sorted_distance::qlist::size<int>() ))
        .Help(  "Each time each entity runs its logic it will collect other\n"
                "Entities near it to determine its velocity, and direction.\n"
                "This value specifies how many entities is going to collect.\n"
                "This option is done in real time.")
,   property_var( m_Separacion )
        .Help(  "Specifies how much separation force is going to apply. The \n"
                "Separation force is a force that makes our entity go away from\n"
                "its neighbors.\n"
                "This option is done in real time.")
,   property_var( m_Aligment )
        .Help(  "Specifies how much force it should use to change its velocity to \n"
                "match its neighbors velocity and direction.\n"
                "This option is done in real time.")
}
property_vend_h(mecs::examples::E04_graphical_2d_boids::my_menu);
