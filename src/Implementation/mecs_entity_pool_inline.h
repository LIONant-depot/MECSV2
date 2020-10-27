namespace mecs::entity_pool
{
    //-------------------------------------------------------------------------------------------

    instance::~instance() noexcept
    {
        Kill();
    }

    //-------------------------------------------------------------------------------------------

    void instance::Init(mecs::archetype::instance& Archetype, const std::span<const mecs::component::descriptor* const> Descriptors, std::uint32_t MaxEntries ) noexcept
    {
        xassert(Descriptors.size());
       // xassert(Descriptors[0] == &mecs::component::descriptor_v<component::entity> );

        m_MaxEntries    = MaxEntries;
        m_pArchetype    = &Archetype;
        m_Descriptors   = Descriptors;

        //
        // Reserve the memory and set all the types
        //
        int i = 0;
        for( auto pE : m_Descriptors )
        {
            std::size_t PageCount = (m_MaxEntries * pE->m_Size) / page_size_v;
            PageCount = ((PageCount * page_size_v) < (m_MaxEntries * pE->m_Size)) ? PageCount + 1 : PageCount;
            xassert((PageCount * page_size_v) >= (m_MaxEntries * pE->m_Size));

            m_Pointers[i++] = static_cast<std::byte*>(VirtualAlloc(nullptr, PageCount * page_size_v, MEM_RESERVE, PAGE_NOACCESS));
        }

        //
        // Reserve memory for the delete entries
        //
        m_DeleteList.Init(MaxEntries);
    }

    //-------------------------------------------------------------------------------------------

    void instance::Kill( void ) noexcept
    {
        clear();
        for ( std::uint32_t i = 0; i < m_Descriptors.size(); ++i)
        {
            VirtualFree( m_Pointers[i], 0, MEM_RELEASE );
        }
    }

    //-------------------------------------------------------------------------------------------

    void instance::clear(void) noexcept
    {
        xassert( m_NewCount.load().m_Count == m_Count);
        if( m_Count >= 10000 )
        {
            // Clear in quantum space
            xcore::scheduler::channel Channel( xconst_universal_str("mecs::component::entity_pool::clear") );

            int Index = 0;
            for( auto pE : m_Descriptors )
            {
                if(pE->m_fnDestruct ) Channel.SubmitJob([Count = m_Count, p = m_Pointers[Index], pE ]
                {
                    const auto  s = pE->m_Size;
                    const auto  f = *pE->m_fnDestruct;

                    for (std::uint32_t i = 0; i < Count; ++i)
                    {
                        f(p + s * i);
                    }
                });

                Index++;
            }
            Channel.join();
        }
        else
        {
            // Do it in linear space
            int Index = 0;
            for( auto pE : m_Descriptors )
            {
                if( pE->m_fnDestruct )
                {
                    auto        p = m_Pointers[Index];
                    const auto  s = pE->m_Size;
                    const auto  f = pE->m_fnDestruct;

                    for (std::uint32_t i = 0; i < m_Count; ++i)
                    {
                        f(p + s * i);
                    }
                }
                Index++;
            }
        }

        m_Count = 0;
    }

    //-------------------------------------------------------------------------------------------

    std::uint32_t instance::append( const int nEntries ) noexcept
    {
        using               offset_array    = std::array< std::uint32_t, mecs::settings::max_data_components_per_entity >;

        // Quick reject if we can't fit that many
        if( (nEntries + size()) > capacity() )
            return ~0;

        xassert(nEntries>0);

        counter             NewCount;
        std::uint64_t       Types;              static_assert( mecs::settings::max_data_components_per_entity <= 64 );
        offset_array        Offsets;

        //
        // Compute the Offsets, and bit Types that need to be allocated and determine if we need to lock
        //
        do
        {
            counter CaptureCount = m_NewCount.load(std::memory_order_relaxed);

            if (CaptureCount.m_Lock)
            {
                XCORE_PERF_ZONE_SCOPED_NC("mecs::component::entity_pool::append Lock is Spinning and waiting", tracy::Color::ColorType::Red);
                do
                {
                    CaptureCount = m_NewCount.load(std::memory_order_relaxed);
                } while (CaptureCount.m_Lock);
            }

            Types = 0;
            int Index = 0;
            for( auto pE : m_Descriptors )
            {
                const auto  ByteOffset  = CaptureCount.m_Count * pE->m_Size;
                const auto  k           = (ByteOffset + nEntries*pE->m_Size) >> page_size_pow_v;
                if (CaptureCount.m_Count == 0 || k != (ByteOffset >> page_size_pow_v))
                {
                    const auto i = Index;
                    Offsets[i]   = static_cast<std::uint32_t>(k);
                    Types       |= (1ull << i);
                }
                Index++;
            }

            NewCount.m_Count = CaptureCount.m_Count + nEntries;
            NewCount.m_Lock  = !!Types;
            xassert(NewCount.m_Count <= capacity() );

            if (m_NewCount.compare_exchange_strong(CaptureCount, NewCount)) break;

        } while (true);

        //
        // Allocate pages if we have to
        //
        const auto Count = NewCount.m_Count - nEntries;
        if (Types)
        {
            xassert(NewCount.m_Lock);

            if (nEntries == 1) for( auto end = m_Descriptors.size(), i=0ull; i < end; ++i)
            {
                if( Types & (1ull << i) )
                {
                    const auto  pE           = m_Pointers[i];
                    const auto  pPageAddress = &pE[ Offsets[i] * page_size_v ];
                    const auto  p            = VirtualAlloc(pPageAddress, page_size_v, MEM_COMMIT, PAGE_READWRITE);
                    xassert(p == pPageAddress);
                }
            }
            else for (auto end = m_Descriptors.size(), i = 0ull; i < end; ++i)
            {
                if (Types & (1ull << i))
                {
                    const auto  pE                  = m_Pointers[i];
                    const auto  pLastPageAddress    = &pE[Offsets[i] * page_size_v];
                    const auto  pNextPageAddress    = xcore::bits::Align( &pE[Count*m_Descriptors[i]->m_Size], page_size_v );
                    const auto  NewBytes            = static_cast<std::size_t>(pLastPageAddress - pNextPageAddress);
                    const auto  p                   = VirtualAlloc(pNextPageAddress, NewBytes + page_size_v, MEM_COMMIT, PAGE_READWRITE);

                    /*
                    const auto  nNewPages    = [&]() noexcept
                    {
                        const auto pNewAddress = xcore::bits::Align(pE + m_Descriptors[i]->m_Size * (Count + nEntries), page_size_v);
                        return static_cast<std::uint32_t>((pNewAddress - pE) / page_size_v) - Offsets[i];
                    }();
                    const auto  p = VirtualAlloc(pPageAddress, nNewPages * page_size_v, MEM_COMMIT, PAGE_READWRITE);
                    */
                    xassert(p == pNextPageAddress);
                }
            }

            // Release the lock
            NewCount.m_Lock = 0;
            m_NewCount.store(NewCount, std::memory_order_relaxed);
        }

        //
        // Call the constructor if we need to
        //
        if (nEntries == 1)
        {
            int Index=0;
            for( auto pE : m_Descriptors )
            {
                auto pFnConstruct = pE->m_fnConstruct;
                if(pFnConstruct)
                {
                    (*pFnConstruct)(m_Pointers[Index] + pE->m_Size * Count);
                }
                Index++;
            }
        }
        else
        {
            int Index = 0;
            for (auto pE : m_Descriptors)
            {
                auto pFnConstruct = pE->m_fnConstruct;
                if (pFnConstruct)
                {
                    const   auto s      = pE->m_Size;
                            auto p      = m_Pointers[Index] + s * Count;
                    const   auto end    = p + s * nEntries;
                    for (; p != end; p += s)
                        (*pFnConstruct)(p);
                }
                Index++;
            }
        }

        return Count;
    }

    //-------------------------------------------------------------------------------------------

    void instance::MemoryBarrier( void ) noexcept
    {
        auto CaptureNewCount = m_NewCount.load(std::memory_order_relaxed);

        xassert(CaptureNewCount.m_Lock == 0);

        // Update the count
        m_Count = CaptureNewCount.m_Count;

        // Delete entries if any
        if( m_DeleteList.size() )
        {
            // Delete Entities
            DeletePendingEntries();

            // Reset the new count
            CaptureNewCount.m_Count = m_Count;
            m_NewCount.store(CaptureNewCount, std::memory_order_relaxed);
        }
    }

    //-------------------------------------------------------------------------------------------

    void instance::deleteBySwap(const std::uint32_t Index) noexcept
    {
        xassert(Index >= 0);
        m_DeleteList.append(static_cast<std::uint32_t>(Index));
    }

    //-------------------------------------------------------------------------------------------

    template< typename T_COMPONENT >
    T_COMPONENT& instance::get(const std::uint32_t Index) noexcept
    {
        xassert(Index >= 0);
        xassert(Index < m_Count);

        for (auto& E : m_Descriptors)
        {
            if (mecs::component::descriptor_v < T_COMPONENT>.m_Guid.m_Value == E.m_pDescriptor->m_Guid)
            {
                if constexpr (mecs::component::descriptor_v<T_COMPONENT>.m_Type == mecs::component::type::SINGLETON)
                {
                    return **reinterpret_cast<std::unique_ptr<T_COMPONENT>*>(E.m_pPointer + Index * E.m_pDescriptor->m_Size);
                }
                else
                {
                    return *reinterpret_cast<T_COMPONENT*>(E.m_pPointer + Index * E.m_pDescriptor->m_Size);
                }
            }
        }

        xassume(false);
    }

    //-------------------------------------------------------------------------------------------

    template< typename T_COMPONENT, bool T_DO_CONVERSIONS_V > xforceinline
    T_COMPONENT& instance::getComponentByIndex ( const index Index, int iComponentIndex ) noexcept
    {
        xassert(mecs::component::descriptor_v<T_COMPONENT>.m_Guid == m_Descriptors[iComponentIndex]->m_Guid );

        auto p = m_Pointers[iComponentIndex] + Index * mecs::component::descriptor_v<T_COMPONENT>.m_Size;
        if constexpr (T_DO_CONVERSIONS_V && mecs::component::descriptor_v<T_COMPONENT>.m_Type == mecs::component::type::SINGLETON )
        {
            return **reinterpret_cast<std::unique_ptr<T_COMPONENT>*>(p);
        }
        else
        {
            return *reinterpret_cast<T_COMPONENT*>(p);
        }
    }

    //-------------------------------------------------------------------------------------------

    template< typename T_COMPONENT, bool T_DO_CONVERSIONS_V > xforceinline
    const T_COMPONENT& instance::getComponentByIndex ( const index Index, int iComponentIndex ) const noexcept
    {
        xassert(mecs::component::descriptor_v<T_COMPONENT>.m_Guid == m_Descriptors[iComponentIndex]->m_Guid);

        auto p = m_Pointers[iComponentIndex] + Index * mecs::component::descriptor_v<T_COMPONENT>.m_Size;
        if constexpr (T_DO_CONVERSIONS_V && mecs::component::descriptor_v<T_COMPONENT>.m_Type == mecs::component::type::SINGLETON)
        {
            return **reinterpret_cast<std::unique_ptr<const T_COMPONENT>*>(p);
        }
        else
        {
            return *reinterpret_cast<const T_COMPONENT*>(p);
        }
    }

    //-------------------------------------------------------------------------------------------
    xforceinline
    std::byte* instance::getComponentByIndexRaw( const index Index, int iComponentIndex) noexcept
    {
        const auto& Desc = *m_Descriptors[iComponentIndex];
        std::byte*  p    = m_Pointers[iComponentIndex] + Index * Desc.m_Size;

        if( Desc.m_Type == mecs::component::type::SINGLETON )
        {
            return reinterpret_cast<std::unique_ptr<std::byte>&>(*p).get();
        }
        else
        {
            return p;
        }
    }

    //-------------------------------------------------------------------------------------------

    template< typename T_COMPONENT > constexpr
    const T_COMPONENT& instance::get(const std::uint32_t Index) const noexcept
    {
        xassert(Index >= 0);
        xassert(Index < m_Count);

        for (auto& E : m_Descriptors)
        {
            if constexpr (mecs::component::descriptor_v<T_COMPONENT>.m_Type == mecs::component::type::SINGLETON)
            {
                return **reinterpret_cast<std::unique_ptr<T_COMPONENT>*>(E.m_pPointer + Index * E.m_pDescriptor->m_Size);
            }
            else
            {
                return *reinterpret_cast<T_COMPONENT*>(E.m_pPointer + Index * E.m_pDescriptor->m_Size);
            }
        }

        xassume(false);
    }

    //-------------------------------------------------------------------------------------------

    void instance::DeletePendingEntries( void ) noexcept
    {
        XCORE_PERF_ZONE_SCOPED_N("mecs::component::entity_pool::DeletePendingEntries")
        std::uint32_t   nDelete     = m_DeleteList.size();
        const auto      WaterMark   = m_Count - nDelete;

        // Are we erasing everything?
        if( WaterMark == 0 )
        {
            int Index = 0;
            for( auto pE : m_Descriptors )
            {
                if(pE->m_fnDestruct == nullptr ) continue;

                auto p = m_Pointers[Index];
                for (std::uint32_t i = 0; i < m_Count; ++i)
                {
                    pE->m_fnDestruct( p + i * pE->m_Size );
                }
                Index++;
            }
        }
        else
        {
            std::uint32_t   TopIndex            = m_Count;
            const auto      ProcessFromTheTop   = [&]() noexcept
            {
                do
                {
                    xassert(TopIndex >= WaterMark);

                    TopIndex--;

                    auto& D      = *m_Descriptors[0];
                    auto& Entity = *reinterpret_cast<component::entity*>( m_Pointers[0] + TopIndex * D.m_Size );

                    if( false == Entity.isZombie() ) break;

                    // Delete the entity
                    int Index = 0;
                    for( auto pE : m_Descriptors )
                    {
                        if (pE->m_fnDestruct == nullptr) continue;

                        auto p = m_Pointers[Index];
                        pE->m_fnDestruct(p + TopIndex * pE->m_Size);
                        Index++;
                    }

                    --nDelete;

                } while (nDelete);
            };

            // Find the real top
            ProcessFromTheTop();

            for( auto E : m_DeleteList )
            {
                if( E >= TopIndex ) continue;

                if( nDelete-- == 0 ) break;
                //xassert( (static_cast<std::uint32_t>(TopIndex) * m_Descriptors GroupDescriptor.m_EntriesPerPage + TopIndex) >= WaterMark );

                // This entity is completely gone at this point
                xassert( reinterpret_cast<component::entity*>(m_Pointers[0] + E * m_Descriptors[0]->m_Size )->isZombie() );

                // Destroy and Move components to new location
                int Index = 0;
                for( auto pE : m_Descriptors )
                {
                    auto        Ptr   = m_Pointers[Index++];
                    const auto  Size  = pE->m_Size;
                    auto        pDest = Ptr + E        * Size;
                    auto        pSrc  = Ptr + TopIndex * Size;

                    xassert(pE->m_fnMove);
                    pE->m_fnMove( pDest, pSrc );
                }

                // TODO: Prefetch the reference to minimize cache hit
                // Update the moved entity reference
                {
                    auto& MovedEntity = *reinterpret_cast<component::entity*>(m_Pointers[0] + E * sizeof(component::entity));
                    xassert(MovedEntity.isZombie() == false );
                    auto& Ref         = MovedEntity.getMapEntry().m_Value;
                    Ref.m_Index       = E;
                }

                // Update the top count
                ProcessFromTheTop();
            }
        }

        // Clear all the list
        m_DeleteList.clear();

        // Set the new count of entries
        m_Count = WaterMark;
    }


}