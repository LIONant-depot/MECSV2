
namespace mecs::archetype
{
    //-----------------------------------------------------------------------------------------

    bool details::safety::LockQueryComponents( mecs::system::instance& System, mecs::archetype::query::instance& Query ) noexcept
    {
        bool bErrors = false;
        
        xassert(Query.m_lResults.size());

        //
        // Lock components
        //
        for( const auto& E : Query.m_lResults )
        {
            auto& Archetype = const_cast<mecs::archetype::instance&>(*E.m_pArchetype);

            xcore::lock::scope Lk( Archetype.m_Safety );
            auto& Safety    = Archetype.m_Safety.get();

            //
            // Check for any kind of error first
            //
            int i = 0;
            auto& EntityDesc = *Archetype.m_Descriptor.m_DataDescriptorSpan[0];
            for( auto& [ Desc, isConstant ] : E.m_FunctionDescriptors )
            {
                // If we have a conditional component and we have nothing to do with it then we just continue
                if( E.m_lFunctionToArchetype[i++].m_Index == mecs::archetype::query::result_entry::invalid_index )
                {
                    continue;
                }

                // Check to make sure that if this archetype is already completely locked because someone is editing the entity 
                if( auto& Entry = Safety.m_lComponentLocks[EntityDesc.m_BitNumber]; Entry.m_pWritingSystem )
                {
                    mecs::graph::lock_error Error;
                    Error.m_lSystemGuids[0] = Entry.m_pWritingSystem->m_Guid;
                    Error.m_lSystemGuids[1] = System.m_Guid;
                    Error.m_pMessage        = "System trying to lock components, but another system has locked the entire archetype because it locked the entity as write";
                    xcore::lock::scope Lk(System.m_World.m_GraphDB.m_LockErrors);
                    System.m_World.m_GraphDB.m_LockErrors.get().push_back(Error);
                    xassert(false);
                    return true;
                }

                // We are trying to lock the entire archetype doe to access the entity for writing so we must make sure not one is accessing this archetype
                if( Desc.m_Guid == mecs::component::entity::type_guid_v && isConstant == false )
                {
                    xcore::lock::scope Lk(System.m_World.m_GraphDB.m_LockErrors);
                    for( auto& Entry : Safety.m_lComponentLocks )
                    {
                        if( Entry.m_pWritingSystem || Entry.m_nReaders )
                        {
                            mecs::graph::lock_error Error;
                            Error.m_lSystemGuids[0] = System.m_Guid;
                            if(Entry.m_pWritingSystem)
                            {
                                Error.m_lSystemGuids[1] = Entry.m_pWritingSystem->m_Guid;
                                Error.m_pMessage        = "System trying to lock the entire archetype due to locking the entity as write, another existing system already has a write lock as one of the components";
                                System.m_World.m_GraphDB.m_LockErrors.get().push_back(Error);
                            }
                            else
                            {
                                Error.m_lSystemGuids[1].setNull();
                                Error.m_pMessage        = "System trying to lock the entire archetype due to locking the entity as write, but another systems has components locked for reading";
                                System.m_World.m_GraphDB.m_LockErrors.get().push_back(Error);
                            }
                        }
                    }

                    if (System.m_World.m_GraphDB.m_LockErrors.get().size()) 
                    {
                        xassert(false);
                        return true;
                    }
                }

                //
                // Deal the data access types
                //
                if (Desc.m_DataAccess == mecs::component::type_data_access::QUANTUM || Desc.m_DataAccess == mecs::component::type_data_access::QUANTUM_DOUBLE_BUFFER)
                    continue;

                if( Desc.m_DataAccess == mecs::component::type_data_access::LINEAR )
                {
                    // If we are trying to write a component it better not have anyone reading or writing
                    if(auto& Entry = Safety.m_lComponentLocks[Desc.m_BitNumber]; isConstant == false && (Entry.m_pWritingSystem || Entry.m_nReaders) )
                    {
                        xcore::lock::scope Lk(System.m_World.m_GraphDB.m_LockErrors);
                        mecs::graph::lock_error Error;
                        Error.m_lSystemGuids[0] = System.m_Guid;
                        if (Entry.m_pWritingSystem)
                        {
                            Error.m_lSystemGuids[1] = Entry.m_pWritingSystem->m_Guid;
                            Error.m_pMessage = "System trying to lock a component for writing but another system has the component already locked for writing";
                            System.m_World.m_GraphDB.m_LockErrors.get().push_back(Error);
                        }
                        else
                        {
                            Error.m_lSystemGuids[1].setNull();
                            Error.m_pMessage = "System trying to lock a component for writing but another system has the component already locked for reading";
                            System.m_World.m_GraphDB.m_LockErrors.get().push_back(Error);
                        }
                        xassert(false);
                        return true;
                    }
                }
                else
                {
                    xassert(Desc.m_DataAccess == mecs::component::type_data_access::DOUBLE_BUFFER);

                    // If we are trying to write a component it better not have anyone reading or writing
                    if (auto& Entry = Safety.m_lComponentLocks[Desc.m_BitNumber]; isConstant == false && Entry.m_pWritingSystem )
                    {
                        xcore::lock::scope Lk(System.m_World.m_GraphDB.m_LockErrors);
                        mecs::graph::lock_error Error;
                        Error.m_lSystemGuids[0] = System.m_Guid;
                        Error.m_lSystemGuids[1] = Entry.m_pWritingSystem->m_Guid;
                        Error.m_pMessage = "System trying to lock a component for writing but another system has the component already locked for writing (even when is double buffer)";
                        System.m_World.m_GraphDB.m_LockErrors.get().push_back(Error);
                        xassert(false);
                        return true;
                    }
                }
            }

            //
            // We got here there are not errors so lets just lock the components
            //
            i = 0;
            for (auto& [Desc, isConstant] : E.m_FunctionDescriptors)
            {
                if (E.m_lFunctionToArchetype[i++].m_Index == mecs::archetype::query::result_entry::invalid_index)
                    continue;

                auto& Entry = Safety.m_lComponentLocks[Desc.m_BitNumber];
                xassert(Desc.m_DataAccess == mecs::component::type_data_access::QUANTUM 
                     || Desc.m_DataAccess == mecs::component::type_data_access::QUANTUM_DOUBLE_BUFFER
                     || Desc.m_DataAccess == mecs::component::type_data_access::DOUBLE_BUFFER
                     || Entry.m_pWritingSystem == nullptr );

                if(isConstant)
                {
                    Entry.m_nReaders++;
                }
                else
                {
                    xassert(Desc.m_DataAccess == mecs::component::type_data_access::QUANTUM
                         || Desc.m_DataAccess == mecs::component::type_data_access::QUANTUM_DOUBLE_BUFFER
                         || Desc.m_DataAccess == mecs::component::type_data_access::DOUBLE_BUFFER
                         || Entry.m_nReaders == 0 );
                    Entry.m_pWritingSystem = &System;
                }
            }
        }

        return false;
    }

    //-----------------------------------------------------------------------------------------

    bool details::safety::UnlockQueryComponents( mecs::system::instance& System, mecs::archetype::query::instance& Query ) noexcept
    {
        for (const auto& E : Query.m_lResults)
        {
            auto& Archetype = const_cast<mecs::archetype::instance&>(*E.m_pArchetype);

            xcore::lock::scope Lk(Archetype.m_Safety);
            auto& Safety = Archetype.m_Safety.get();

            for (auto& [Desc, isConstant] : E.m_FunctionDescriptors)
            {
                auto& Entry = Safety.m_lComponentLocks[Desc.m_BitNumber];
                if (isConstant)
                {
                    xassert(Entry.m_nReaders);
                    Entry.m_nReaders--;
                }
                else
                {
                    xassert(Entry.m_pWritingSystem == &System);
                    Entry.m_pWritingSystem = nullptr ;
                }
            }
        }
        return false;
    }

}