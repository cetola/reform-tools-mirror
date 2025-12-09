# PAN_MESA_DEBUG=gl3 enables OpenGL 3.3 from 3.1 (but doesn't change OpenGL ES)
# on Mali GPUs (like in RK3588 and A311D) by skipping over some features that
# normally don't allow the driver to advertise these versions, making some
# applications and games work that don't work with older OpenGL versions.
export PAN_MESA_DEBUG=gl3
