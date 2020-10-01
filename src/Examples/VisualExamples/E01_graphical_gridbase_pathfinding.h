
namespace mecs::examples::E01_graphical_gridbase_pathfinding
{
    struct int2
    {
        int m_X, m_Y;
    };

    int2 operator + ( int2 A, int2 B )
    {
        return { A.m_X + B.m_X, A.m_Y + B.m_Y };
    }

    template< typename T, typename T_LAMBDA >
    bool BinarySearch( int& Where, const T& Container, T_LAMBDA&& Compare ) 
    {
        int l = 0;
        if( Container.size() )
        {
            int r = static_cast<int>(Container.size()) - 1;
            while (l <= r) 
            { 
                const int m = l + (r - l) / 2; 
          
                // Check if x is present at mid 
                switch( 1 + Compare( Container[m] ) )
                {
                    case  0:    l = m + 1; break;
                    case  1:    Where = m; return true;
                    case  2:    r = m - 1; break;
                    default:    xassume(false);
                }
            } 
        }

        // if we reach here, then element was not present
        // we will return where it should have been  
        Where = l;
        return false;
    }

    class path_find
    {
    public:

        enum class state : std::uint8_t
        {
                WIP
            ,   FAIL
            ,   SUCCESS
        };

    public:

        void Initialize( int W, int H, const int2 Start, const int2 End ) noexcept
        {
            m_GridWidth  = W;
            m_GridHeight = H;
            m_xpGrid.New( W * H ).CheckError();

            for( int i = 0, end = m_xpGrid.size<int>(); i<end ; ++i )
            {
                auto& Node = m_xpGrid[i];
                Node.m_Position.m_X = i%m_GridWidth;
                Node.m_Position.m_Y = i/m_GridHeight;
            }

            m_iEnd        = ComputeNodeIndex(End);

            const   auto    iStart      = ComputeNodeIndex(Start);
                    auto&   StartNode   = m_xpGrid[ iStart.m_Value ];

            StartNode.m_CurrentCost         = 0;
            StartNode.m_PathStepLength      = 0;
            StartNode.m_BestGuessTotalCost  = ComputeCostTo( StartNode.m_Position, End );

            StartNode.m_inOpenList = true;
            m_lOpen.append(iStart);
        }

        state find( int Steps = 10000 ) noexcept
        {
            for( int i=0; m_lOpen.size() && i<Steps; ++i )
            {
                const auto iOpenNode = m_lOpen.size()-1;
                const auto iCurNode  = m_lOpen[iOpenNode];
                if( iCurNode == m_iEnd )
                {
                    // Found the path....
                    break;
                }

                // get current node
                auto& CurrNode = m_xpGrid[iCurNode.m_Value];

                // Add it to the close list
                CurrNode.m_isClosed = true;

                // Remove node
                CurrNode.m_inOpenList = false;
                m_lOpen.eraseSwap( iOpenNode );

                constexpr static xcore::array offsets
                {
                      int2{ -1, -1 }
                    , int2{ +0, -1 }
                    , int2{ +1, -1 }
                    , int2{ -1, +0 }
                    , int2{ +1, +0 }
                    , int2{ -1, +1 }
                    , int2{ +0, +1 }
                    , int2{ +1, +1 }
                };

                for( auto& Offset : offsets )
                {
                    const auto NewPos = CurrNode.m_Position + Offset;
                    if( NewPos.m_X < 0 
                     || NewPos.m_Y < 0 
                     || NewPos.m_X >= m_GridWidth 
                     || NewPos.m_Y >= m_GridHeight ) continue;

                    const   auto    iNewNode    = ComputeNodeIndex( NewPos );
                            auto&   NewNode     = m_xpGrid[iNewNode.m_Value];
                    if( NewNode.m_isClosed ) continue;
                    if( NewNode.m_isWalkable == false ) continue;

                    auto TentativeCost = CurrNode.m_CurrentCost + ComputeCostTo( CurrNode.m_Position, NewPos );
                    if( TentativeCost < NewNode.m_CurrentCost )
                    {
                        NewNode.m_CameFromIndex         = iCurNode;
                        NewNode.m_CurrentCost           = TentativeCost;
                        NewNode.m_PathStepLength        = CurrNode.m_PathStepLength + 1;
                        NewNode.m_BestGuessTotalCost    = NewNode.m_CurrentCost + ComputeCostTo( NewNode.m_Position, m_xpGrid[m_iEnd.m_Value].m_Position );

                        if( NewNode.m_inOpenList == false )
                        {
                            NewNode.m_inOpenList = true;

                            // Insert base on its priority
                            int Index;
                            BinarySearch( Index, m_lOpen, [&]( const node_index& Element ) -> int
                            {
                                if( NewNode.m_BestGuessTotalCost < m_xpGrid[Element.m_Value].m_BestGuessTotalCost ) return -1;
                                return NewNode.m_BestGuessTotalCost > m_xpGrid[Element.m_Value].m_BestGuessTotalCost;
                            });
                            
                            m_lOpen.insert( Index ) = iNewNode;
                        }
                    }
                }
            };

            //
            // Handle the state
            //
            if( m_xpGrid[m_iEnd.m_Value].m_CameFromIndex == empty_node_index_v )
            {
                if( m_lOpen.size() )
                    return state::WIP;

                // did not found a path
                return state::FAIL;
            }

            CalculatePath( m_iEnd );
            return state::SUCCESS;
        }

