#ifndef CUSTOMARRAY_H
#define CUSTOMARRAY_H

#include <Godot.hpp>
#include <Variant.hpp>
#include <vector>

namespace godot
{
    /**
     * @brief Custom array class that becomes necessary as Godot's own array causes memory leaks when passed to GDNative:
     *          https://github.com/godotengine/godot/issues/29391
     *          If this ever becomes resolved, this class would become obsolete.
     */
    class CustomArray : public Reference
    {
        GODOT_CLASS(CustomArray, Reference)

    public:
        static void _register_methods();

        /**
         * @brief Called when .new() is called in gdscript
         */
        void _init();

        // Various typical array functions
        Variant at(int index);
        void append(Variant value);
        Variant popFront();
        void clear();
        void erase(int index);
        int size();

        // Public access to work with it easier in C++
        std::vector<Variant> contents;
    };

    // Inline functions
    inline Variant
    CustomArray::at(int index)
    {
        if (index >= contents.size())
        {
            ERR_PRINT(String("CustomArray:: Trying to access out of bounds index {0}").format(Array::make(index)));
            return 0;
        }
        return contents[index];
    }

    inline void
    CustomArray::append(Variant value)
    {
        contents.emplace_back(value);
    }

    inline Variant
    CustomArray::popFront()
    {
        Variant front = contents.front();
        contents.erase(contents.begin());
        return front;
    }

    inline void
    CustomArray::clear()
    {
        contents.clear();
    }

    inline void
    CustomArray::erase(int index)
    {
        if (index >= contents.size())
        {
            ERR_PRINT(String("CustomArray:: Trying to erase out of bounds index {0}").format(Array::make(index)));
            return;
        }
        contents.erase(contents.begin() + index);
    }

    inline int
    CustomArray::size()
    {
        return contents.size();
    }
}

#endif // CUSTOMARRAY_H
