
#define NOMINMAX
#include <windows.h>
#include <memoryapi.h>
#include "mecs.h"
#include "E02_system_introduction.h"
#include "E03_system_moving_entities.h"


//------------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------------
int main(void)
{
    xcore::Init("Engine Programming");

    //
    // Tests
    //
    if(0) mecs::examples::E02_system_introduction::Test();
    if(1) mecs::examples::E03_system_moving_entities::Test();


    xcore::Kill();
    return 0;
}