    protected:

        using node_index = xcore::units::type<int, int>;
        constexpr static auto empty_node_index_v = node_index{-1};

        struct pathnode
        {
            int2                            m_Position{};
            float                           m_CurrentCost{ std::numeric_limits<float>::max() };
            float                           m_BestGuessTotalCost{};

            node_index                      m_CameFromIndex = empty_node_index_v;
            int                             m_PathStepLength{};

            bool                            m_isWalkable    = true;
            bool                            m_inOpenList    = false;
            bool                            m_isClosed      = false;

           // pathnode(void) {}
        };

    protected:

        static float ComputeCostTo( int2 Start, int2 End ) noexcept
        {
            constexpr static int move_straight_cost = 10;
            constexpr static int move_diagonal_cost = 14;
            const auto x = std::abs(Start.m_X - End.m_X);
            const auto y = std::abs(Start.m_Y - End.m_Y);
            const auto d = std::abs( x - y );
            return static_cast<float>(move_diagonal_cost * std::min( x, y ) + move_straight_cost * d);
        }

        void CalculatePath( node_index iEnd ) noexcept
        {
            m_FinalPath.clear();
            m_FinalPath.New(m_xpGrid[iEnd.m_Value].m_PathStepLength + 1).CheckError();
            for( auto I = iEnd; true; I = m_xpGrid[I.m_Value].m_CameFromIndex )
            {
                m_FinalPath[ m_xpGrid[I.m_Value].m_PathStepLength ] = m_xpGrid[I.m_Value].m_Position;
                if( m_xpGrid[I.m_Value].m_CameFromIndex == empty_node_index_v ) break;
            }
        }

        constexpr 
        node_index ComputeNodeIndex( int2 Pos ) const noexcept
        {
            xassert( Pos.m_X >=0 );
            xassert( Pos.m_Y >=0 );
            xassert( Pos.m_X < m_GridWidth );
            xassert( Pos.m_Y < m_GridHeight );
            return node_index{ Pos.m_X + Pos.m_Y * m_GridWidth };
        }

    protected:

        xcore::unique_span<int2>        m_FinalPath     {};
        node_index                      m_iEnd          {};
        xcore::vector<node_index>       m_lOpen         {};
        xcore::unique_span<pathnode>    m_xpGrid        {};
        int                             m_GridWidth     {};
        int                             m_GridHeight    {};
    };



    void Test()
    {
        path_find PathFind;
        PathFind.Initialize( 4, 4, int2{0,0}, int2{3,1} );
        PathFind.find();

        int a = 22;
    }
}