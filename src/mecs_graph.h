namespace mecs::graph
{
    namespace details
    {
        struct graph_system : mecs::system::instance
        {
            inline                     graph_system            ( construct&& Construct )        noexcept;
            inline void                msgUpdate               ( void )                         noexcept;
            inline void                msgSyncPointStart       ( mecs::sync_point::instance& )  noexcept;
            virtual void               qt_onDone               ( void )                         noexcept override;

            bool m_bPause{false};
        };
    }

    struct lock_error
    {
        std::array<mecs::system::instance*, 2>          m_pSystems;
        mecs::archetype::instance*                      m_pArchetype;
        const char*                                     m_pMessage;
    };

    struct events
    {
        using graph_init  = xcore::types::make_unique< mecs::tools::event<world::instance&>,    struct graph_init_tag >;
        using frame_start = xcore::types::make_unique< mecs::tools::event<>,                    struct frame_start_tag >;
        using frame_done  = xcore::types::make_unique< mecs::tools::event<>,                    struct frame_done_tag >;

        graph_init  m_GraphInit;
        frame_start m_FrameStart;
        frame_done  m_FrameDone;
    };

    struct instance
    {
        static constexpr auto start_sync_point_v = mecs::sync_point::instance::guid{ 1ull };
        static constexpr auto end_sync_point_v   = mecs::sync_point::instance::guid{ 2ull };

        events                      m_Events;

        instance( mecs::world::instance& World, mecs::universe::instance& Universe )
            : m_Universe                { Universe }
            , m_World                   { World }
            , m_SystemDB                { World }
            , m_SystemGlobalEventDB     { World }
            , m_ArchetypeDelegateDB     { World }
            , m_SystemDelegateDB        { World }
            , m_SyncpointDB             { World }
            , m_StartSyncPoint          { m_SyncpointDB.Create<mecs::sync_point::instance>(start_sync_point_v) }
            , m_EndSyncPoint            { m_SyncpointDB.Create<mecs::sync_point::instance>(end_sync_point_v)   }
            , m_GraphSystem             { CreateGraphConnection< mecs::graph::details::graph_system >(m_EndSyncPoint, m_StartSyncPoint) }
        {
        }

        void Play( bool bContinuousPlay ) noexcept
        {
            m_bContinuousPlay           = bContinuousPlay;

            //
            // Add the graph as the core of the graph
            //
            XCORE_PERF_FRAME_MARK()
            auto& Scheduler = xcore::get().m_Scheduler;

            Scheduler.AddJobToQuantumWorld   (m_GraphSystem);
            Scheduler.MainThreadStartsWorking();
        }

        void Stop( void ) noexcept
        {
            m_bGraphStarted = false;
        }

        template< typename T_SYSTEM, typename...T_END_SYNC_POINTS >
        T_SYSTEM& CreateGraphConnection( mecs::sync_point::instance& StartSyncpoint, T_END_SYNC_POINTS&...EndSyncpoints ) noexcept;

        template< typename T_ARCHETYPE_DELEGATE >
        T_ARCHETYPE_DELEGATE& CreateArchetypeDelegate(mecs::archetype::delegate::overrides::guid Guid = mecs::archetype::delegate::overrides::guid{ xcore::not_null }) noexcept;

        template< typename T_SYSTEM_DELEGATE >
        T_SYSTEM_DELEGATE& CreateSystemDelegate(mecs::system::delegate::overrides::guid Guid = mecs::system::delegate::overrides::guid{ xcore::not_null }) noexcept;

        template< typename T_DELEGATE > xforceinline
        auto& CreateDelegate( void ) noexcept
        {
            if constexpr ( std::is_base_of_v< mecs::archetype::delegate::overrides, T_DELEGATE > )
                return CreateArchetypeDelegate<T_DELEGATE>();
            else
                return CreateSystemDelegate<T_DELEGATE>();
        }

        template< typename T_SYNC_POINT = mecs::sync_point::instance >
        T_SYNC_POINT& CreateSyncPoint( mecs::sync_point::instance::guid Guid = mecs::sync_point::instance::guid{ xcore::not_null} ) noexcept
        {
            return m_SyncpointDB.Create<T_SYNC_POINT>(Guid);
        }


        bool                                            m_bContinuousPlay       { false };
        bool                                            m_bGraphStarted         { false };
        bool                                            m_bFrameStarted         { false };
        std::uint64_t                                   m_FrameNumber           {0};
        time                                            m_Time                  {};

        mecs::universe::instance&                       m_Universe;
        mecs::world::instance&                          m_World;
        mecs::system::instance_data_base                m_SystemDB;
        mecs::sync_point::instance_data_base            m_SyncpointDB;
        mecs::system::event::instance_data_base         m_SystemGlobalEventDB;
        mecs::archetype::delegate::instance_data_base   m_ArchetypeDelegateDB;
        mecs::system::delegate::instance_data_base      m_SystemDelegateDB;

        mecs::sync_point::instance&                     m_StartSyncPoint;
        mecs::sync_point::instance&                     m_EndSyncPoint;
        mecs::graph::details::graph_system&             m_GraphSystem;


        xcore::lock::object<std::vector<lock_error>, xcore::lock::spin> m_LockErrors{};
    };
}