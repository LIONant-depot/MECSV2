namespace mecs::tools
{
    //---------------------------------------------------------------------------------
    // TOOLS:qvstp - Quantum Virtual Single Type Pool
    //               Please note that only append in quantum
    //---------------------------------------------------------------------------------
    template< typename T_TYPE >
    class qvstp
    {
    public:

        using                           self            = qvstp;
        using                           entry           = T_TYPE;
        using                           iterator        = entry*;
        constexpr static std::size_t    page_size_v     = 4096;
        constexpr static auto           page_size_pow_v = xcore::bits::Log2Int(page_size_v);

    public:

        constexpr   auto begin()                        const   noexcept { return size() ? m_pEntries : nullptr; }
                    auto begin()                                noexcept { return size() ? m_pEntries : nullptr; }
        constexpr   auto end()                          const   noexcept { return size() ? &m_pEntries[size()] : nullptr; }
        constexpr   auto size()                         const   noexcept { return m_Count.load(std::memory_order_relaxed).m_Count; }


        void Init( std::uint32_t MaxEntries ) noexcept
        {
            m_MaxEntries = MaxEntries;

            auto page_count_v = [&]
            {
                auto PageCount = (m_MaxEntries * sizeof(entry)) / page_size_v;
                return PageCount + ((PageCount * page_size_v) < (m_MaxEntries * sizeof(entry)));
            }();

            m_MaxEntries = static_cast<std::uint32_t>(page_count_v / sizeof(entry));
            m_pEntries   = reinterpret_cast<entry*>(VirtualAlloc(nullptr, page_count_v * page_size_v, MEM_RESERVE, PAGE_NOACCESS));
        }

        ~qvstp(void) noexcept
        {
            clear();
            if (m_pEntries) VirtualFree(m_pEntries, 0, MEM_RELEASE);
        }

        template< typename...T_ARGS >
        std::size_t append(T_ARGS&&... Args) noexcept
        {
            counter             NewCount;
            std::size_t         k;
            std::size_t         Count;

            //
            // Compute the Offsets, and bit Types that need to be allocated and determine if we need to lock
            //
            do
            {
                counter LocalCount = m_Count.load(std::memory_order_relaxed);

                if (LocalCount.m_Lock)
                {
                    do
                    {
                        LocalCount = m_Count.load(std::memory_order_relaxed);
                    } while (LocalCount.m_Lock);
                }

                           Count        = LocalCount.m_Count;
                const auto ByteOffset   = Count * sizeof(entry);
                           k            = (ByteOffset + sizeof(entry)) >> page_size_pow_v;

                NewCount.m_Lock         = Count == 0 || k != (ByteOffset >> page_size_pow_v);
                NewCount.m_Count        = Count + 1;

                if (m_Count.compare_exchange_weak(LocalCount, NewCount, std::memory_order_release, std::memory_order_relaxed)) break;

            } while (true);

            //
            // Allocate if needed
            //
            if( NewCount.m_Lock )
            {
                auto pRaw = &reinterpret_cast<std::byte*>(m_pEntries)[k * page_size_v];
                auto p    = VirtualAlloc(pRaw, page_size_v, MEM_COMMIT, PAGE_READWRITE);
                xassert(p == pRaw);

                // Release the lock
                NewCount.m_Lock = 0;
                m_Count.store(NewCount, std::memory_order_relaxed);
            }

            //
            // construct the entry
            //
            if constexpr (std::is_constructible_v<entry>)
            {
                if constexpr (sizeof...(T_ARGS) > 0)
                {
                    new(&m_pEntries[Count]) entry{ std::forward<T_ARGS>(Args)... };
                }
                else
                {
                    new(&m_pEntries[Count]) entry;
                }
            }

            return Count;
        }

        template< typename T >
        auto& operator[](T Index) noexcept
        {
            xassert( Index >= 0 );
            xassert( static_cast<std::uint32_t>(Index) < m_Count.load(std::memory_order_relaxed).m_Count );
            return m_pEntries[Index];
        }

        template< typename T >
        constexpr auto& operator[](T Index) const noexcept
        {
            xassert(Index < m_Count.load(std::memory_order_relaxed).m_Count);
            return m_pEntries[Index];
        }

        void clear() noexcept
        {
            const auto Count = m_Count.load(std::memory_order_relaxed).m_Count;
            if constexpr (std::is_destructible_v<entry>)
            {
                for (auto i = 0u; i < Count; ++i)
                    std::destroy_at(&m_pEntries[i]);
            }

            // Free memory
            VirtualFree(m_pEntries, xcore::bits::Align(Count*sizeof(entry), page_size_v), MEM_DECOMMIT);

            // Reset the count
            m_Count.store({ 0 }, std::memory_order_relaxed);
        }

    protected:

        union counter
        {
            std::uint32_t       m_FullValue;
            struct
            {
                std::uint32_t   m_Count : 31
                              , m_Lock  : 1;
            };
        };

    protected:

        entry*                  m_pEntries      { nullptr };
        std::atomic<counter>    m_Count         {{0}};
        std::uint32_t           m_MaxEntries    {0};
    };
}