/* Generated from design/layout.json exported from Figma. Do not edit. */
#include "stc_layout.h"
#include "stc_tokens.h"
LV_FONT_DECLARE(stc_font_tc_26);
LV_FONT_DECLARE(stc_font_tc_32);
LV_FONT_DECLARE(stc_font_tc_56);
LV_FONT_DECLARE(stc_font_tc_112);
LV_FONT_DECLARE(stc_font_num_32);
LV_FONT_DECLARE(stc_font_num_80);
static const stc_layout_node_t state_0[] = {
  {.kind=STC_NODE_BOX,.parent=-1,.x=0,.y=0,.w=800,.h=480,.radius=0,.name="state/idle",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_BOX,.parent=0,.x=0,.y=0,.w=800,.h=72,.radius=0,.name="comp/topbar",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BRAND,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=0,.x=24,.y=12,.w=352,.h=48,.radius=0,.name="brand",.text="分類小隊",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_ON_BRAND,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=0,.x=424,.y=12,.w=352,.h=48,.radius=0,.name="status",.text="準備好",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_ON_BRAND,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_BOX,.parent=0,.x=24,.y=96,.w=752,.h=280,.radius=0,.name="comp/content",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_BOX,.parent=4,.x=384,.y=136,.w=368,.h=144,.radius=16,.name="comp/score-panel",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_SURFACE,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_BOX,.parent=5,.x=0,.y=0,.w=368,.h=8,.radius=0,.name="accent",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_ACCENT,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=4,.x=0,.y=0,.w=752,.h=80,.radius=0,.name="prompt",.text="一起做分類小達人",.font=&stc_font_tc_56,.font_px=56,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=4,.x=0,.y=88,.w=352,.h=48,.radius=0,.name="hint",.text="放一件物件落台",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_CAMERA,.parent=4,.x=0,.y=136,.w=320,.h=144,.radius=16,.name="comp/camera",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=4,.x=408,.y=88,.w=320,.h=48,.radius=0,.name="score-label",.text="目前分數",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=4,.x=408,.y=160,.w=320,.h=96,.radius=0,.name="score",.text="120",.font=&stc_font_num_80,.font_px=80,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_BUTTON,.parent=0,.x=24,.y=400,.w=752,.h=64,.radius=16,.name="action/start",.text="開始挑戰",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_ON_BRAND,.bg=STC_COLOR_BRAND,.has_bg=true,.asset=-1},
};
static const stc_layout_node_t state_1[] = {
  {.kind=STC_NODE_BOX,.parent=-1,.x=0,.y=0,.w=800,.h=480,.radius=0,.name="state/capturing",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_BOX,.parent=0,.x=0,.y=0,.w=800,.h=72,.radius=0,.name="comp/topbar",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BRAND,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=0,.x=24,.y=12,.w=352,.h=48,.radius=0,.name="brand",.text="分類小隊",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_ON_BRAND,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=0,.x=424,.y=12,.w=352,.h=48,.radius=0,.name="status",.text="拍攝中",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_ON_BRAND,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_BOX,.parent=0,.x=24,.y=96,.w=752,.h=280,.radius=0,.name="comp/content",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=4,.x=0,.y=0,.w=752,.h=80,.radius=0,.name="prompt",.text="小偵探，睇清楚",.font=&stc_font_tc_56,.font_px=56,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_CAMERA,.parent=4,.x=0,.y=96,.w=480,.h=184,.radius=16,.name="comp/camera",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=4,.x=504,.y=104,.w=248,.h=96,.radius=0,.name="hint",.text="先觀察物料\n手移開，再拍攝",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_BOX,.parent=0,.x=24,.y=400,.w=752,.h=64,.radius=16,.name="comp/instruction",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_SURFACE,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=8,.x=24,.y=8,.w=704,.h=48,.radius=0,.name="instruction",.text="保持物件穩定，避免遮擋",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
};
static const stc_layout_node_t state_2[] = {
  {.kind=STC_NODE_BOX,.parent=-1,.x=0,.y=0,.w=800,.h=480,.radius=0,.name="state/classifying",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_BOX,.parent=0,.x=0,.y=0,.w=800,.h=72,.radius=0,.name="comp/topbar",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BRAND,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=0,.x=24,.y=12,.w=352,.h=48,.radius=0,.name="brand",.text="分類小隊",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_ON_BRAND,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=0,.x=424,.y=12,.w=352,.h=48,.radius=0,.name="status",.text="辨識中",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_ON_BRAND,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_BOX,.parent=0,.x=24,.y=96,.w=752,.h=280,.radius=0,.name="comp/content",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=4,.x=0,.y=0,.w=752,.h=80,.radius=0,.name="prompt",.text="你估會係邊一類？",.font=&stc_font_tc_56,.font_px=56,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_CAMERA,.parent=4,.x=0,.y=96,.w=480,.h=184,.radius=16,.name="comp/camera",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=4,.x=504,.y=104,.w=248,.h=96,.radius=0,.name="hint",.text="想一想物料\n我哋一齊搵答案",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=4,.x=504,.y=224,.w=248,.h=40,.radius=0,.name="previous",.text="上一輪：可回收物",.font=&stc_font_tc_26,.font_px=26,.fg=STC_COLOR_FG_MUTED,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_BOX,.parent=4,.x=16,.y=112,.w=448,.h=8,.radius=16,.name="scan",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_ACCENT,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_BOX,.parent=0,.x=24,.y=400,.w=752,.h=64,.radius=16,.name="comp/instruction",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_SURFACE,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=10,.x=24,.y=8,.w=704,.h=48,.radius=0,.name="instruction",.text="本輪完成後會顯示分類",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
};
static const stc_layout_node_t state_3[] = {
  {.kind=STC_NODE_BOX,.parent=-1,.x=0,.y=0,.w=800,.h=480,.radius=0,.name="state/result_ok",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_BOX,.parent=0,.x=0,.y=0,.w=800,.h=72,.radius=0,.name="comp/topbar",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BRAND,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=0,.x=24,.y=12,.w=352,.h=48,.radius=0,.name="brand",.text="分類小隊",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_ON_BRAND,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=0,.x=424,.y=12,.w=224,.h=48,.radius=0,.name="status",.text="連對 4 次",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_ON_BRAND,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_BOX,.parent=0,.x=24,.y=80,.w=752,.h=304,.radius=0,.name="comp/content",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=4,.x=0,.y=0,.w=512,.h=144,.radius=0,.name="category",.text="可回收物",.font=&stc_font_tc_112,.font_px=112,.fg=STC_COLOR_CAT0,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_ICON,.parent=4,.x=656,.y=24,.w=96,.h=96,.radius=16,.name="cat/0",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=false,.asset=0},
  {.kind=STC_NODE_TEXT,.parent=4,.x=256,.y=160,.w=496,.h=40,.radius=0,.name="confidence",.text="信心值 93%",.font=&stc_font_tc_26,.font_px=26,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=4,.x=0,.y=208,.w=232,.h=96,.radius=0,.name="score",.text="120",.font=&stc_font_num_80,.font_px=80,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=4,.x=256,.y=208,.w=496,.h=96,.radius=0,.name="tip",.text="先清空，再沖洗\n乾淨回收再造",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=4,.x=0,.y=160,.w=232,.h=40,.radius=0,.name="points",.text="目前分數",.font=&stc_font_tc_26,.font_px=26,.fg=STC_COLOR_OK,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_CAMERA,.parent=4,.x=528,.y=24,.w=112,.h=112,.radius=16,.name="comp/object",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_BOX,.parent=0,.x=24,.y=400,.w=752,.h=64,.radius=16,.name="comp/instruction",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_SURFACE,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=12,.x=24,.y=8,.w=704,.h=48,.radius=0,.name="instruction",.text="等待開蓋，請勿推動桶蓋",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_BUTTON,.parent=0,.x=648,.y=12,.w=128,.h=48,.radius=16,.name="action/finish",.text="完成",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_ON_ACCENT,.bg=STC_COLOR_ACCENT,.has_bg=true,.asset=-1},
};
static const stc_layout_node_t state_4[] = {
  {.kind=STC_NODE_BOX,.parent=-1,.x=0,.y=0,.w=800,.h=480,.radius=0,.name="state/result_retry",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_BOX,.parent=0,.x=0,.y=0,.w=800,.h=72,.radius=0,.name="comp/topbar",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BRAND,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=0,.x=24,.y=12,.w=352,.h=48,.radius=0,.name="brand",.text="分類小隊",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_ON_BRAND,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=0,.x=424,.y=12,.w=352,.h=48,.radius=0,.name="status",.text="需要再試",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_ON_BRAND,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_BOX,.parent=0,.x=24,.y=96,.w=752,.h=280,.radius=0,.name="comp/content",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=4,.x=0,.y=0,.w=752,.h=80,.radius=0,.name="prompt",.text="未睇清，再試就得",.font=&stc_font_tc_56,.font_px=56,.fg=STC_COLOR_WARN,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=4,.x=0,.y=88,.w=752,.h=48,.radius=0,.name="hint",.text="唔使急，換個角度再影",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_BOX,.parent=4,.x=0,.y=160,.w=368,.h=120,.radius=16,.name="candidate/0",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_SURFACE,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=7,.x=88,.y=16,.w=264,.h=40,.radius=0,.name="category",.text="可回收物",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=7,.x=88,.y=64,.w=264,.h=40,.radius=0,.name="probability",.text="52%",.font=&stc_font_num_32,.font_px=32,.fg=STC_COLOR_FG_MUTED,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_ICON,.parent=7,.x=16,.y=36,.w=48,.h=48,.radius=16,.name="cat/0",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=false,.asset=0},
  {.kind=STC_NODE_BOX,.parent=4,.x=384,.y=160,.w=368,.h=120,.radius=16,.name="candidate/1",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_SURFACE,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=11,.x=88,.y=16,.w=264,.h=40,.radius=0,.name="category",.text="其他",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=11,.x=88,.y=64,.w=264,.h=40,.radius=0,.name="probability",.text="43%",.font=&stc_font_num_32,.font_px=32,.fg=STC_COLOR_FG_MUTED,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_ICON,.parent=11,.x=16,.y=36,.w=48,.h=48,.radius=16,.name="cat/3",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=false,.asset=3},
  {.kind=STC_NODE_BOX,.parent=0,.x=24,.y=400,.w=752,.h=64,.radius=16,.name="comp/instruction",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_SURFACE,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=15,.x=24,.y=8,.w=704,.h=48,.radius=0,.name="instruction",.text="換個角度後，再放回台中央",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_WARN,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
};
static const stc_layout_node_t state_5[] = {
  {.kind=STC_NODE_BOX,.parent=-1,.x=0,.y=0,.w=800,.h=480,.radius=0,.name="state/error",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_BOX,.parent=0,.x=0,.y=0,.w=800,.h=72,.radius=0,.name="comp/topbar",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BRAND,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=0,.x=24,.y=12,.w=352,.h=48,.radius=0,.name="brand",.text="分類小隊",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_ON_BRAND,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=0,.x=424,.y=12,.w=352,.h=48,.radius=0,.name="status",.text="暫時未能辨識",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_ON_BRAND,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_BOX,.parent=0,.x=24,.y=96,.w=752,.h=280,.radius=0,.name="comp/content",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=4,.x=0,.y=0,.w=752,.h=80,.radius=0,.name="prompt",.text="相機未就緒",.font=&stc_font_tc_56,.font_px=56,.fg=STC_COLOR_ERROR,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=4,.x=0,.y=88,.w=752,.h=48,.radius=0,.name="hint",.text="請檢查相機，再按重試",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=4,.x=0,.y=160,.w=752,.h=48,.radius=0,.name="error-code",.text="CAMERA_OFFLINE",.font=&stc_font_num_32,.font_px=32,.fg=STC_COLOR_FG_MUTED,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_BUTTON,.parent=0,.x=24,.y=400,.w=752,.h=64,.radius=16,.name="action/retry",.text="重試",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_BG,.bg=STC_COLOR_BRAND,.has_bg=true,.asset=-1},
};
static const stc_layout_node_t state_6[] = {
  {.kind=STC_NODE_BOX,.parent=-1,.x=0,.y=0,.w=800,.h=480,.radius=0,.name="state/category_guide",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_BOX,.parent=0,.x=0,.y=0,.w=800,.h=72,.radius=0,.name="comp/topbar",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BRAND,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=0,.x=24,.y=12,.w=352,.h=48,.radius=0,.name="brand",.text="分類小隊",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_ON_BRAND,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=0,.x=424,.y=12,.w=352,.h=48,.radius=0,.name="status",.text="分類圖鑑",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_ON_BRAND,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=0,.x=24,.y=88,.w=752,.h=40,.radius=0,.name="prompt",.text="點一類，發現分類小知識",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_BUTTON,.parent=0,.x=24,.y=136,.w=368,.h=112,.radius=16,.name="action/learn-0",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_SURFACE,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=0,.x=112,.y=152,.w=264,.h=40,.radius=0,.name="cat-label/0",.text="可回收物",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_CAT0,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=0,.x=112,.y=200,.w=264,.h=32,.radius=0,.name="example/0",.text="膠樽、鋁罐",.font=&stc_font_tc_26,.font_px=26,.fg=STC_COLOR_FG_MUTED,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_BUTTON,.parent=0,.x=408,.y=136,.w=368,.h=112,.radius=16,.name="action/learn-1",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_SURFACE,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=0,.x=496,.y=152,.w=264,.h=40,.radius=0,.name="cat-label/1",.text="廚餘",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_CAT1,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=0,.x=496,.y=200,.w=264,.h=32,.radius=0,.name="example/1",.text="果皮、剩飯",.font=&stc_font_tc_26,.font_px=26,.fg=STC_COLOR_FG_MUTED,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_BUTTON,.parent=0,.x=24,.y=264,.w=368,.h=112,.radius=16,.name="action/learn-2",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_SURFACE,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=0,.x=112,.y=280,.w=264,.h=40,.radius=0,.name="cat-label/2",.text="有害",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_CAT2,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=0,.x=112,.y=328,.w=264,.h=32,.radius=0,.name="example/2",.text="充電池",.font=&stc_font_tc_26,.font_px=26,.fg=STC_COLOR_FG_MUTED,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_BUTTON,.parent=0,.x=408,.y=264,.w=368,.h=112,.radius=16,.name="action/learn-3",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_SURFACE,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=0,.x=496,.y=280,.w=264,.h=40,.radius=0,.name="cat-label/3",.text="其他",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_CAT3,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=0,.x=496,.y=328,.w=264,.h=32,.radius=0,.name="example/3",.text="用過的紙巾",.font=&stc_font_tc_26,.font_px=26,.fg=STC_COLOR_FG_MUTED,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_BUTTON,.parent=0,.x=24,.y=400,.w=368,.h=64,.radius=16,.name="action/finish",.text="返回成績",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_BRAND,.bg=STC_COLOR_SURFACE,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_BUTTON,.parent=0,.x=408,.y=400,.w=368,.h=64,.radius=16,.name="action/home",.text="返回主頁",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_ON_BRAND,.bg=STC_COLOR_BRAND,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_ICON,.parent=0,.x=40,.y=168,.w=48,.h=48,.radius=16,.name="cat/0",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=false,.asset=0},
  {.kind=STC_NODE_ICON,.parent=0,.x=424,.y=168,.w=48,.h=48,.radius=16,.name="cat/1",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=false,.asset=1},
  {.kind=STC_NODE_ICON,.parent=0,.x=40,.y=296,.w=48,.h=48,.radius=16,.name="cat/2",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=false,.asset=2},
  {.kind=STC_NODE_ICON,.parent=0,.x=424,.y=296,.w=48,.h=48,.radius=16,.name="cat/3",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=false,.asset=3},
};
static const stc_layout_node_t state_7[] = {
  {.kind=STC_NODE_BOX,.parent=-1,.x=0,.y=0,.w=800,.h=480,.radius=0,.name="state/learning",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_BOX,.parent=0,.x=0,.y=0,.w=800,.h=72,.radius=0,.name="comp/topbar",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BRAND,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=0,.x=24,.y=12,.w=352,.h=48,.radius=0,.name="brand",.text="分類小隊",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_ON_BRAND,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=0,.x=424,.y=12,.w=352,.h=48,.radius=0,.name="status",.text="分類小百科",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_ON_BRAND,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=0,.x=24,.y=88,.w=624,.h=80,.radius=0,.name="category",.text="可回收物",.font=&stc_font_tc_56,.font_px=56,.fg=STC_COLOR_CAT0,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_ICON,.parent=0,.x=656,.y=80,.w=96,.h=96,.radius=16,.name="cat/0",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=false,.asset=0},
  {.kind=STC_NODE_BOX,.parent=0,.x=24,.y=184,.w=752,.h=192,.radius=16,.name="comp/lesson",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_SURFACE,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=6,.x=24,.y=24,.w=704,.h=96,.radius=0,.name="tip",.text="先清空，再沖洗\n乾淨回收再造",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=6,.x=24,.y=128,.w=704,.h=40,.radius=0,.name="example",.text="例子：膠樽、鋁罐",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG_MUTED,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_BUTTON,.parent=0,.x=24,.y=400,.w=368,.h=64,.radius=16,.name="action/guide",.text="返回圖鑑",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_BRAND,.bg=STC_COLOR_SURFACE,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_BUTTON,.parent=0,.x=408,.y=400,.w=368,.h=64,.radius=16,.name="action/home",.text="返回主頁",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_ON_BRAND,.bg=STC_COLOR_BRAND,.has_bg=true,.asset=-1},
};
static const stc_layout_node_t state_8[] = {
  {.kind=STC_NODE_BOX,.parent=-1,.x=0,.y=0,.w=800,.h=480,.radius=0,.name="state/summary",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_BOX,.parent=0,.x=0,.y=0,.w=800,.h=72,.radius=0,.name="comp/topbar",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BRAND,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=0,.x=24,.y=12,.w=352,.h=48,.radius=0,.name="brand",.text="分類小隊",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_ON_BRAND,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=0,.x=424,.y=12,.w=352,.h=48,.radius=0,.name="status",.text="挑戰回顧",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_ON_BRAND,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=0,.x=24,.y=88,.w=624,.h=80,.radius=0,.name="prompt",.text="做得好，分類小達人！",.font=&stc_font_tc_56,.font_px=56,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_ICON,.parent=0,.x=656,.y=80,.w=96,.h=96,.radius=16,.name="icon/trophy",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=false,.asset=4},
  {.kind=STC_NODE_BOX,.parent=0,.x=24,.y=184,.w=368,.h=192,.radius=16,.name="comp/summary-0",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_SURFACE,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=6,.x=24,.y=24,.w=320,.h=40,.radius=0,.name="metric-label",.text="總分",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=6,.x=24,.y=72,.w=320,.h=96,.radius=0,.name="score",.text="120",.font=&stc_font_num_80,.font_px=80,.fg=STC_COLOR_BRAND,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_BOX,.parent=0,.x=408,.y=184,.w=368,.h=192,.radius=16,.name="comp/summary-1",.text="",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_SURFACE,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=9,.x=24,.y=24,.w=320,.h=40,.radius=0,.name="metric-label",.text="連續答對",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_FG,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_TEXT,.parent=9,.x=24,.y=72,.w=320,.h=96,.radius=0,.name="streak",.text="4",.font=&stc_font_num_80,.font_px=80,.fg=STC_COLOR_BRAND,.bg=STC_COLOR_BG,.has_bg=false,.asset=-1},
  {.kind=STC_NODE_BUTTON,.parent=0,.x=24,.y=400,.w=368,.h=64,.radius=16,.name="action/home",.text="返回主頁",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_BRAND,.bg=STC_COLOR_SURFACE,.has_bg=true,.asset=-1},
  {.kind=STC_NODE_BUTTON,.parent=0,.x=408,.y=400,.w=368,.h=64,.radius=16,.name="action/guide",.text="繼續學分類",.font=&stc_font_tc_32,.font_px=32,.fg=STC_COLOR_ON_BRAND,.bg=STC_COLOR_BRAND,.has_bg=true,.asset=-1},
};
const stc_layout_t stc_layouts[9] = {{state_0,sizeof(state_0)/sizeof(state_0[0])},{state_1,sizeof(state_1)/sizeof(state_1[0])},{state_2,sizeof(state_2)/sizeof(state_2[0])},{state_3,sizeof(state_3)/sizeof(state_3[0])},{state_4,sizeof(state_4)/sizeof(state_4[0])},{state_5,sizeof(state_5)/sizeof(state_5[0])},{state_6,sizeof(state_6)/sizeof(state_6[0])},{state_7,sizeof(state_7)/sizeof(state_7[0])},{state_8,sizeof(state_8)/sizeof(state_8[0])}};
const char *stc_tips[4] = {"先清空，再沖洗\n乾淨回收再造","先瀝走多餘水分\n不要混入膠袋","請交由大人處理\n送到專門回收點","紙巾不能當廢紙回收\n先減少使用更好"};
const char *stc_examples[4] = {"膠樽、鋁罐","果皮、剩飯","充電池","用過的紙巾"};
