
#define NOMINMAX
#include <windows.h>
#include <memoryapi.h>
#include "mecs.h"
#include "E02_system_introduction.h"
#include "E03_system_moving_entities.h"
#include "E04_delegate_move_inout.h"
#include "E05_custom_delegate_update_entity.h"
#include "E06_global_system_events.h"
#include "E07_double_buffer_components.h"
#include "E08_quantum_mutable_components.h"
#include "E09_quantum_double_buffer_components.h"
#include "E10_share_components.h"
#include "E11_complex_system_queries.h"
#include "E12_hierarchy_components.h"



//------------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------------
int main(void)
{
    xcore::Init("Engine Programming");

    //
    // Examples
    //
    if(0) mecs::examples::E02_system_introduction::Test();
    if(0) mecs::examples::E03_system_moving_entities::Test();
    if(0) mecs::examples::E04_delegate_moveinout::Test();
    if(0) mecs::examples::E05_custom_delegate_updated_entity::Test();
    if(0) mecs::examples::E06_global_system_events::Test();
    if(0) mecs::examples::E07_double_buffer_components::Test();
    if(0) mecs::examples::E08_quantum_mutable_components::Test();
    if(0) mecs::examples::E09_quantum_double_buffer_components::Test();
    if(0) mecs::examples::E10_shared_components::Test();
    if(0) mecs::examples::E11_complex_system_queries::Test();
    if(1) mecs::examples::E12_hierarchy_components::Test();

    xcore::Kill();
    return 0;
}