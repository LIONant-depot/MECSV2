namespace mecs::archetype::query
{
    //---------------------------------------------------------------------------------
    // QUERY::ALL, NONE, ANY
    // Filters used for queries
    //---------------------------------------------------------------------------------
    namespace details
    {
        using entry = std::tuple
        < 
              const mecs::component::descriptor&    // 0 - Descriptor pointer
            , bool                                  // 1 - is constant?
        >;

        template< typename T_BASE, typename T_TUPLE >
        struct filter_out;

        template< typename T_BASE, typename...T_ARGS >
        struct filter_out< T_BASE, std::tuple<T_ARGS...> >
        {
            using type  = xcore::types::tuple_cat_t< std::conditional_t< std::is_base_of_v< T_BASE, xcore::types::decay_full_t<T_ARGS>>, std::tuple<>, std::tuple<T_ARGS> > ... >;
        };

        template< typename T_BASE, typename T_TUPLE >
        struct filter_out_all_but;

        template< typename T_BASE, typename...T_ARGS >
        struct filter_out_all_but< T_BASE, std::tuple<T_ARGS...> >
        {
            using type  = xcore::types::tuple_cat_t< std::conditional_t< std::is_base_of_v< T_BASE, xcore::types::decay_full_t<T_ARGS>>, std::tuple<T_ARGS>, std::tuple<> > ... >;
        };

        template< typename...T_ARGS >
        using filter_out_all_but_refereces_t = xcore::types::tuple_cat_t< std::conditional_t< std::is_reference_v<T_ARGS>, std::tuple<T_ARGS>, std::tuple<> > ... >;

        template< typename...T_ARGS >
        using filter_out_all_but_pointers_t = xcore::types::tuple_cat_t< std::conditional_t< std::is_pointer_v<T_ARGS>, std::tuple<T_ARGS>, std::tuple<> > ... >;

        template< typename T_TUPLE_COMPONENTS_AND_TAGS >
        struct get_arrays;

        template< typename...T_COMPONENTS_AND_TAGS >
        struct get_arrays< std::tuple<T_COMPONENTS_AND_TAGS...> >
        {
            static constexpr auto value = std::array{ entry{ mecs::component::descriptor_v<T_COMPONENTS_AND_TAGS>, std::is_const_v<std::remove_pointer_t<std::remove_reference_t<T_COMPONENTS_AND_TAGS>>> } ... };
        };

        template<>
        struct get_arrays< std::tuple<> >
        {
            static constexpr auto value = std::span<entry>{ nullptr, 0ull };
        };

        template< template< typename... > typename T, typename...T_ARGS >
        using convert_t = std::invoke_result_t< T<T_ARGS... >(std::tuple<T_ARGS...>*), std::tuple<T_ARGS...>* >;

        template< typename T_ORIGINAL_TUPLE, typename T_SHORTED_TUPLE >
        struct remap_arrays;

        template< typename T_ORIGINAL_TUPLE, typename...T_ARGS >
        struct remap_arrays< T_ORIGINAL_TUPLE, std::tuple<T_ARGS...> >
        {
            static constexpr auto value = std::array{ static_cast<std::uint16_t>(xcore::types::tuple_t2i_v<T_ARGS, T_ORIGINAL_TUPLE>) ... };
        };

        template< typename T_ORIGINAL_TUPLE >
        struct remap_arrays< T_ORIGINAL_TUPLE, std::tuple<> >
        {
            static constexpr auto value = std::array< std::uint16_t, 0> {};
        };
    }

    template< typename...T_COMPONENTS_AND_TAGS >
    struct all final
    {
        static_assert( ((std::is_reference_v<T_COMPONENTS_AND_TAGS> == false) && ...) );
     //   static_assert( ((std::is_const_v<T_COMPONENTS_AND_TAGS>     == false) && ...) );

        using                 type              = xcore::types::tuple_sort_t                    <mecs::component::smaller_guid, std::tuple<T_COMPONENTS_AND_TAGS...>>;
        using                 filterout         = typename query::details::filter_out           <mecs::component::tag, T_COMPONENTS_AND_TAGS...>::type;
        using                 filterout_allbut  = typename query::details::filter_out_all_but   <mecs::component::tag, T_COMPONENTS_AND_TAGS...>::type;

        static constexpr auto component_list_v  = details::get_arrays<filterout>::value;
        static constexpr auto tag_list_v        = details::get_arrays<filterout_allbut>::value;
    };

    template< typename...T_COMPONENTS_AND_TAGS >
    struct none final
    {
        static_assert( ((std::is_reference_v<T_COMPONENTS_AND_TAGS> == false) && ...) );
      //  static_assert( ((std::is_const_v<T_COMPONENTS_AND_TAGS>     == false) && ...) );

        using                 type              = xcore::types::tuple_sort_t             <mecs::component::smaller_guid, std::tuple<T_COMPONENTS_AND_TAGS...>>;
        using                 filterout         = typename details::filter_out           <mecs::component::tag, T_COMPONENTS_AND_TAGS...>::type;
        using                 filterout_allbut  = typename details::filter_out_all_but   <mecs::component::tag, T_COMPONENTS_AND_TAGS...>::type;

        static constexpr auto component_list_v  = details::get_arrays<filterout>::value;
        static constexpr auto tag_list_v        = details::get_arrays<filterout_allbut>::value;
    };

    template< typename...T_COMPONENTS_AND_TAGS >
    struct any final
    {
        static_assert( ((std::is_reference_v<T_COMPONENTS_AND_TAGS> == false) && ...) );
     //   static_assert( ((std::is_const_v<T_COMPONENTS_AND_TAGS>     == false) && ...) );

        using                 type              = xcore::types::tuple_sort_t             <mecs::component::smaller_guid, std::tuple<T_COMPONENTS_AND_TAGS...>>;
        using                 filterout         = typename details::filter_out           <mecs::component::tag, T_COMPONENTS_AND_TAGS...>::type;
        using                 filterout_allbut  = typename details::filter_out_all_but   <mecs::component::tag, T_COMPONENTS_AND_TAGS...>::type;

        static constexpr auto component_list_v  = details::get_arrays<filterout>::value;
        static constexpr auto tag_list_v        = details::get_arrays<filterout_allbut>::value;
    };

    //---------------------------------------------------------------------------------
    // QUERY::DETAILS DEFINE
    //---------------------------------------------------------------------------------
    namespace details
    {
        struct define_data
        {
            struct query
            {
                std::span<const entry>             m_All;            // Which components/tags we must have
                std::span<const entry>             m_Any;            // Must have at least one component/tag from the set
                std::span<const entry>             m_None;           // Which components/tags you must not have
            };

            query   m_ComponentQuery;
            query   m_TagQuery;
        };

        template< typename...T_QUERY_FILTERS >
        struct define;

        template< typename...T_QUERY_FILTERS >
        struct define<std::tuple<T_QUERY_FILTERS...>> : define_data
        {
            template< typename T >
            constexpr static void Initializer( define& This ) noexcept
            {
                if constexpr ( xcore::types::is_specialized_v<mecs::archetype::query::all, T> ) 
                {
                    if constexpr (T::tag_list_v.size())           This.m_TagQuery.m_All           = std::span(&T::tag_list_v[0],       T::tag_list_v.size());
                    if constexpr (T::component_list_v.size())     This.m_ComponentQuery.m_All     = std::span(&T::component_list_v[0], T::component_list_v.size());
                }
                else if constexpr ( xcore::types::is_specialized_v<mecs::archetype::query::any, T> )
                {
                    if constexpr (T::tag_list_v.size())           This.m_TagQuery.m_Any           = std::span(&T::tag_list_v[0],       T::tag_list_v.size());
                    if constexpr (T::component_list_v.size())     This.m_ComponentQuery.m_Any     = std::span(&T::component_list_v[0], T::component_list_v.size());
                }
                else if constexpr ( xcore::types::is_specialized_v<mecs::archetype::query::none, T> )
                {
                    if constexpr (T::tag_list_v.size())           This.m_TagQuery.m_None          = std::span(&T::tag_list_v[0],       T::tag_list_v.size());
                    if constexpr (T::component_list_v.size())     This.m_ComponentQuery.m_None    = std::span(&T::component_list_v[0], T::component_list_v.size() );
                }
            };

            constexpr define( void ) noexcept
            {
                ( Initializer<T_QUERY_FILTERS>( *this ), ... );
            }
        };

        template< typename...T_QUERY_PARAMS >
        struct define<void(T_QUERY_PARAMS...) > : define_data
        {
            constexpr define(void) noexcept
            {
                // Validate arguments
                static_assert
                ((
                    (   std::is_same_v<      xcore::types::decay_full_t<T_QUERY_PARAMS>&, T_QUERY_PARAMS >      // We support references
                    ||  std::is_same_v<const xcore::types::decay_full_t<T_QUERY_PARAMS>&, T_QUERY_PARAMS >      // We support const references
                    ||  std::is_same_v<      xcore::types::decay_full_t<T_QUERY_PARAMS>*, T_QUERY_PARAMS >      // We support pointers
                    ||  std::is_same_v<const xcore::types::decay_full_t<T_QUERY_PARAMS>*, T_QUERY_PARAMS >      // We support const pointers
                    ) && ...
                ));
                static_assert( false == xcore::types::tuple_has_duplicates_v<std::tuple<T_QUERY_PARAMS...>> );
                static_assert( ((xcore::types::count_of_v< xcore::types::decay_full_t<T_QUERY_PARAMS>, xcore::types::decay_full_t<T_QUERY_PARAMS>...> <= 2) && ... )
                               ,"You have a system with same components as parameters too many times" );
                static_assert( ((xcore::types::count_of_v < xcore::types::decay_full_t<T_QUERY_PARAMS>, xcore::types::decay_full_t<T_QUERY_PARAMS>...> == 2 
                                ? mecs::component::descriptor_v<T_QUERY_PARAMS>.m_isDoubleBuffer
                                : true ) && ...), "You have a system with a parameter duplicated that is NOT a component data_access double buffer" );

                using all_components = typename mecs::archetype::query::details::filter_out_all_but_refereces_t<T_QUERY_PARAMS...>;
                if constexpr( !!std::tuple_size_v<all_components> )
                {
                    using all_t = typename mecs::archetype::query::details::template convert_t< mecs::archetype::query::all, all_components >;
                    if constexpr (!!all_t::tag_list_v.size())         m_TagQuery.m_All        = std::span(&all_t::tag_list_v[0],        all_t::tag_list_v.size()        );
                    if constexpr (!!all_t::component_list_v.size())   m_ComponentQuery.m_All  = std::span(&all_t::component_list_v[0],  all_t::component_list_v.size()  );
                }

                using any_components = typename mecs::archetype::query::details::filter_out_all_but_pointers_t<T_QUERY_PARAMS...>;
                if constexpr( !!std::tuple_size_v<any_components> )
                {
                    using any_t = typename mecs::archetype::query::details::template convert_t< mecs::archetype::query::any, any_components >;
                    if constexpr( !!any_t::tag_list_v.size()       )  m_TagQuery.m_Any       = std::span( &any_t::tag_list_v[0],       any_t::tag_list_v.size()       );
                    if constexpr( !!any_t::component_list_v.size() )  m_ComponentQuery.m_Any = std::span( &any_t::component_list_v[0], any_t::component_list_v.size() );
                }
            }
        };

        template< typename T_CLASS >
        struct define< T_CLASS > : define<decltype(&T_CLASS::operator())> {};

        template< class T_CLASS, typename... T_ARGS >
        struct define < void(T_CLASS::*)(T_ARGS...) > : define< void(T_ARGS...) >{};

        template< class T_CLASS, typename... T_ARGS >
        struct define < void(T_CLASS::*)(T_ARGS...) const> : define< void(T_ARGS...) > {};

        template< class T_CLASS, typename... T_ARGS >
        struct define < void(T_CLASS::*)(T_ARGS...) noexcept> : define< void(T_ARGS...) > {};

        template< class T_CLASS, typename... T_ARGS >
        struct define < void(T_CLASS::*)(T_ARGS...) const noexcept> : define< void(T_ARGS...) > {};
    }

    //---------------------------------------------------------------------------------
    // QUERY::ACCESS
    //---------------------------------------------------------------------------------
    /*
    template< typename...T_COMPONENTS_WITH_CONST_AND_REFERENCES >
    struct access
    {
        static_assert( (std::remove_pointer_t<std::decay_t<T_COMPONENTS_WITH_CONST_AND_REFERENCES>>::type_guid_v.isValid() && ...), "You must pass a valid component" );
        static_assert( (( std::is_reference_v<T_COMPONENTS_WITH_CONST_AND_REFERENCES>
                       || std::is_pointer_v<T_COMPONENTS_WITH_CONST_AND_REFERENCES>   ) && ...), "You must specify the access type if reference '&' or pointer '*' for each component" );

        using tuple_t = std::tuple<T_COMPONENTS_WITH_CONST_AND_REFERENCES ...>;

        tuple_t m_Tuple;

        template< typename T > xforceinline constexpr
        bool isValid( void ) const noexcept
        {
            static_assert( std::is_pointer_v<T> );
            return std::get<T>(m_Tuple) != nullptr;
        }

        template< typename T > xforceinline constexpr
        auto& get( void ) const noexcept
        {
            static_assert( std::is_reference_v<T> || std::is_pointer_v<T> );
            if constexpr ( std::is_reference_v<T> )
            {
                return std::get<T>(m_Tuple);
            }
            else
            {
                xassume(std::get<T>(m_Tuple));
                return *std::get<T>(m_Tuple);
            }
        }
    };
    */

    //---------------------------------------------------------------------------------
    // QUERY:: RESULT 
    //---------------------------------------------------------------------------------
    struct result_entry
    {
        constexpr static std::uint16_t invalid_index = (0xffff>>1);
        struct e
        {
            std::uint16_t   m_Index:15
            ,               m_isShared:1;
        };

        mecs::archetype::instance*                          m_pArchetype;
        std::span<const details::entry>                     m_FunctionDescriptors;
        std::array<e, 15>                                   m_lFunctionToArchetype;     // Positive numbers are data components, negative numbers are share components
        std::uint8_t                                        m_nParameters;              // Number of parameters in the function call
    };

    //---------------------------------------------------------------------------------
    // QUERY:: INSTANCE
    //---------------------------------------------------------------------------------
    struct instance
    {
        struct component_query
        {
            mecs::component::filter_bits                 m_All  { nullptr };
            mecs::component::filter_bits                 m_Any  { nullptr };
            mecs::component::filter_bits                 m_None { nullptr };
        };

        struct tag_query
        {
            mecs::component::filter_bits                 m_All  { nullptr };
            mecs::component::filter_bits                 m_Any  { nullptr };
            mecs::component::filter_bits                 m_None { nullptr };
        };

        xcore::vector<result_entry>     m_lResults          {};
        bool                            m_isInitialized     { false };
        mecs::component::filter_bits    m_Write             { nullptr };
        component_query                 m_ComponentQuery    {};
        tag_query                       m_TagQuery          {};

        template< typename T_SYSTEM_FUNCTION, auto& Defined >
        xforceinline void Initialize( void ) noexcept //const query::details::define_data& Defined, mecs::component::descriptors_data_base& ComponentDescriptorDB ) const noexcept
        {
            static_assert(std::is_convertible_v< decltype(Defined), query::details::define_data >);

            if( m_isInitialized ) return;
            m_isInitialized = true;

            static constexpr query::details::define<T_SYSTEM_FUNCTION> function_define_data_v{};

            //
            // Handle the Component Side
            //
            for( auto E : Defined.m_ComponentQuery.m_All )
                m_ComponentQuery.m_All.AddBit( std::get<0>(E).m_BitNumber );

            for (auto E : Defined.m_ComponentQuery.m_Any)
                m_ComponentQuery.m_Any.AddBit( std::get<0>(E).m_BitNumber );

            for (auto E : Defined.m_ComponentQuery.m_None)
                m_ComponentQuery.m_None.AddBit( std::get<0>(E).m_BitNumber );

            //
            // Add the contribution from the function parameters
            //
            if constexpr (function_define_data_v.m_ComponentQuery.m_All.size()) for( auto& E : function_define_data_v.m_ComponentQuery.m_All )
            {
                // DEBUG WARNING: Make sure all components are register!!!
                const auto& Descriptor = std::get<0>(E);
                m_ComponentQuery.m_All.AddBit(Descriptor.m_BitNumber);
                if( std::get<1>(E) ) m_Write.AddBit(Descriptor.m_BitNumber);
            }

            if constexpr (function_define_data_v.m_ComponentQuery.m_Any.size()) for (auto& E : function_define_data_v.m_ComponentQuery.m_Any )
            {
                // DEBUG WARNING: Make sure all components are register!!!
                const auto& Descriptor = std::get<0>(E);
                m_ComponentQuery.m_Any.AddBit(Descriptor.m_BitNumber);
                if( std::get<1>(E) ) m_Write.AddBit(Descriptor.m_BitNumber);
            }

            //
            // Handle the tag side
            //
            for (auto E : Defined.m_TagQuery.m_All)
                m_TagQuery.m_All.AddBit(std::get<0>(E).m_BitNumber);

            for (auto E : Defined.m_TagQuery.m_Any)
                m_TagQuery.m_Any.AddBit(std::get<0>(E).m_BitNumber);

            for (auto E : Defined.m_TagQuery.m_None)
                m_TagQuery.m_None.AddBit(std::get<0>(E).m_BitNumber);
        }

        bool TryAppendArchetype( mecs::archetype::instance& Archetype ) noexcept;
    };
}