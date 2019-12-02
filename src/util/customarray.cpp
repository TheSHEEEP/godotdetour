#include "customarray.h"

using namespace godot;

void
CustomArray::_register_methods()
{
    register_method("at", &CustomArray::at);
    register_method("append", &CustomArray::append);
    register_method("popFront", &CustomArray::popFront);
    register_method("clear", &CustomArray::clear);
    register_method("erase", &CustomArray::erase);
    register_method("size", &CustomArray::size);
}

void
CustomArray::_init()
{

}
