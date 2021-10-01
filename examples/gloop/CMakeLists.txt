add_executable(
  gloop
  gloop.cpp
  ${PicoSystemSrc}
)

# Pull in pico libraries that we need
target_link_libraries(gloop picosystem)

# create map/bin/hex file etc.
pico_add_extra_outputs(gloop)