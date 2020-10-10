
namespace mecs::archetype
{
    //--------------------------------------------------------------------------------------------------------------------
    inline
    bool CheckWriteComponentOnFullEntityLocked( details::safety& Safety, int ComponentID, bool isConstant, mecs::system::instance& System, mecs::archetype::instance& Archetype )
    {
        for( auto& PerSystem : Safety.m_PerSystem )
        {
            // If we are dealing with the same system then we can skip it event though it could be a bit dangerous...
            // Because one lock could happen because of a lock could be a column lock (foreach) the other one could be an individual lock (getComponent)
            // Which if the user is not careful it could hurt himself...
            if( PerSystem.m_pSystem == &System ) continue;

            if( PerSystem.m_lComponentLocks[0].m_nWriters )
            {
                if( ComponentID == 0 )
                {
                    mecs::graph::lock_error Error;
                    Error.m_pSystems[0] = &System;
                    Error.m_pSystems[1] = PerSystem.m_pSystem;
                    Error.m_pArchetype  = &Archetype;
                    xcore::lock::scope Lk( System.m_World.m_GraphDB.m_LockErrors );
                    System.m_World.m_GraphDB.m_LockErrors.get().push_back(Error);

                    if( isConstant == false )
                        Error.m_pMessage = "System trying to lock the entity component, but another system has locked the entire archetype because it locked the component entity as write";
                    else
                        Error.m_pMessage = "System trying to read the entity component, but another system has locked the entire archetype because it locked the component entity as write";
                    xassert(false);
                    return true;
                }
                else
                {
                    mecs::graph::lock_error Error;
                    Error.m_pSystems[0] = &System;
                    Error.m_pSystems[1] = PerSystem.m_pSystem;
                    Error.m_pArchetype  = &Archetype;
                    Error.m_pMessage    = "System trying to lock components, but another system has locked the entire archetype because it locked the component entity as write";
                    xcore::lock::scope Lk( System.m_World.m_GraphDB.m_LockErrors );
                    System.m_World.m_GraphDB.m_LockErrors.get().push_back(Error);
                    xassert(false);
                    return true;
                }
            }

            // Trying to lock the entity while is been used by other system
            if( isConstant == false && ComponentID == 0 )
            {
                mecs::graph::lock_error Error;
                Error.m_pSystems[0] = &System;
                Error.m_pSystems[1] = PerSystem.m_pSystem;
                Error.m_pArchetype  = &Archetype;
                xcore::lock::scope Lk( System.m_World.m_GraphDB.m_LockErrors );
                System.m_World.m_GraphDB.m_LockErrors.get().push_back(Error);

                if( PerSystem.m_lComponentLocks[0].m_nWriters )
                {
                    Error.m_pMessage = "System trying to lock the entity component for writing while another system already have it locked it for writing";
                }
                else if (PerSystem.m_lComponentLocks[0].m_nReaders )
                {
                    Error.m_pMessage = "System trying to lock the entity component for writing while another system already have it locked it for reading";
                }
                else
                {
                    Error.m_pMessage = "System trying to lock the entity component for writing while another system already is accessing some of its components";
                }
                xassert(false);
                return true;
            }

            // Share writter already locked the entity
            if( PerSystem.m_nShareWriters )
            {
                mecs::graph::lock_error Error;
                Error.m_pSystems[0] = &System;
                Error.m_pSystems[1] = PerSystem.m_pSystem;
                Error.m_pArchetype  = &Archetype;
                xcore::lock::scope Lk( System.m_World.m_GraphDB.m_LockErrors );
                System.m_World.m_GraphDB.m_LockErrors.get().push_back(Error);

                if( ComponentID == 0 )
                    Error.m_pMessage = "System trying to lock the entity component for writing while another system has the entire entity locked due to writing to a share component";
                else
                    Error.m_pMessage = "System trying to access a component while another system has the entire entity locked due to writing to a share component";

                xassert(false);
                return true;
            }
        }

        return false;
    }

    //--------------------------------------------------------------------------------------------------------------------

