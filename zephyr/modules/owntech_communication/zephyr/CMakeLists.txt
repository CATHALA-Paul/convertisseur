if(CONFIG_OWNTECH_COMMUNICATION)
  # Select directory to add to the include path
  zephyr_include_directories(./public_api)

  # Define the current folder as a Zephyr library
  zephyr_library()

  # Select source files to be compiled
  if (CONFIG_OWNTECH_COMMUNICATION_ENABLE_ANALOG)
    zephyr_library_sources(
      public_api/AnalogCommunication.cpp
    )
  endif()

  if (CONFIG_OWNTECH_COMMUNICATION_ENABLE_CAN)
    zephyr_library_sources(
      src/can.cpp
      src/data_objects.cpp
      src/thingset_isotp.cpp
      src/thingset_pub_sub.cpp
      public_api/CanCommunication.cpp
    )
  endif()

  if (CONFIG_OWNTECH_COMMUNICATION_ENABLE_RS485)
    zephyr_library_sources(
      public_api/Rs485Communication.cpp
    )
  endif()

  if (CONFIG_OWNTECH_COMMUNICATION_ENABLE_SYNC)
    zephyr_library_sources(
      public_api/SyncCommunication.cpp
    )
  endif()

endif()
