namespace mecs::examples::E12_hierarchy_components
{
    //
    // Describing a hierarchy in MECS
    //
    struct parent : mecs::component::share
    {
        mecs::component::entity m_Entity;
    };

    struct children : mecs::component::data
    {
        std::vector<mecs::component::entity> m_List{};
    };

    struct position : mecs::component::data
    {
        xcore::vector2 m_Value;
    };

    struct local_position : mecs::component::data
    {
        xcore::vector2 m_Value;
    };

    struct velocity : mecs::component::data
    {
        xcore::vector2 m_Value;
    };

    //-----------------------------------------------------------------------------------------
    // Systems
    //-----------------------------------------------------------------------------------------
    struct update_hierarchy_system : mecs::system::instance
    {
        using instance::instance;

        // This should give us the roots
        using query_t = std::tuple
        <
            none<parent>
        >;

        void operator()( const children& Children, const position& ParentWorldPos ) noexcept
        {
            // Loop thought out all our children
            for( auto& E : Children.m_List )
            {
                getComponents( E, [&]( local_position& LocalPos, position& WorldPos, children* pChildren )
                {
                    WorldPos.m_Value = LocalPos.m_Value + ParentWorldPos.m_Value;
                    if(pChildren)
                    {
                        (*this)( *pChildren, WorldPos );
                    }
                });
            }
        }
    };

    // Move all the parents
    struct root_move_system : mecs::system::instance
    {
        using instance::instance;

        using query_t = std::tuple
        <
            none<local_position>
        >;

        static void Update( xcore::vector2& Pos, xcore::vector2& Vel, float DT ) noexcept
        {
            Pos += Vel * DT;
        }

        void operator() (position& Position, velocity& Velocity) const noexcept
        {
            Update(Position.m_Value, Velocity.m_Value, getTime().m_DeltaTime );
        }
    };

    // Move all the children
    struct local_move_system : mecs::system::instance
    {
        using instance::instance;

        void operator() (local_position& LocalPosition, velocity& Velocity) const noexcept
        {
            root_move_system::Update(LocalPosition.m_Value, Velocity.m_Value, getTime().m_DeltaTime);
        }
    };



    void Test(){}
}