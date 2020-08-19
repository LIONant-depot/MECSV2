namespace mecs::graph
{
    struct lock_error
    {
        std::array<mecs::system::instance::guid, 2>     m_lSystemGuids;
        mecs::archetype::instance::guid                 m_gArchetype;
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

    namespace details
    {
        template< typename...T_ARGS > xforceinline
        void RegisterSystemEventTuple( mecs::system::instance::guid Guid, mecs::system::event::instance_data_base& SystemEventDB, std::tuple<T_ARGS...>& Events ) noexcept
        {
            ( SystemEventDB.AddSystemEvent( Guid, mecs::system::event::descriptor_v<T_ARGS>.m_Guid,  &std::get<T_ARGS>(Events) ), ... );
        }
    }

    struct instance : xcore::scheduler::job<1>
    {
      //  template< typename T_SYSTEM >
        //void AddGraphConnection( mecs::syncpin)

        events                      m_Events;
        mecs::sync_point::instance  m_StartSyncPoint;
        mecs::sync_point::instance  m_EndSyncPoint;

        instance( mecs::world::instance& World, mecs::universe::instance& Universe )
            : xcore::scheduler::job<1>
            { xcore::scheduler::definition::definition::Flags
                (
                  xcore::scheduler::lifetime::DONT_DELETE_WHEN_DONE
                , xcore::scheduler::jobtype::NORMAL
                , xcore::scheduler::affinity::NORMAL
                , xcore::scheduler::priority::NORMAL
                , xcore::scheduler::triggers::DONT_CLEAN_COUNT 
                )
            }
            , m_Universe{ Universe }
            , m_World(World)
            , m_SystemDB(World){}

        void Init( void ) noexcept
        {
            m_SystemDB.Init();
            m_SyncpointDB.Init();

            //
            // Add the graph as the core of the graph
            //
            m_EndSyncPoint.AddJobToBeTrigger(*this);
            m_StartSyncPoint.JobWillNotifyMe(*this);
        }

        void Start( bool bContinuousPlay ) noexcept
        {
            //
            // Make sure I am the last system to get notified in the End trigger
            //
            for( auto it = m_EndSyncPoint.m_Events.m_Done.m_lDelegates.begin(); it != m_EndSyncPoint.m_Events.m_Done.m_lDelegates.end(); ++it )
            {
                if (it->m_pThis == this) m_EndSyncPoint.m_Events.m_Done.m_lDelegates.erase(it);
            }
            m_EndSyncPoint.m_Events.m_Done.AddDelegate<&instance::msgSyncPointDone>(*this);

            //
            // Clear some variables
            //
            m_bContinuousPlay           = bContinuousPlay;
            m_EndSyncPoint.m_bDisable   = !m_bContinuousPlay;
            m_FrameNumber               = 0;

            //
            // Get started
            //
            XCORE_PERF_FRAME_MARK()
            auto& Scheduler = xcore::get().m_Scheduler;
            Scheduler.AddJobToQuantumWorld(*this);
            Scheduler.MainThreadStartsWorking();
        }

        void Resume( bool bContinuousPlay ) noexcept
        {
            m_bContinuousPlay           = bContinuousPlay;
            m_EndSyncPoint.m_bDisable   = !m_bContinuousPlay;

            //
            // Add the graph as the core of the graph
            //
            XCORE_PERF_FRAME_MARK()
            auto& Scheduler = xcore::get().m_Scheduler;
            Scheduler.AddJobToQuantumWorld(*this);
            Scheduler.MainThreadStartsWorking();
        }

        template< typename T_SYSTEM, typename...T_END_SYNC_POINTS >
        T_SYSTEM& CreateGraphConnection( mecs::sync_point::instance& StartSyncpoint, T_END_SYNC_POINTS&...EndSyncpoints ) noexcept;

        template< typename T_DELEGATE >
        void registerDelegate( void )
        {
            
        }

        template< typename T_DELEGATE >
        void registerDelegate( mecs::system::delegate::type_guid Guid )
        {
            
        }

        virtual void qt_onRun(void) noexcept override
        {
            XCORE_PERF_FRAME_MARK_START("mecs::Frame")
            m_bFrameStarted = true;
            m_FrameNumber++;
            m_Events.m_FrameStart.NotifyAll();
        }

        void msgSyncPointDone( mecs::sync_point::instance& SyncPoint)
        {
            XCORE_PERF_ZONE_SCOPED()

            m_bFrameStarted = false;

            //
            // Let everyone know that we are done
            //
            m_Events.m_FrameDone.NotifyAll();

            //
            // Deal with profiler
            //
            XCORE_PERF_FRAME_MARK_END("mecs::Frame")
            //TODO: XCORE_PERF_PLOT("Total Pages", static_cast<int64_t>(m_PageMgr.m_TotalPages.load(std::memory_order_relaxed)))
            //TODO: XCORE_PERF_PLOT("Free Pages", static_cast<int64_t>(m_PageMgr.m_FreePages.load(std::memory_order_relaxed)))

            //
            // Decided if we need to return control to the system
            //
            if(false == m_bContinuousPlay) xcore::get().m_Scheduler.MainThreadStopWorking();
        }

        bool                                    m_bContinuousPlay       { false };
        bool                                    m_bFrameStarted         { false };
        std::uint64_t                           m_FrameNumber           {0};

        mecs::universe::instance&               m_Universe;
        mecs::world::instance&                  m_World;
        mecs::system::instance_data_base        m_SystemDB;
        mecs::sync_point::instance_data_base    m_SyncpointDB           {};
        mecs::system::event::instance_data_base m_SystemEventDB         {};
//        mecs::archetype::data_base              m_ArchetypeDB           {};

        xcore::lock::object<std::vector<lock_error>, xcore::lock::spin> m_LockErrors{};
    };
}