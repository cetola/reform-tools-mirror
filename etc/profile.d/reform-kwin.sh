# KWIN_FORCE_SW_CURSOR=1 is currently needed because the RK3588 VOP2 driver
# doesn't report or enforce the limitations for overlays on certain displays,
# resulting in a black square behind the cursor and the display pipeline
# crashing when the cursor is moved into the screen edges.
# The bug is wontfix upstream. TODO: find the bug URL
KWIN_FORCE_SW_CURSOR=1
