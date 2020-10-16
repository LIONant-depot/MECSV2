namespace mecs::universe
{
    struct instance
    {
        instance()
        {
            Init();
        }

        void Init()
        {
            m_SyncpointDescriptorsDB.Init();
            m_SystemDescriptorsDB.Init();
            m_ComponentDescriptorsDB.Init();
            m_SystemEventDescriptorsDB.Init();
            m_SystemDelegateDescriptorsDB.Init();
            m_ArchetypeDelegateDescriptorsDB.Init();

            m_WorldDB.push_back( std::make_unique<mecs::world::instance>(*this) );
        }

        template< typename...T_ARGS > constexpr xforceinline
        void registerTypes( void ) noexcept
        {
            static_assert(!!sizeof...(T_ARGS));
            static constexpr auto Reg = []( mecs::universe::instance& I, auto* p ) noexcept
            {
                using t = xcore::types::decay_full_t<decltype(p)>;
                if constexpr (std::is_base_of_v<mecs::sync_point::overrides, t>)
                {
                    I.m_SyncpointDB.Register<t>();
                }
                else if constexpr (std::is_base_of_v<mecs::system::overrides, t>)
                {
                    I.m_SystemDescriptorsDB.Register<t>( I.m_SystemEventDescriptorsDB );
                }
                else if constexpr   (   std::is_base_of_v<mecs::component::data,        t>
                                    ||  std::is_base_of_v<mecs::component::share,       t>
                                    ||  std::is_base_of_v<mecs::component::singleton,   t>
                                    ||  std::is_base_of_v<mecs::component::tag,         t>
                                    )
                {
                    I.m_ComponentDescriptorsDB.registerComponent<t>();
                }
                else if constexpr (std::is_base_of_v<mecs::archetype::delegate::overrides, t>)
                {
                    I.m_ArchetypeDelegateDescriptorsDB.Register<t>();
                }
                else if constexpr (std::is_base_of_v<mecs::system::delegate::overrides, t>)
                {
                    I.m_SystemDelegateDescriptorsDB.Register<t>();
                }
                else
                {
                    static_assert(xcore::types::always_false_v<t>);
                }
            };

            (Reg(*this, reinterpret_cast<T_ARGS*>(nullptr)), ...);
        }

        mecs::sync_point::descriptors_data_base             m_SyncpointDescriptorsDB            {};
        mecs::system::descriptors_data_base                 m_SystemDescriptorsDB               {};
        mecs::component::descriptors_data_base              m_ComponentDescriptorsDB            {};
        mecs::system::event::descriptors_data_base          m_SystemEventDescriptorsDB          {};
        mecs::system::delegate::descriptors_data_base       m_SystemDelegateDescriptorsDB       {};
        mecs::archetype::delegate::descriptors_data_base    m_ArchetypeDelegateDescriptorsDB    {};
        std::vector<std::unique_ptr<mecs::world::instance>> m_WorldDB                           {};
    };
}