    inline
    bool CheckShareWriteComponent( details::safety& Safety, int ShareComponentID, bool isConstant, mecs::system::instance& System, mecs::archetype::instance& Archetype )
    {
        if(isConstant) return false;
        for( auto& PerSystem : Safety.m_PerSystem )
        {
            // If we are dealing with the same system then we can skip it event though it could be a bit dangerous...
            // Because one lock could happen because of a lock could be a column lock (foreach) the other one could be an individual lock (getComponent)
            // Which if the user is not careful it could hurt himself...
            if( PerSystem.m_pSystem == &System ) 
            {
                // TODO: We should really detect if it is the care of a foreach vs a getComponent case this would definitely create a problem
                continue;
            }

            mecs::graph::lock_error Error;
            Error.m_pSystems[0] = &System;
            Error.m_pSystems[1] = PerSystem.m_pSystem;
            Error.m_pArchetype  = &Archetype;
            Error.m_pMessage    = "System trying to lock the entire entity due to a share component write but another system is already accessing some of its components";
            xcore::lock::scope Lk( System.m_World.m_GraphDB.m_LockErrors );
            System.m_World.m_GraphDB.m_LockErrors.get().push_back(Error);
            xassert(false);
            return true;
        }
        return false;
    }

    //--------------------------------------------------------------------------------------------------------------------

    inline
    bool CheckLinearComponent( details::safety& Safety, int ComponentID, bool isConstant, mecs::system::instance& System, mecs::archetype::instance& Archetype )
    {
        for( auto& PerSystem : Safety.m_PerSystem )
        {
            if (PerSystem.m_pSystem == &System) continue;

            if( isConstant )
            {
                if( PerSystem.m_lComponentLocks[ComponentID].m_nWriters )
                {
                    mecs::graph::lock_error Error;
                    Error.m_pSystems[0] = &System;
                    Error.m_pSystems[1] = PerSystem.m_pSystem;
                    Error.m_pArchetype  = &Archetype;
                    Error.m_pMessage =   "System trying to lock a linear component for reading but another system already have a writing lock to it";
                    xcore::lock::scope Lk(System.m_World.m_GraphDB.m_LockErrors);
                    System.m_World.m_GraphDB.m_LockErrors.get().push_back(Error);
                    xassert(false);
                    return true;
                }
            }
            else
            {
                if( PerSystem.m_lComponentLocks[ComponentID].m_nWriters || PerSystem.m_lComponentLocks[ComponentID].m_nReaders )
                {
                    mecs::graph::lock_error Error;
                    Error.m_pSystems[0] = &System;
                    Error.m_pSystems[1] = PerSystem.m_pSystem;
                    Error.m_pArchetype  = &Archetype;
                    Error.m_pMessage =   "System trying to lock a linear component for writing but another system already locked it";
                    xcore::lock::scope Lk(System.m_World.m_GraphDB.m_LockErrors);
                    System.m_World.m_GraphDB.m_LockErrors.get().push_back(Error);

                    if( PerSystem.m_lComponentLocks[ComponentID].m_nWriters && PerSystem.m_lComponentLocks[ComponentID].m_nReaders )
                        Error.m_pMessage = "System trying to lock a linear component for writing but another system already locked it for writing and reading";
                    else if( PerSystem.m_lComponentLocks[ComponentID].m_nWriters )
                        Error.m_pMessage = "System trying to lock a linear component for writing but another system already locked it for writing";
                    else
                        Error.m_pMessage = "System trying to lock a linear component for writing but another system already locked it for reading";
                    xassert(false);
                    return true;
                }
            }
        }
        return false;
    }

    //--------------------------------------------------------------------------------------------------------------------

    inline
    bool CheckDoubleBufferComponent( details::safety& Safety, int ComponentID, bool isConstant, mecs::system::instance& System, mecs::archetype::instance& Archetype )
    {
        if(isConstant) return false;

        for( auto& PerSystem : Safety.m_PerSystem )
        {
            if (PerSystem.m_pSystem == &System) continue;

            if( PerSystem.m_lComponentLocks[ComponentID].m_nWriters )
            {
                mecs::graph::lock_error Error;
                Error.m_pSystems[0] = &System;
                Error.m_pSystems[1] = PerSystem.m_pSystem;
                Error.m_pArchetype  = &Archetype;
                Error.m_pMessage   = "System trying to lock a linear double buffer component for writing but another system already locked it for writing";
                xcore::lock::scope Lk(System.m_World.m_GraphDB.m_LockErrors);
                System.m_World.m_GraphDB.m_LockErrors.get().push_back(Error);
                xassert(false);
                return true;
            }
        }
        return false;
    }

