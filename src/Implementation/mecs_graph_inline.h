namespace mecs::graph
{
    namespace details
    {
        //----------------------------------------------------------------------------

        inline
        graph_system::graph_system( construct&& Construct ) noexcept
        : instance
        { []( auto&& OldConstruct)
          {
            auto NewConstruct = OldConstruct;

            NewConstruct.m_Name = xconst_universal_str("graph_system");
            NewConstruct.m_JobDefinition = xcore::scheduler::definition::definition::Flags
            (
                  xcore::scheduler::lifetime::DONT_DELETE_WHEN_DONE
                , xcore::scheduler::jobtype::NORMAL
                , xcore::scheduler::affinity::NORMAL
                , xcore::scheduler::priority::NORMAL
                , xcore::scheduler::triggers::DONT_CLEAN_COUNT
            );

            return NewConstruct;
         }(Construct)
        }
        {}

        //----------------------------------------------------------------------------

        inline
        void graph_system::msgUpdate() noexcept
        {
            auto& Graph = m_World.m_GraphDB;

            // If we are in pause this system does not run...
            if (m_bPause == true) return;

            XCORE_PERF_FRAME_MARK_START("mecs::Frame")

            //
            // Update Time
            //
            if(Graph.m_FrameNumber == 0 )   Graph.m_Time.Start();
            else                            Graph.m_Time.UpdateDeltaTimeRoundTrip();

            //
            // Frame counters, etc...
            //
            Graph.m_bFrameStarted = true;
            Graph.m_FrameNumber++;
            Graph.m_Events.m_FrameStart.NotifyAll();
        }

        //----------------------------------------------------------------------------

        inline
        void graph_system::msgSyncPointStart(mecs::sync_point::instance&) noexcept
        {
            auto& Graph = m_World.m_GraphDB;

            XCORE_PERF_ZONE_SCOPED()

            Graph.m_bFrameStarted = false;

            //
            // Let everyone know that we are done
            //
            Graph.m_Events.m_FrameDone.NotifyAll();

            //
            // Deal with profiler
            //
            XCORE_PERF_FRAME_MARK_END("mecs::Frame")
            //TODO: XCORE_PERF_PLOT("Total Pages", static_cast<int64_t>(m_PageMgr.m_TotalPages.load(std::memory_order_relaxed)))
            //TODO: XCORE_PERF_PLOT("Free Pages", static_cast<int64_t>(m_PageMgr.m_FreePages.load(std::memory_order_relaxed)))
        }

        //----------------------------------------------------------------------------
        inline 
        void graph_system::qt_onDone( void ) noexcept
        {
            if( m_bPause == true )
            {
                m_bPause = false;
                xcore::get().m_Scheduler.MainThreadStopWorking();
                // We do not call our parent which means that no other trigger gets executed
                // So at this point it should exit
            }
            else
            {
                auto& Graph = m_World.m_GraphDB;

                //
                // If it is the very first frame then we need to deal with some details...
                //
                if (Graph.m_bGraphStarted == false)
                {
                    //
                    // Force the system to flush its cache
                    //
                    auto& CustomSystem = static_cast<mecs::system::details::custom_system<graph_system>&>(*this);
                    CustomSystem.msgSyncPointDone( Graph.m_EndSyncPoint );

                    //
                    // Notify anyone that the graph is starting
                    //
                    Graph.m_Events.m_GraphInit.NotifyAll(m_World);

                    //
                    // Clear some variables
                    //
                    Graph.m_FrameNumber   = 0;
                    Graph.m_bGraphStarted = true;
                }
                else
                {
                    if (false == Graph.m_bContinuousPlay) m_bPause = true;
                }
                mecs::system::instance::qt_onDone();
            }
        }
    }

    //------------------------------------------------------------------------------------------------------------
    // GRAPH:: INSTANCE
    //------------------------------------------------------------------------------------------------------------

    //------------------------------------------------------------------------------------------------------------

    template< typename T_SYSTEM, typename...T_END_SYNC_POINTS >
    T_SYSTEM& instance::CreateGraphConnection(mecs::sync_point::instance& StartSyncpoint, T_END_SYNC_POINTS&...EndSyncpoints) noexcept
    {
        static_assert(std::is_base_of_v< mecs::system::overrides, T_SYSTEM >, "This is not a system");
        static_assert(sizeof...(EndSyncpoints) > 0, "You must pass at least one end Sync_point");
        static_assert(std::is_base_of_v<mecs::sync_point::instance, std::common_type_t<T_END_SYNC_POINTS...>>, "All the end sync_points must be non const sync_points");

        m_Universe.registerTypes<T_SYSTEM>();

        using custom_system_t = mecs::system::details::custom_system<T_SYSTEM>;
        auto& System          = static_cast<custom_system_t&>(m_SystemDB.Create<T_SYSTEM>(mecs::system::instance::guid{xcore::not_null}));

        StartSyncpoint.AddJobToBeTrigger(System);
        (EndSyncpoints.JobWillNotifyMe(System), ...);

        //
        // Register to SyncPoint messages as needed
        //

        // This message is a must have since we need to deal with the memory barrier
        {
            auto& MainEndSyncPoint = xcore::types::PickFirstArgument(std::forward<T_END_SYNC_POINTS>(EndSyncpoints) ...);
            MainEndSyncPoint.m_Events.m_Done.AddDelegate<&custom_system_t::msgSyncPointDone>(System);
        }

        if constexpr (&custom_system_t::msgSyncPointStart != &system::overrides::msgSyncPointStart)
        {
            StartSyncpoint.m_Events.m_Start.AddDelegate<&custom_system_t::msgSyncPointStart>(System);
        }

        return System;
    }


    //------------------------------------------------------------------------------------------------------------

    template< typename T_ARCHETYPE_DELEGATE >
    T_ARCHETYPE_DELEGATE& instance::CreateArchetypeDelegate(mecs::archetype::delegate::overrides::guid Guid ) noexcept
    {
        using custom_delegate_t = typename mecs::archetype::delegate::details::custom_instance<T_ARCHETYPE_DELEGATE>;
        auto& Delegate = static_cast<custom_delegate_t&>(m_ArchetypeDelegateDB.Create<T_ARCHETYPE_DELEGATE>(Guid));
        return Delegate;
    }

    //------------------------------------------------------------------------------------------------------------

    template< typename T_SYSTEM_DELEGATE >
    T_SYSTEM_DELEGATE& instance::CreateSystemDelegate(mecs::system::delegate::overrides::guid Guid) noexcept
    {
        m_Universe.registerTypes<T_SYSTEM_DELEGATE>();
        auto& Delegate = static_cast<T_SYSTEM_DELEGATE&>(m_SystemDelegateDB.Create( mecs::system::delegate::descriptor_v<T_SYSTEM_DELEGATE>.m_Guid, Guid ));
        return Delegate;
    }
}