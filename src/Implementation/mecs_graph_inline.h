namespace mecs::graph
{

    template< typename T_SYSTEM, typename...T_END_SYNC_POINTS >
    T_SYSTEM& instance::CreateGraphConnection(mecs::sync_point::instance& StartSyncpoint, T_END_SYNC_POINTS&...EndSyncpoints) noexcept
    {
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

        if constexpr (&custom_system_t::msgSyncPointStart != &system::instance::msgSyncPointStart)
        {
            System.m_WorldStartDelegate.setupThis(System);
            System.m_SyncPointStartDelegate.Connect(StartSyncpoint.m_Events.m_Start);
        }

        //
        // Register to World Instances messages
        //
        if constexpr (&custom_system_t::msgGraphInit != &system::instance::msgGraphInit)
        {
            System.m_WorldStartDelegate.setupThis(System);
            System.m_WorldStartDelegate.Connect(m_Events.m_GraphInit);
        }

        if constexpr (&custom_system_t::msgFrameStart != &system::instance::msgFrameStart)
        {
            System.m_FrameStartDelegate.setupThis(System);
            System.m_FrameStartDelegate.Connect(m_Events.m_FrameStart);
        }

        if constexpr (&custom_system_t::msgFrameDone != &system::instance::msgFrameDone)
        {
            System.m_FrameDoneDelegate.setupThis(System);
            System.m_FrameDoneDelegate.Connect(m_Events.m_FrameDone);
        }

        //
        // Register all the events
        //
        details::RegisterSystemEventTuple( System.m_Guid, m_SystemEventDB, System.m_Events );

        return System;
    }


    template< typename T_ARCHETYPE_DELEGATE >
    T_ARCHETYPE_DELEGATE& instance::CreateArchetypeDelegate(mecs::archetype::delegate::overrites::guid Guid ) noexcept
    {
        using custom_delegate_t = typename mecs::archetype::delegate::details::custom_instance<T_ARCHETYPE_DELEGATE>;
        auto& Delegate = static_cast<custom_delegate_t&>(m_ArchetypeDelegateDB.Create<T_ARCHETYPE_DELEGATE>(Guid));
        return Delegate;
    }


}