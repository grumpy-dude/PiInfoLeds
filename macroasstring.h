#ifndef _MACRO_AS_STRING_H

    #define _MACRO_AS_STRING_H

    #define MACRO_NAME_AS_STRING(macro_name)  #macro_name
    #define MACRO_VALUE_AS_STRING(macro_name) MACRO_NAME_AS_STRING(macro_name)

#endif