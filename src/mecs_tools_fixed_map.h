namespace mecs::tools
{
    //---------------------------------------------------------------------------------
    // TOOLS::fixed_map - Fixed sized quantum map with per entry locked and thread-safe callbacks 
    //---------------------------------------------------------------------------------
    template< typename T_KEY, typename T_VALUE, std::size_t T_MAXSIZE_V >
    struct fixed_map
    {
        using                           key                     = T_KEY;
        using                           value                   = T_VALUE;
        using                           entry_magic_offset      = std::uint16_t;

        static constexpr auto           max_size_v              = []() constexpr 
        {
            // does not need to be a power of 2 but it will be faster.
            // Add an additional 30% of entries for collisions and such
            const auto t = xcore::bits::RoundToNextPowOfTwo(T_MAXSIZE_V);
            const auto x = T_MAXSIZE_V + (T_MAXSIZE_V * 30) / 100;
            return ( t >= x ) ? t : xcore::bits::RoundToNextPowOfTwo(x);
        }();
        static constexpr std::uint16_t  unused_entry_v          = 0xffff;
        static constexpr std::uint16_t  end_linklist_v          = unused_entry_v - 1;

        struct entry
        {
            key                             m_Key;                                  // Key
            value                           m_Value;                                // Value
            std::atomic<entry_magic_offset> m_iNextEntryOrEmpty{ unused_entry_v };  // can be: end_linklist_v, or unused_entry_v, any other value is the offset to the entry
        };

        // Row is the core of the hash table.
        // Rows represent a set of keys which index to its location
        struct row
        {
            entry                                                             m_Entry{};                             // Memory where an entry may be allocated
            xcore::lock::object< entry_magic_offset, xcore::lock::semaphore > m_iHeadEntryOrEmpty{ end_linklist_v }; // can be: end_linklist_v, or any other value is the offset to the entry
        };                                                                                                      

        constexpr fixed_map()
        {
            // Allocate the memory
            m_Map.New(max_size_v).CheckError();
        }

        void clear()
        {
            m_Map.clear();
        }

        constexpr xforceinline entry& getEntryFromItsValue( value& Value ) const noexcept
        {
            xassert(reinterpret_cast<std::size_t>(&Value) >= reinterpret_cast<std::size_t>(m_Map.data()));
            xassert(reinterpret_cast<std::size_t>(&Value) < reinterpret_cast<std::size_t>(m_Map.end()));
            return *reinterpret_cast<entry*>(reinterpret_cast<std::size_t>(&Value) - offsetof(entry, m_Value));
        }

        template< typename T_CALLBACK = void(*)(value&)> xforceinline constexpr
        entry* alloc(key Key, T_CALLBACK&& Callback = [](value&) constexpr noexcept {}) noexcept
        {
            assert(Key.isValid());
            const auto  RowTrueIndex = Key.m_Value % max_size_v;
            auto&       Row          = m_Map[RowTrueIndex];

            // Lock the row link list
            xcore::lock::scope Lk( Row.m_iHeadEntryOrEmpty );

            // Make sure still not constructed
            for( auto i = Row.m_iHeadEntryOrEmpty.get(); i != end_linklist_v; )
            {
                auto& NextRow = m_Map[ (RowTrueIndex + i) % max_size_v ].m_Entry;
                if (NextRow.m_Key == Key) return &NextRow;
                i = NextRow.m_iNextEntryOrEmpty.load(std::memory_order_relaxed);
            }

            // Find/Alloc free entry memory
            auto pEntry         = &Row.m_Entry;
            auto EntryTrueIndex = RowTrueIndex;
            auto L              = pEntry->m_iNextEntryOrEmpty.load(std::memory_order_relaxed);
            do
            {
                if( L != unused_entry_v)
                {
                    EntryTrueIndex++;
                    pEntry         = &m_Map[EntryTrueIndex % max_size_v].m_Entry;
                    L              = pEntry->m_iNextEntryOrEmpty.load(std::memory_order_relaxed);
                }
                // Store our entry_index so must convert from true_index by adding one
                else if ( pEntry->m_iNextEntryOrEmpty.compare_exchange_weak(L, Row.m_iHeadEntryOrEmpty.get() )) break;

            } while (true);

            // Set our entry 
            pEntry->m_Key                 = Key;
            Row.m_iHeadEntryOrEmpty.get() = static_cast<entry_magic_offset>( EntryTrueIndex - RowTrueIndex );

            // Setup the node
            Callback( pEntry->m_Value );

            return nullptr;
        }

        template< typename T = void(*)(const value&)> xforceinline constexpr
        bool find(key Key, T&& CallBack = [](const value&) constexpr noexcept {}) const noexcept
        {
            xassert(Key.isValid());

            const auto  RowTrueIndex = Key.m_Value % max_size_v;
            auto&       Row          = m_Map[RowTrueIndex];

            // User requesting Read only lock here because the function is const
            xcore::lock::scope Lk(Row.m_iHeadEntryOrEmpty);

            for (auto i = Row.m_iHeadEntryOrEmpty.get(); i != end_linklist_v; )
            {
                auto& NextRow = m_Map[(RowTrueIndex + i) % max_size_v].m_Entry;
                if (NextRow.m_Key == Key)
                {
                    CallBack(NextRow.m_Value);
                    return true;
                }
                i = NextRow.m_iNextEntryOrEmpty.load(std::memory_order_relaxed);
            }

            return false;
        }

        template< typename T = void(*)(value&)> xforceinline constexpr
        bool find(key Key, T&& CallBack = [](value&) constexpr noexcept {}) noexcept
        {
            xassert(Key.isValid());

            const auto  RowTrueIndex = Key.m_Value % max_size_v;
            auto&       Row          = m_Map[RowTrueIndex];

            // User requesting Read only lock here because the function is const
            xcore::lock::scope Lk(Row.m_iHeadEntryOrEmpty);

            for (auto i = Row.m_iHeadEntryOrEmpty.get(); i != end_linklist_v; )
            {
                auto& NextRow = m_Map[(RowTrueIndex + i) % max_size_v].m_Entry;
                if (NextRow.m_Key == Key)
                {
                    CallBack(NextRow.m_Value);
                    return true;
                }
                i = NextRow.m_iNextEntryOrEmpty.load(std::memory_order_relaxed);
            }

            return false;
        }

        template< typename T_CREATE_CALLBACK, typename T_GET_CALLBACK > xforceinline constexpr
        bool getOrCreate(key Key, T_GET_CALLBACK&& GetCallback, T_CREATE_CALLBACK&& CreateCallBack) noexcept
        {
        TRY_AGAIN_GET_OR_CREATE:
            if (find(Key, GetCallback))
                return true;

            if (alloc(Key, CreateCallBack))
                goto TRY_AGAIN_GET_OR_CREATE;

            // if we can not find it then create it
            return false;
        }

        template< typename T_CALLBACK = void(*)(value&)> xforceinline constexpr
        bool getExlusive(key Key, T_CALLBACK&& CallBack = [](value&) constexpr noexcept {}) noexcept
        {
            return find(Key, CallBack);
        }

        /*
        xforceinline constexpr
        value get(key Key) noexcept
        {
            value Val;
            bool b = find(Key, [&]const value& V) { Val = V; });
            xassert(b);
            return Val;
        }
        */

        xforceinline constexpr
        value get(key Key) const noexcept
        {
            value Val;
            bool b = find(Key, [&](const value& V) { Val = V; });
            xassert(b);
            return Val;
        }

        template< typename T = void(*)(const value&)>  xforceinline constexpr
        bool free( key Key, T&& CallBack = [](const value&) constexpr noexcept {} ) noexcept
        {
            xassert(Key.isValid());

            const auto                       RowTrueIndex    = Key.m_Value % max_size_v;
            auto&                            Row             = m_Map[RowTrueIndex];
            std::atomic<entry_magic_offset>* pLast           = nullptr;

            xcore::lock::scope Lk( Row.m_iHeadEntryOrEmpty );
            
            for( auto i = Row.m_iHeadEntryOrEmpty.get(); i != end_linklist_v;  )
            {
                auto& E = m_Map[(RowTrueIndex + i) % max_size_v].m_Entry;

                if( E.m_Key == Key )
                {
                    CallBack( std::as_const(E.m_Value) );

                    if( pLast ) pLast->store( E.m_iNextEntryOrEmpty.load(std::memory_order_relaxed), std::memory_order_relaxed );
                    else        Row.m_iHeadEntryOrEmpty.get() = E.m_iNextEntryOrEmpty.load(std::memory_order_relaxed);

                    E.m_iNextEntryOrEmpty.store( unused_entry_v, std::memory_order_relaxed) ;
                    return true;
                }

                i     = E.m_iNextEntryOrEmpty.load(std::memory_order_relaxed);
                pLast = &E.m_iNextEntryOrEmpty;
            }

            return false;
        }

        xcore::unique_span<row>    m_Map{};
    };
}