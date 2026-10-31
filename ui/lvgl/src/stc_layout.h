#ifndef STC_LAYOUT_H
#define STC_LAYOUT_H
#include "lvgl.h"
typedef enum { STC_NODE_BOX, STC_NODE_TEXT, STC_NODE_CAMERA, STC_NODE_ICON, STC_NODE_BUTTON } stc_node_kind_t;
typedef struct {
    stc_node_kind_t kind; int parent, x, y, w, h, radius;
    const char *name, *text;
    const lv_font_t *font; int font_px;
    uint32_t fg, bg; bool has_bg;
    int asset;
} stc_layout_node_t;
typedef struct { const stc_layout_node_t *nodes; unsigned count; } stc_layout_t;
extern const stc_layout_t stc_layouts[9];
extern const lv_image_dsc_t *stc_icons[4][2];
extern const lv_image_dsc_t stc_camera_icon, stc_trophy_icon;
extern const char *stc_tips[4], *stc_examples[4];
#endif
