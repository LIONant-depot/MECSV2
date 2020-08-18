
#define NOMINMAX
#include <windows.h>
#include <memoryapi.h>
#include "mecs.h"
#include "mecs_examples.h"

//------------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------------
int main(void)
{
    xcore::Init("Engine Programming");
    mecs::examples::Test();
    xcore::Kill();
    return 0;
}