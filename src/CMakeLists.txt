include_directories(. ${mhwd_SOURCE_DIR}/libmhwd)

###
### mhwd binary
###

set( HEADERS
    ConsoleWriter.hpp
    Mhwd.hpp
    Transaction.hpp
)

set( SOURCES
    ConsoleWriter.cpp
    main.cpp
    Mhwd.cpp
    Transaction.cpp
)

set( LIBS mhwd)


add_executable(mhwd-bin ${SOURCES} ${HEADERS})
target_link_libraries(mhwd-bin ${LIBS})
set_target_properties(mhwd-bin PROPERTIES OUTPUT_NAME mhwd)


INSTALL(TARGETS mhwd-bin
  RUNTIME DESTINATION bin
)
