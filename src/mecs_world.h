namespace mecs::world
{
    using guid = xcore::guid::unit<64, struct world_tag >;

    struct instance
    {
        xforceinline instance( mecs::universe::instance& Universe ) noexcept : m_Universe{ Universe }, m_GraphDB{ *this, Universe }
        {
            m_ArchetypeDB.Init();
        }

        void Play(bool bContinuousPlay = false)
        {
            //
            // First make sure the archetypes are ready to start
            // This allows archetypes to update their double buffer components and such
            //
            if( false == m_GraphDB.m_bGraphStarted ) m_ArchetypeDB.Start();

            //
            // Lets start the graph
            //
            m_GraphDB.Play(bContinuousPlay);
        }

        void Stop( void )
        {
            m_GraphDB.Stop();
        }

        template< typename...T_COMPONENTS >
        xforceinline
        mecs::archetype::instance& getOrCreateArchitype(void) noexcept
        {
            return m_ArchetypeDB.getOrCreateArchitype<T_COMPONENTS ... >();
        }

        template< typename      T_CALLBACK          = void(*)()
                , typename...   T_SHARE_COMPONENTS >
        xforceinline
        void CreateEntity( mecs::archetype::instance& Archetype, T_CALLBACK&& Callback = []() {}, T_SHARE_COMPONENTS&&... ShareComponets )
        {
            return Archetype.CreateEntity( m_GraphDB.m_GraphSystem, std::forward<T_CALLBACK>(Callback), std::forward<T_SHARE_COMPONENTS&&>(ShareComponets) ... );
        }

        template< typename T_SYSTEM, typename...T_END_SYNC_POINTS >
        xforceinline
        T_SYSTEM& CreateGraphConnection( mecs::sync_point::instance& StartSyncpoint, T_END_SYNC_POINTS&...EndSyncpoints ) noexcept
        {
            return m_GraphDB.CreateGraphConnection<T_SYSTEM>(StartSyncpoint, std::forward<T_END_SYNC_POINTS&>(EndSyncpoints) ... );
        }

        xforceinline
        auto& getStartSyncpoint()
        {
            return m_GraphDB.m_StartSyncPoint;
        }

        xforceinline
        auto& getEndSyncpoint()
        {
            return m_GraphDB.m_EndSyncPoint;
        }

        template< typename T_ARCHETYPE_DELEGATE >
        xforceinline
        T_ARCHETYPE_DELEGATE& CreateArchetypeDelegate()
        {
            return m_GraphDB.CreateArchetypeDelegate<T_ARCHETYPE_DELEGATE>();
        }

        template< typename T_SYSTEM_DELEGATE >
        xforceinline
        T_SYSTEM_DELEGATE& CreateSystemDelegate(mecs::system::delegate::overrides::guid Guid = mecs::system::delegate::overrides::guid{ xcore::not_null }) noexcept
        {
            return m_GraphDB.CreateSystemDelegate< T_SYSTEM_DELEGATE>(Guid);
        }

        template< typename T_SYNC_POINT = mecs::sync_point::instance >
        xforceinline
        T_SYNC_POINT& CreateSyncPoint(mecs::sync_point::instance::guid Guid = mecs::sync_point::instance::guid{ xcore::not_null }) noexcept
        {
            return m_GraphDB.CreateSyncPoint<T_SYNC_POINT>(Guid);
        }

        template< typename      T_CREATION_CALLBACK = void(*)()
                , typename...   T_SHARE_COMPONENTS >
        xforceinline
        void CreateEntities( mecs::archetype::instance&                 Archetype
                            , const int                                 nEntities
                            , std::span<mecs::component::entity::guid>  Guids
                            , T_CREATION_CALLBACK&&                     CreationCallback = []() {}
                            , T_SHARE_COMPONENTS&&...                   ShareComponents
                           ) noexcept
        {
            if constexpr ( std::is_same_v<T_CREATION_CALLBACK, void(*)() > )
            {
                Archetype.CreateEntities( m_GraphDB.m_GraphSystem
                                        , nEntities
                                        , Guids
                                        , std::forward<T_SHARE_COMPONENTS&&>(ShareComponents) ...
                                        )();
            }
            else
            {
                Archetype.CreateEntities( m_GraphDB.m_GraphSystem
                                        , nEntities
                                        , Guids
                                        , std::forward<T_SHARE_COMPONENTS&&>(ShareComponents) ... 
                                        )( std::forward<T_CREATION_CALLBACK&&>(CreationCallback) );
                
            }
        }



        guid                            m_Guid              {};
        mecs::universe::instance&       m_Universe;
        mecs::archetype::data_base      m_ArchetypeDB       {};
        mecs::graph::instance           m_GraphDB;
    };
}
