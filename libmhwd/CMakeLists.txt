include_directories(. vita)

###
### mhwd library
###

set( LIB_HEADERS
    const.h
    vita/string.hpp
    Data.hpp
    Device.hpp
    Enums.hpp
    Utils.hpp
    Config.hpp
)

set( LIB_SOURCES
    vita/string.cpp
    Data.cpp
    Device.cpp
    Config.cpp
)

set( LIB_LIBS hd)


add_library (mhwd SHARED ${LIB_SOURCES} ${LIB_HEADERS})
target_link_libraries(mhwd ${LIB_LIBS})


INSTALL(TARGETS mhwd
  LIBRARY DESTINATION lib
)
