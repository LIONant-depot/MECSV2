namespace mecs::archetype
{
    namespace details
    {
        constexpr bool entity_creation::isValid( void ) const noexcept { return m_pEntityMap; }

        template< typename T >
        void entity_creation::operator() ( T&& Callback ) noexcept
        {
            xassert(isValid());
            using fn_traits = xcore::function::traits<T>;
            Initialize( std::forward<T&&>(Callback), reinterpret_cast<typename fn_traits::args_tuple*>(nullptr) );
        }

        template< typename...T_COMPONENTS > xforceinline
        constexpr void entity_creation::getTrueComponentIndices( std::span<std::uint8_t> Array, std::span<const mecs::component::descriptor* const> Span, std::tuple<T_COMPONENTS...>* ) const noexcept
        {
            static constexpr std::array Inputs{ &mecs::component::descriptor_v<T_COMPONENTS> ... };
            static_assert( ((std::is_const_v<std::remove_pointer_t<std::remove_reference_t<T_COMPONENTS>>> == false) && ...) );
            int I = 0;
            for(std::uint8_t c=0, end = static_cast<std::uint8_t>(Span.size()); c<end; c++ )
            {
                auto& Desc = *Span[c];
                if( Desc.m_Guid == Inputs[I]->m_Guid )
                {
                    if( Desc.m_isDoubleBuffer )
                    {
                        const std::uint32_t state = (m_Archetype.m_DoubleBufferInfo.m_StateBits >> Desc.m_BitNumber) & 1;
                        const std::uint32_t iBase = (Span[c - 1]->m_Guid == Desc.m_Guid) ? c - 1 : c; 
                        Array[I] = c + 1 - state;
                    }
                    else
                    {
                        Array[I] = c;
                    }
                    if( I == static_cast<std::uint8_t>(sizeof...(T_COMPONENTS)-1) ) return;
                    I++;
                }
            }

            xassume(false);
        }

        template< typename T, typename...T_COMPONENTS > xforceinline
        void entity_creation::Initialize( T&& Callback, std::tuple<T_COMPONENTS...>* )
        {
            static_assert( ((mecs::component::descriptor_v<T_COMPONENTS>.m_Type != mecs::component::type::SHARE) && ...) );
            static_assert( ((mecs::component::descriptor_v<T_COMPONENTS>.m_Type != mecs::component::type::TAG)   && ...) );
            static_assert( ((std::is_same_v<T_COMPONENTS, xcore::types::decay_full_t<T_COMPONENTS>&>) && ...));

            using raw_tuple    = std::tuple<T_COMPONENTS...>;
            using sorted_tuple = xcore::types::tuple_sort_t< mecs::component::smaller_guid, raw_tuple >;

            std::array<std::uint8_t, sizeof...(T_COMPONENTS)> ComponentIndices;
            getTrueComponentIndices
            (
                std::span<std::uint8_t>                             { ComponentIndices }
            ,   std::span<const mecs::component::descriptor* const> { m_pPool->m_EntityPool.getDescriptors() }
            ,   reinterpret_cast<sorted_tuple*>(nullptr) 
            );
            static constexpr auto max_entries_single_thread = 10000;
            if(m_Guids.size()) 
            {
                if(m_Count <= max_entries_single_thread )
                {
                    for( int i = m_StartIndex, end = m_StartIndex + m_Count, g=0; i != end; ++i, ++g )
                    {
                        m_pEntityMap->alloc(m_Guids[g], [&]( mecs::component::entity::reference& Reference )
                        {
                            Reference.m_Index = i;
                            Reference.m_pPool = &m_pPool->m_EntityPool;
                            m_pPool->m_EntityPool.getComponentByIndex<mecs::component::entity>(i,0).m_pInstance = &m_pEntityMap->getEntryFromItsValue(Reference);
                            Callback(m_pPool->m_EntityPool.getComponentByIndex<xcore::types::decay_full_t<T_COMPONENTS>>(i, ComponentIndices[xcore::types::tuple_t2i_v<T_COMPONENTS, sorted_tuple>]) ...);
                            m_pPool->m_pInstance->m_Events.m_CreatedEntity.NotifyAll(m_pPool->m_EntityPool.getComponentByIndex<mecs::component::entity>(i,0), m_System );
                        });
                    }
                }
                else
                {
                    xcore::scheduler::channel Channel( xconst_universal_str("entity_creation::Initialize+g"));
                    for (int i = m_StartIndex, end = m_StartIndex + m_Count, g = 0; i != end; )
                    {
                        int my_end = std::min<int>( end, i + max_entries_single_thread);
                        Channel.SubmitJob([this,&Callback,&ComponentIndices, gStart=g, iStart=i, my_end ]() constexpr noexcept
                        {
                            for( auto i=iStart,g=gStart; i != my_end; ++i, ++g ) m_pEntityMap->alloc(m_Guids[g], [&](mecs::component::entity::reference& Reference)
                            {
                                Reference.m_Index = i;
                                Reference.m_pPool = &m_pPool->m_EntityPool;
                                m_pPool->m_EntityPool.getComponentByIndex<mecs::component::entity>(i, 0).m_pInstance = &m_pEntityMap->getEntryFromItsValue(Reference);
                                Callback(m_pPool->m_EntityPool.getComponentByIndex<xcore::types::decay_full_t<T_COMPONENTS>>(i, ComponentIndices[xcore::types::tuple_t2i_v<T_COMPONENTS, sorted_tuple>]) ...);
                                m_pPool->m_pInstance->m_Events.m_CreatedEntity.NotifyAll(m_pPool->m_EntityPool.getComponentByIndex<mecs::component::entity>(i, 0), m_System);
                            });
                        });

                        g += my_end - i;
                        i = my_end;
                    }
                    Channel.join();
                }
            }
            else 
            {
                if (m_Count <= max_entries_single_thread)
                {
                    for (int i = m_StartIndex, end = m_StartIndex + m_Count; i != end; ++i)
                    {
                        m_pEntityMap->alloc(mecs::component::entity::guid{ xcore::not_null }, [&](mecs::component::entity::reference& Reference)
                        {
                            Reference.m_Index = i;
                            Reference.m_pPool = &m_pPool->m_EntityPool;
                            m_pPool->m_EntityPool.getComponentByIndex<mecs::component::entity>(i, 0).m_pInstance = &m_pEntityMap->getEntryFromItsValue(Reference);
                            Callback(m_pPool->m_EntityPool.getComponentByIndex< xcore::types::decay_full_t<T_COMPONENTS>>(i, ComponentIndices[xcore::types::tuple_t2i_v<T_COMPONENTS, sorted_tuple>]) ...);
                            m_pPool->m_pInstance->m_Events.m_CreatedEntity.NotifyAll(m_pPool->m_EntityPool.getComponentByIndex<mecs::component::entity>(i, 0), m_System );
                        });
                    }
                }
                else
                {
                    xcore::scheduler::channel Channel( xconst_universal_str("entity_creation::Initialize"));
                    for (int i = m_StartIndex, end = m_StartIndex + m_Count; i != end; )
                    {
                        int my_end = std::min<int>( end, i + max_entries_single_thread);
                        Channel.SubmitJob([this,&Callback,&ComponentIndices, iStart=i, my_end ] () constexpr noexcept
                        {
                            xassert(iStart< my_end);
                            for( auto i=iStart; i != my_end; ++i ) m_pEntityMap->alloc(mecs::component::entity::guid{ xcore::not_null }, [&](mecs::component::entity::reference& Reference)
                            {
                                Reference.m_Index = i;
                                Reference.m_pPool = &m_pPool->m_EntityPool;
                                m_pPool->m_EntityPool.getComponentByIndex<mecs::component::entity>(i, 0).m_pInstance = &m_pEntityMap->getEntryFromItsValue(Reference);
                                Callback(m_pPool->m_EntityPool.getComponentByIndex<xcore::types::decay_full_t<T_COMPONENTS>>(i, ComponentIndices[xcore::types::tuple_t2i_v<T_COMPONENTS, sorted_tuple>]) ...);
                                m_pPool->m_pInstance->m_Events.m_CreatedEntity.NotifyAll(m_pPool->m_EntityPool.getComponentByIndex<mecs::component::entity>(i, 0), m_System);
                            });
                        });

                        i = my_end;
                    }
                    Channel.join();
                }
            }
        }
    }

}