# Need a module config for find_package as lvgl doesn't provide one
clean(LVGL)
fpath(LVGL lvgl/lvgl.h) # Include path is the location of the main lvgl.h header
flib(LVGL lvgl) # Library is named liblvgl.a
export_lib(LVGL)
