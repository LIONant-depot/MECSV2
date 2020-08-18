namespace mecs::world
{
    using guid = xcore::guid::unit<64, struct world_tag >;

    struct instance
    {
        xforceinline instance( mecs::universe::instance& Universe ) noexcept : m_Universe{ Universe }, m_GraphDB{ *this, Universe }
        {
            m_ArchetypeDB.Init();
            m_GraphDB.Init();
        }

        void Start(bool bContinuousPlay = false)
        {
            //
            // Make sure that the scene is ready to go
            // So we are going to loop thought all the archetypes and tell them to get ready
            //
            for (const auto& TagE : std::span{ m_ArchetypeDB.m_lTagEntries.data(), m_ArchetypeDB.m_nTagEntries } )
            {
                xcore::lock::scope Lk( TagE.m_ArchetypeDB );
                for (auto& ArchetypeEntry : std::span{ TagE.m_ArchetypeDB.get().m_uPtr->data(), TagE.m_ArchetypeDB.get().m_nArchetypes} )
                {
                    ArchetypeEntry.m_upArchetype->Start();
                }
            }

            //
            // Lets start the graph
            //
            //
            m_GraphDB.Start(bContinuousPlay);
        }

        void Resume(bool bContinuousPlay = false)
        {
            m_GraphDB.Resume(bContinuousPlay);
        }

        guid                            m_Guid              {};
        mecs::universe::instance&       m_Universe;
        mecs::archetype::data_base      m_ArchetypeDB       {};
        mecs::graph::instance           m_GraphDB;
    };
}
