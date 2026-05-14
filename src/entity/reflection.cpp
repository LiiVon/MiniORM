#include "../../include/miniorm/entity/reflection.hpp"

namespace miniorm
{
    String field_flag_to_string(FieldFlag flag)
    {
        switch (flag)
        {
        case FieldFlag::PrimaryKey:
            return "PrimaryKey";
        case FieldFlag::AutoIncrement:
            return "AutoIncrement";
        case FieldFlag::NotNull:
            return "NotNull";
        case FieldFlag::Unique:
            return "Unique";
        case FieldFlag::Indexed:
            return "Indexed";
        case FieldFlag::DefaultValue:
            return "DefaultValue";
        default:
            return "None";
        }
    }
}