    //--------------------------------------------------------------------------------------------------------------------
    inline
    bool CheckForErrors( details::safety::per_system& PerSystem, const mecs::archetype::query::result_entry& E ) noexcept
    {
        auto& Archetype = *E.m_pArchetype;
        auto& Safety = Archetype.m_Safety.get(); // should already be locked by the other two functions

        //
        // Check for any kind of error first
        //
        int i = 0;
        auto& EntityDesc = *Archetype.m_Descriptor.m_DataDescriptorSpan[0];
        for (auto& [Desc, isConstant] : E.m_FunctionDescriptors)
        {
            // If we have a conditional component and we have nothing to do with it then we just continue
            if (E.m_lFunctionToArchetype[i++].m_Index == mecs::archetype::query::result_entry::invalid_index)
            {
                continue;
            }

            // Check to make sure that if this archetype is already completely locked because someone is editing the entity
            // Of if we are trying to lock the entity component and there is someone already accessing it
            if (CheckWriteComponentOnFullEntityLocked(Safety, EntityDesc.m_BitNumber, isConstant, *PerSystem.m_pSystem, Archetype)) return true;

            // Check when we are trying to write to a share component
            if (Desc.m_Type == mecs::component::type::SHARE)
            {
                if (CheckShareWriteComponent(Safety, EntityDesc.m_BitNumber, isConstant, *PerSystem.m_pSystem, Archetype)) return true;
            }

            //
            // Deal the data access types
            //
            if (Desc.m_DataAccess == mecs::component::type_data_access::QUANTUM
                || Desc.m_DataAccess == mecs::component::type_data_access::QUANTUM_DOUBLE_BUFFER)
                continue;

            if (Desc.m_DataAccess == mecs::component::type_data_access::LINEAR)
            {
                if (CheckLinearComponent(Safety, EntityDesc.m_BitNumber, isConstant, *PerSystem.m_pSystem, Archetype)) return true;
            }
            else
            {
                xassert(Desc.m_DataAccess == mecs::component::type_data_access::DOUBLE_BUFFER);
                if (CheckDoubleBufferComponent(Safety, EntityDesc.m_BitNumber, isConstant, *PerSystem.m_pSystem, Archetype)) return true;
            }
        }

        return false;
    }

    //--------------------------------------------------------------------------------------------------------------------

    bool details::safety::LockQueryComponents( per_system& PerSystem, const mecs::archetype::query::result_entry& E ) noexcept
    {
        //
        // Check for errors
        //
        if( CheckForErrors(PerSystem, E) )
            return true;

        //
        // Lock components
        //
        auto&   Archetype   = *E.m_pArchetype;
        auto&   Safety      = Archetype.m_Safety.get(); // should already be locked by the other two functions
        int     i           = 0;

        for( auto& [Desc, isConstant] : E.m_FunctionDescriptors )
        {
            if( E.m_lFunctionToArchetype[i++].m_Index == mecs::archetype::query::result_entry::invalid_index )
                continue;

            auto& Entry = PerSystem.m_lComponentLocks[Desc.m_BitNumber];
            xassert(Desc.m_DataAccess == mecs::component::type_data_access::QUANTUM 
                 || Desc.m_DataAccess == mecs::component::type_data_access::QUANTUM_DOUBLE_BUFFER
                 || Desc.m_DataAccess == mecs::component::type_data_access::DOUBLE_BUFFER
                 || Entry.m_nWriters == 0 );

            PerSystem.m_nTotalInformation++;

            if( isConstant )
            {
                Entry.m_nReaders++;
            }
            else
            {
                xassert(Desc.m_DataAccess == mecs::component::type_data_access::QUANTUM
                     || Desc.m_DataAccess == mecs::component::type_data_access::QUANTUM_DOUBLE_BUFFER
                     || Desc.m_DataAccess == mecs::component::type_data_access::DOUBLE_BUFFER
                     || Entry.m_nReaders == 0 );
                Entry.m_nWriters++;
                if( Desc.m_Type == mecs::component::type::SHARE ) PerSystem.m_nShareWriters++;
            }
        }

        return false;
    }

    //-----------------------------------------------------------------------------------------

    bool details::safety::LockQueryComponents(mecs::system::instance& System, const mecs::archetype::query::result_entry& E) noexcept
    {
        //
        // Lock components
        //
        auto& Archetype = const_cast<mecs::archetype::instance&>(*E.m_pArchetype);

        xcore::lock::scope Lk(Archetype.m_Safety);
        auto& Safety = Archetype.m_Safety.get();

        for( auto& PerSys : Safety.m_PerSystem )
        {
            if( PerSys.m_pSystem == &System )
            {
                if (LockQueryComponents(PerSys, E)) return true;
                return false;
            }
        }

        Safety.m_PerSystem.emplace_back();
        auto& PerSys = Safety.m_PerSystem.back();
        PerSys.m_pSystem = &System;
        if (LockQueryComponents(PerSys, E)) return true;

        return false;
    }

