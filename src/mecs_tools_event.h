namespace mecs::tools
{
    template< typename ...T_ARGS >
    struct event
    {
        using arguments_tuple_t = std::tuple< T_ARGS... >;

        struct entry
        {
            using fn = void(void* pThis, T_ARGS&&...) noexcept;
            void*   m_pThis;
            fn*     m_pCallBack;
        };

        template< auto T_CALLBACK, typename T >
        xforceinline void AddDelegate(T& This) noexcept
        {
            m_lDelegates.push_back({ &This, [](void* pThis, T_ARGS&&... Args) constexpr noexcept
            {
                std::invoke(T_CALLBACK, reinterpret_cast<T*>(pThis), std::forward<T_ARGS>(Args) ...);
            } });
        }

        template< typename T >
        xforceinline void RemoveDelegate(T* pThis) noexcept
        {
            for (auto& E : m_lDelegates) if (E.m_pThis == pThis)
            {
                m_lDelegates.remove(E);
                break;
            }
        }

        xforceinline constexpr bool hasSubscribers      (void)            const noexcept { return m_lDelegates.empty() == false; }
        xforceinline constexpr void NotifyAll           (T_ARGS... Args)  const noexcept
        {
            for (auto& E : m_lDelegates) E.m_pCallBack(E.m_pThis, std::forward<T_ARGS>(Args) ...);
        }

        std::vector<entry>       m_lDelegates;
    };
}