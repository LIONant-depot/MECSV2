namespace mecs::system::delegate
{
    namespace details
    {
        //------------------------------------------------------------------------------------------
        // Check if msgHandleEvents is a function 
        //------------------------------------------------------------------------------------------
        template< typename, typename T >
        struct has_handle_events;

        template< typename T_CLASS, typename... T_ARGS >
        struct has_handle_events<T_CLASS, std::tuple<T_ARGS...>>
        {
            template<typename T>
            static constexpr auto            _check(T*) -> typename std::is_same< decltype(std::declval<T>().msgHandleEvents(std::declval<T_ARGS>()...)), void>::type;

            template<typename>
            static constexpr std::false_type _check(...);

            using type = decltype(_check<T_CLASS>(0));

            static constexpr bool value = type::value;
        };

        //------------------------------------------------------------------------------------------

        template< typename T_USER_DELEGATE, typename...T_ARGS > constexpr
        custom_instance< T_USER_DELEGATE, std::tuple<T_ARGS...> >::custom_instance(mecs::world::instance& World, guid Guid ) noexcept
            : m_World{ World }
        {
            user_delegate_t::m_Guid = Guid;

            //
            // Register to Graph messages
            //
            if constexpr (&custom_instance::msgGraphInit != &overrides::msgGraphInit)
            {
                m_World.m_GraphDB.m_Events.m_GraphInit.AddDelegate<&custom_instance::msgGraphInit>(*this);
            }

            if constexpr (&custom_instance::msgFrameStart != &overrides::msgFrameStart)
            {
                m_World.m_GraphDB.m_Events.m_FrameStart.AddDelegate<&custom_instance::msgFrameStart>(*this);
            }

            if constexpr (&custom_instance::msgFrameDone != &overrides::msgFrameDone)
            {
                m_World.m_GraphDB.m_Events.m_FrameDone.AddDelegate<&custom_instance::msgFrameDone>(*this);
            }

            //
            // Register to SystemDB messages
            //
            if constexpr (is_exclusive_v)
            {
                const auto& ExpectedDesc = system::descriptor_v
                <
                    std::conditional_t< std::is_same_v<custom_instance::event_t::system_t, void>
                        , mecs::system::details::default_system
                        , custom_instance::event_t::system_t >
                >;

                for( auto& E : m_World.m_GraphDB.m_SystemDB.m_lInstance )
                {
                    if( ExpectedDesc.m_Guid != E->m_TypeGuid ) continue;
                    if( auto pRealEvent = E->DetailsGetExclusiveRealEvent( mecs::system::event::descriptor_v<user_delegate_t::event_t>.m_Guid ); pRealEvent )
                    {
                        auto& RealEvent = *reinterpret_cast<typename user_delegate_t::event_t::real_event_t*>(pRealEvent);
                        if constexpr( has_handle_events<custom_instance, typename user_delegate_t::event_t::real_event_t::arguments_tuple_t>::value )
                        {
                            RealEvent.AddDelegate<&user_delegate_t::msgHandleEvents>(*this);
                        }
                        else
                        {
                            RealEvent.AddDelegate<&custom_instance::msgHandleSystemEvents>(*this);
                        }
                    }
                }
            }
            else
            {
                auto& RealEvent = m_World.m_GraphDB.m_SystemGlobalEventDB.getOrCreate<user_delegate_t::event_t>( mecs::system::event::descriptor_v<user_delegate_t::event_t>.m_Guid );
                if constexpr( has_handle_events<custom_instance, typename user_delegate_t::event_t::real_event_t::arguments_tuple_t>::value )
                {
                    RealEvent.AddDelegate<&user_delegate_t::msgHandleEvents>(*this);
                }
                else
                {
                    RealEvent.AddDelegate<&custom_instance::msgHandleSystemEvents>(*this);
                }
            }
        }

        //------------------------------------------------------------------------------------------

        template< typename T_USER_DELEGATE, typename...T_ARGS >
        void custom_instance< T_USER_DELEGATE, std::tuple<T_ARGS...> >::msgHandleSystemEvents( system::instance& System, T_ARGS... Args ) noexcept
        {
            static_assert(std::is_same_v< system::instance&, std::tuple_element_t< 0u, xcore::function::traits<self_t>::args_tuple >>,
                "Your delegate function should have <<<system::instance&>>> as your first parameter");

            (*this)( System, std::forward<T_ARGS>(Args) ... );
        }
    }

    //------------------------------------------------------------------------------------------

    inline
    overrides& instance_data_base::Create (type_guid gTypeGuid, overrides::guid InstanceGuid) noexcept
    {
        return m_World.m_Universe.m_SystemDelegateDescriptorsDB.Create(m_World, gTypeGuid, InstanceGuid);
    }

}