    //-----------------------------------------------------------------------------------------

    bool details::safety::LockQueryComponents( mecs::system::instance& System, const mecs::archetype::query::instance& Query ) noexcept
    {
        xassert(Query.m_lResults.size());

        //
        // Lock components
        //
        for( const auto& E : Query.m_lResults )
        {
            auto& Archetype = const_cast<mecs::archetype::instance&>(*E.m_pArchetype);

            xcore::lock::scope Lk(Archetype.m_Safety);
            auto& Safety = Archetype.m_Safety.get();

            bool bFound = false;
            for (auto& PerSys : Safety.m_PerSystem)
            {
                if (PerSys.m_pSystem == &System)
                {
                    if (LockQueryComponents(PerSys, E)) return true;
                    bFound = true;
                    break;
                }
            }

            if(bFound == false )
            {
                Safety.m_PerSystem.emplace_back();
                auto& PerSys = Safety.m_PerSystem.back();
                PerSys.m_pSystem = &System;
                if (LockQueryComponents(PerSys, E)) return true;
            }
        }

        return false;
    }


    //-----------------------------------------------------------------------------------------

    bool details::safety::UnlockQueryComponents( per_system& PerSystem, const mecs::archetype::query::result_entry& E ) noexcept
    {
        auto&   Archetype   = *E.m_pArchetype;
        auto&   Safety      = Archetype.m_Safety.get(); // should already be locked by the other two functions
        int     i           = 0;

        for( auto& [Desc, isConstant] : E.m_FunctionDescriptors )
        {
            if( E.m_lFunctionToArchetype[i++].m_Index == mecs::archetype::query::result_entry::invalid_index )
                continue;

            xassert(PerSystem.m_nTotalInformation);
            PerSystem.m_nTotalInformation--;

            auto& Entry = PerSystem.m_lComponentLocks[Desc.m_BitNumber];
            if( isConstant )
            {
                xassert(Entry.m_nReaders);
                Entry.m_nReaders--;
            }
            else
            {
                xassert(Entry.m_nWriters);
                Entry.m_nWriters--;
                if( Desc.m_Type == mecs::component::type::SHARE ) 
                {
                    xassert(PerSystem.m_nShareWriters);
                    PerSystem.m_nShareWriters--;
                }
            }
        }

        return false;
    }

    //-----------------------------------------------------------------------------------------


    bool details::safety::UnlockQueryComponents( system::instance& System, const mecs::archetype::query::result_entry& E ) noexcept
    {
        auto& Archetype = const_cast<mecs::archetype::instance&>(*E.m_pArchetype);

        xcore::lock::scope Lk(Archetype.m_Safety);
        auto& Safety = Archetype.m_Safety.get();
        
        for(int i = 0, end = static_cast<int>(Safety.m_PerSystem.size()); i<end; ++i )
        {
            auto& PerSys = Safety.m_PerSystem[i];
            if (PerSys.m_pSystem == &System)
            {
                UnlockQueryComponents(PerSys, E);

                if( PerSys.m_nTotalInformation == 0 )
                {
                    PerSys = Safety.m_PerSystem.back();
                    Safety.m_PerSystem.pop_back();
                }
                return false;
            }
        }

        xassert(false);
        return true;
    }

    //-----------------------------------------------------------------------------------------

    bool details::safety::UnlockQueryComponents( mecs::system::instance& System, const mecs::archetype::query::instance& Query ) noexcept
    {
        for (const auto& E : Query.m_lResults)
        {
            auto& Archetype = const_cast<mecs::archetype::instance&>(*E.m_pArchetype);

            xcore::lock::scope Lk(Archetype.m_Safety);
            auto& Safety = Archetype.m_Safety.get();

            for (int i = 0, end = static_cast<int>(Safety.m_PerSystem.size()); i < end; i++ )
            {
                auto& PerSys = Safety.m_PerSystem[i];
                if (PerSys.m_pSystem == &System)
                {
                    UnlockQueryComponents(PerSys, E);
                    if (PerSys.m_nTotalInformation == 0)
                    {
                        PerSys = Safety.m_PerSystem.back();
                        Safety.m_PerSystem.pop_back();
                    }
                    return false;
                }
            }
        }

        xassert(false);
        return true;
    }

}