# Generated by oovCMaker
add_library(oovDbWriter SHARED DbString.cpp OovDatabase.cpp OovDatabaseWriter.cpp)

target_link_libraries(oovDbWriter oovCommon)

install(TARGETS oovDbWriter
  LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT lib)
