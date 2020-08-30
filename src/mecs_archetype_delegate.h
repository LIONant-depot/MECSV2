namespace mecs::archetype::delegate
{
    using type_guid = xcore::guid::unit<64, struct mecs_archetype_delegate_type_guid_tag>;

    //---------------------------------------------------------------------------------
    // DELEGATE:: INSTANCE OVERRIDE
    // This classes specify what the end user can override to extend the functionality
    // of the delegate.
    //---------------------------------------------------------------------------------
    struct overrides
    {
        template<typename...T> using    all                     = mecs::archetype::query::all<std::tuple<T...>>;
        template<typename...T> using    any                     = mecs::archetype::query::any<std::tuple<T...>>;
        template<typename...T> using    none                    = mecs::archetype::query::none<std::tuple<T...>>;
        using                           type_guid               = mecs::archetype::delegate::type_guid;
        using                           entity                  = mecs::component::entity;
        using                           guid                    = xcore::guid::unit<64, struct mecs_archetype_delegate_tag>;
        using                           system                  = mecs::system::instance;

        constexpr static auto           type_guid_v             = type_guid{ nullptr };
        constexpr static auto           type_name_v             = xconst_universal_str("mecs::archetype::delegate(unnamed)");
        using                           query_t                 = std::tuple<>;

        xforceinline            void    msgInitializeQuery      (void)                                              noexcept {}
        xforceinline            void    msgGraphInit            (mecs::world::instance& World)                      noexcept {}
        xforceinline            void    msgFrameStart           (void)                                              noexcept {}
        xforceinline            void    msgFrameDone            (void)                                              noexcept {}
        xforceinline            void    msgHandleEvents         (mecs::component::entity&,mecs::system::instance&)  noexcept {}

        virtual                        ~overrides               (void) noexcept = default;
        virtual                 void    Disable                 (void) noexcept = 0;
        virtual                 void    Enable                  (void) noexcept = 0;

        guid                            m_Guid                  { nullptr };
    };

    //---------------------------------------------------------------------------------
    // DELEGATE:: INSTANCE
    //---------------------------------------------------------------------------------
    template< typename T_EVENT >
    struct instance : overrides
    {
        static_assert( std::is_base_of_v<mecs::archetype::event::details::base_event, T_EVENT> );
        using event_t = T_EVENT;
    };

    //---------------------------------------------------------------------------------
    // DELEGATE:: DESCRIPTOR
    //---------------------------------------------------------------------------------
    struct descriptor
    {
        using fn_create = overrides&( mecs::world::instance& ) noexcept;
        const type_guid                         m_Guid;
        const xcore::string::const_universal    m_Name;
        fn_create*                              m_fnCreate;
    };

    namespace details
    {
        //---------------------------------------------------------------------------------
        // DELEGATE:: DETAILS:: DELEGATE CONTRACT
        //---------------------------------------------------------------------------------
        template< typename T_DELEGATE >
        constexpr bool DelegateContract() noexcept
        {
            /*
          //  static_assert( std::is_based_of_v< base, T_DELEGATE >
          //                 , "Make sure your delegate is derived from mecs::delegate::instance" );

            static_assert(mecs::event::details::template EventContract<T_DELEGATE::event_t>());

            static_assert(std::is_same_v<decltype(T_DELEGATE::guid_v),const delegate::guid >
                            ,"Make sure your delegate has defined a guid_v of the right type (mecs::delegate::guid)");

            static_assert(std::is_same_v<decltype(T_DELEGATE::name_v),const xcore::string::const_universal >
                            ,"Make sure your delegate has a defined a name_v of the right type (xcore::string::const_universal)");

            static_assert(T_DELEGATE::guid_v.isValid()
                            ,"Make sure your delegate has a valid guid_v. Did you know you can initialize them with a string?");
                            */
            return true;
        };

        //---------------------------------------------------------------------------------
        // DELEGATE:: DETAILS:: MAKE CUSTOM INSTANCE
        //---------------------------------------------------------------------------------
        template< typename T_USER_DELEGATE >
        struct custom_instance final : T_USER_DELEGATE
        {
            using                   self_t              = custom_instance;
            using                   user_delegate_t     = T_USER_DELEGATE;
            using                   type_t              = mecs::archetype::event::details::base_event;
            using                   event_t             = typename user_delegate_t::event_t;
            constexpr static auto   query_v             = mecs::archetype::query::details::define<typename user_delegate_t::query_t>{};

            mecs::archetype::query::instance         m_Query;
            mecs::world::instance&                   m_World;

            virtual void    Disable (void) noexcept override 
            {
                // TODO: Missing implementation
                xassert(false);
            }

            virtual void    Enable  (void) noexcept override
            {
                // TODO: Missing implementation
                xassert(false);
            }

            constexpr           custom_instance         ( mecs::world::instance& World )                                        noexcept;
            inline      void    AttachToArchetype       ( archetype::instance& Archetype )                                      noexcept;
            inline      void    eventAddNewArchetype    ( archetype::instance& Archetype )                                      noexcept;
            inline      void    eventDeleteArchetype    ( archetype::instance& Archetype )                                      noexcept;
            inline      void    HandleEvents            ( mecs::component::entity& Entity, mecs::system::instance& System )     noexcept;
        };

        //---------------------------------------------------------------------------------

        template< typename T_DELEGATE >
        constexpr auto MakeDescriptor(void) noexcept
        {
            return mecs::archetype::delegate::descriptor
            {
                T_DELEGATE::type_guid_v.isValid() ? T_DELEGATE::type_guid_v : type_guid{ __FUNCSIG__ }
            ,   T_DELEGATE::type_name_v
            ,   []( mecs::world::instance& World ) constexpr noexcept -> overrides& { return *new custom_instance<T_DELEGATE>{ World }; }
            };
        }
    }

    template< typename T_DELEGATE >
    inline static constexpr auto descriptor_v = details::MakeDescriptor<T_DELEGATE>();

    //---------------------------------------------------------------------------------
    // DELEGATE:: DESCRIPTOR DATA BASE
    //---------------------------------------------------------------------------------
    struct descriptors_data_base
    {
        using map_delegate_type_t = mecs::tools::fixed_map<type_guid, const descriptor*, mecs::settings::max_event_types >;

        void Init(void) {}

        template< typename T_DELEGATE >
        void Register( void ) noexcept
        {
            m_mapDescriptors.alloc( descriptor_v<T_DELEGATE>.m_Guid, [](const descriptor*& pDescriptor )
            {
                pDescriptor = &descriptor_v<T_DELEGATE>;
            });
        }

        template< typename...T_DELEGATE >
        void TupleRegister( std::tuple<T_DELEGATE...>* ) noexcept
        {
            ( Register<T_DELEGATE>(), ... );
        }

        overrides& Create( mecs::world::instance& World, type_guid gTypeGuid ) noexcept
        {
            return m_mapDescriptors.get(gTypeGuid)->m_fnCreate(World);
        }

        map_delegate_type_t     m_mapDescriptors;
    };

    struct instance_data_base
    {
        using map_delegate_instance_t = mecs::tools::fixed_map<overrides::guid, std::unique_ptr<overrides>, mecs::settings::max_archetype_delegates >;

        template< typename T_ARCHETYPE_DELEGATE >
        T_ARCHETYPE_DELEGATE& Create( overrides::guid Guid ) noexcept;

        overrides& Create( type_guid gTypeGuid, overrides::guid InstanceGuid ) noexcept;
        instance_data_base(mecs::world::instance& World ) : m_World{ World } {}

        mecs::world::instance&  m_World;
        map_delegate_instance_t m_mapInstance;
    };
}