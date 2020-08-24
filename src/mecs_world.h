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
            // First make sure the archetypes are ready to start
            // This allows archetypes to update their double buffer components and such
            //
            m_ArchetypeDB.Start();

            //
            // Lets start the graph
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
