#include "stc_ui.h"
#include "stc_layout.h"
#include "stc_tokens.h"
#include "src/misc/cache/instance/lv_image_cache.h"
#include "src/misc/lv_text_private.h" /* LVGL 9.5 native acceptance diagnostics. */
#include <math.h>
#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(STC_FONT_BODY);
LV_FONT_DECLARE(STC_FONT_CAPTION);
LV_FONT_DECLARE(STC_FONT_NUM_BODY);
LV_FONT_DECLARE(STC_FONT_SCORE);
#define NODE_CAPACITY 48
struct stc_ui {
    lv_obj_t *container, *objects[NODE_CAPACITY], *camera, *camera_image, *scan;
    const stc_layout_t *layout;
    const lv_image_dsc_t *original, *cutout;
    stc_retry_cb retry; stc_action_cb action; void *user;
    uint32_t score, streak; int category;
    stc_state_t state;
    bool reduced, acknowledged, pending_start;
    int previous_category;
};
static const char *state_names[] = {"idle","capturing","classifying","result_ok","result_retry","error","category_guide","learning","summary"};
#define LABEL(id, text) text,
static const char *categories[] = { STC_CATEGORY_LIST(LABEL) };
#undef LABEL
const char *stc_category_label(int id) { return id >= 0 && id < STC_CATEGORY_COUNT ? categories[id] : ""; }
const char *stc_ui_state_name(stc_state_t s) { return s >= STC_IDLE && s <= STC_SUMMARY ? state_names[s] : "error"; }
stc_state_t stc_ui_state_from_string(const char *s) {
    for(unsigned i=0; s && i<9; i++) if(strcmp(s,state_names[i])==0) return (stc_state_t)i;
    return STC_ERROR;
}
stc_state_t stc_ui_state(const stc_ui_t *ui) { return ui ? ui->state : STC_ERROR; }
static lv_obj_t *find(stc_ui_t *ui, const char *name) {
    for(unsigned i=0;i<ui->layout->count;i++) if(strcmp(ui->layout->nodes[i].name,name)==0) return ui->objects[i];
    return NULL;
}
static lv_obj_t *within(stc_ui_t *ui, const char *parent, const char *name) {
    for(unsigned i=0;i<ui->layout->count;i++) {
        const stc_layout_node_t *n=&ui->layout->nodes[i];
        if(n->parent>=0 && strcmp(n->name,name)==0 && strcmp(ui->layout->nodes[n->parent].name,parent)==0) return ui->objects[i];
    }
    return NULL;
}
static uint32_t category_color(int id) {
    static const uint32_t colors[]={STC_COLOR_CAT0,STC_COLOR_CAT1,STC_COLOR_CAT2,STC_COLOR_CAT3};
    return colors[id];
}
static void action_event(lv_event_t *e) {
    stc_ui_t *ui=lv_event_get_user_data(e);lv_obj_t *target=lv_event_get_target(e);
    for(unsigned i=0;i<ui->layout->count;i++) if(ui->objects[i]==target){
        const char *name=ui->layout->nodes[i].name;stc_action_t action;int category=-1;
        if(strcmp(name,"action/retry")==0){stc_ui_retry(ui);return;}
        if(strcmp(name,"action/start")==0)action=STC_ACTION_START;
        else if(strcmp(name,"action/home")==0)action=STC_ACTION_HOME;
        else if(strcmp(name,"action/guide")==0)action=STC_ACTION_GUIDE;
        else if(strcmp(name,"action/finish")==0)action=STC_ACTION_FINISH;
        else if(strncmp(name,"action/learn-",13)==0){category=name[13]-'0';action=STC_ACTION_LEARN;}
        else return;
        if(!ui->action)return;
        if(action==STC_ACTION_START){if(ui->pending_start)return;ui->pending_start=true;}
        stc_action_cb callback=ui->action;void *user=ui->user;
        callback(action,category,user);return;
    }
}
void stc_ui_set_action_callback(stc_ui_t *ui,stc_action_cb cb){if(ui)ui->action=cb;}
void stc_ui_show_guide(stc_ui_t *ui){if(ui){stc_payload_t p={.state=STC_CATEGORY_GUIDE,.score=ui->score,.streak=ui->streak};stc_ui_apply(ui,&p);}}
void stc_ui_show_learning(stc_ui_t *ui,int id){if(ui){stc_payload_t p={.state=STC_LEARNING,.category_id=id,.score=ui->score,.streak=ui->streak};stc_ui_apply(ui,&p);}}
void stc_ui_show_summary(stc_ui_t *ui){if(ui){stc_payload_t p={.state=STC_SUMMARY,.score=ui->score,.streak=ui->streak};stc_ui_apply(ui,&p);}}
void stc_ui_retry(stc_ui_t *ui) {
    if(!ui || !ui->container || ui->state!=STC_ERROR || ui->acknowledged) return;
    ui->acknowledged=true;
    if(ui->retry) ui->retry(ui->user);
}
static void scan_opacity(void *obj, int32_t value) { lv_obj_set_style_opa(obj,(lv_opa_t)value,0); }
static void animate(stc_ui_t *ui) {
    if(!ui->scan) return;
    lv_anim_delete(ui->scan,scan_opacity);
    lv_obj_set_style_opa(ui->scan,LV_OPA_COVER,0);
    if(ui->reduced) return;
    /* One opacity animation, 260 ms each direction, below the board's 300 ms limit. */
    lv_anim_t a;lv_anim_init(&a);lv_anim_set_var(&a,ui->scan);lv_anim_set_exec_cb(&a,scan_opacity);
    lv_anim_set_values(&a,128,255);lv_anim_set_duration(&a,STC_MOTION_SCAN_MS);
    lv_anim_set_reverse_duration(&a,STC_MOTION_SCAN_MS);lv_anim_set_repeat_count(&a,LV_ANIM_REPEAT_INFINITE);lv_anim_start(&a);
}
void stc_ui_set_reduced_motion(stc_ui_t *ui,bool reduced) { if(ui){ui->reduced=reduced;animate(ui);} }
static void image_update(stc_ui_t *ui) {
    if(!ui->camera) return;
    lv_obj_update_layout(ui->camera);
    const lv_image_dsc_t *source=(ui->state==STC_RESULT_OK && ui->cutout)?ui->cutout:ui->original;
    lv_obj_clean(ui->camera);ui->camera_image=NULL;
    if(source && source->header.w && source->header.h) {
        ui->camera_image=lv_image_create(ui->camera);lv_image_set_src(ui->camera_image,source);
        int w=lv_obj_get_width(ui->camera), h=lv_obj_get_height(ui->camera);
        uint32_t scale_x=(uint32_t)w*256/source->header.w, scale_y=(uint32_t)h*256/source->header.h;
        lv_image_set_scale(ui->camera_image,scale_x<scale_y?scale_x:scale_y);
        lv_obj_center(ui->camera_image);
    } else {
        lv_obj_set_flex_flow(ui->camera,LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(ui->camera,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(ui->camera,STC_SPACE_SM,0);
        lv_obj_t *icon=lv_image_create(ui->camera);lv_image_set_src(icon,&stc_camera_icon);
        lv_obj_set_style_image_recolor(icon,lv_color_hex(STC_COLOR_CAMERA_FG),0);lv_obj_set_style_image_recolor_opa(icon,LV_OPA_COVER,0);
        if(lv_obj_get_width(ui->camera)<240)return;
        lv_obj_t *label=lv_label_create(ui->camera);lv_label_set_text(label,"等待相機畫面");
        lv_obj_set_style_text_font(label,&STC_FONT_BODY,0);lv_obj_set_style_text_color(label,lv_color_hex(STC_COLOR_CAMERA_FG),0);
    }
}
void stc_ui_set_images(stc_ui_t *ui,const lv_image_dsc_t *original,const lv_image_dsc_t *cutout) {
    if(ui && ui->container){if(ui->original)lv_image_cache_drop(ui->original);if(ui->cutout)lv_image_cache_drop(ui->cutout);ui->original=original;ui->cutout=cutout;image_update(ui);}
}
static void container_deleted(lv_event_t *event){
    stc_ui_t *ui=lv_event_get_user_data(event);
    if(ui->scan)lv_anim_delete(ui->scan,scan_opacity);
    ui->container=NULL;ui->scan=NULL;ui->camera=NULL;
}
static void render(stc_ui_t *ui,stc_state_t state) {
    if(ui->scan) lv_anim_delete(ui->scan,scan_opacity);
    lv_obj_clean(ui->container);memset(ui->objects,0,sizeof(ui->objects));ui->camera=NULL;ui->camera_image=NULL;ui->scan=NULL;
    ui->state=state;ui->layout=&stc_layouts[state];
    for(unsigned i=0;i<ui->layout->count;i++) {
        const stc_layout_node_t *n=&ui->layout->nodes[i];
        lv_obj_t *parent=n->parent<0?ui->container:ui->objects[n->parent], *obj;
        if(n->kind==STC_NODE_TEXT) obj=lv_label_create(parent);
        else if(n->kind==STC_NODE_ICON) obj=lv_image_create(parent);
        else if(n->kind==STC_NODE_BUTTON) obj=lv_button_create(parent);
        else obj=lv_obj_create(parent);
        ui->objects[i]=obj;lv_obj_remove_style_all(obj);lv_obj_set_pos(obj,n->x,n->y);lv_obj_set_size(obj,n->w,n->h);
        lv_obj_remove_flag(obj,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_style_radius(obj,n->radius,0);
        if(n->has_bg || n->kind==STC_NODE_CAMERA) {
            lv_obj_set_style_bg_color(obj,lv_color_hex(n->kind==STC_NODE_CAMERA?STC_COLOR_CAMERA_BG:n->bg),0);
            lv_obj_set_style_bg_opa(obj,LV_OPA_COVER,0);
        }
        if(n->kind==STC_NODE_TEXT) {
            lv_obj_set_style_text_font(obj,n->font,0);lv_obj_set_style_text_color(obj,lv_color_hex(n->fg),0);
            lv_obj_set_style_text_letter_space(obj,0,0);
            int line_space=(n->font_px*14+5)/10-lv_font_get_line_height(n->font);
            lv_obj_set_style_text_line_space(obj,line_space>0?line_space:0,0);lv_label_set_long_mode(obj,LV_LABEL_LONG_WRAP);lv_label_set_text(obj,n->text);
        } else if(n->kind==STC_NODE_ICON) {
            lv_image_set_src(obj,n->asset<4?stc_icons[n->asset][n->w>48]:&stc_trophy_icon);
            lv_obj_set_style_image_recolor(obj,lv_color_hex((n->asset<4?category_color(n->asset):STC_COLOR_BRAND)),0);lv_obj_set_style_image_recolor_opa(obj,LV_OPA_COVER,0);
        } else if(n->kind==STC_NODE_BUTTON) {
            lv_obj_set_style_bg_color(obj,lv_color_hex(n->bg),0);lv_obj_set_style_bg_opa(obj,LV_OPA_COVER,0);
            lv_obj_set_style_outline_width(obj,3,LV_STATE_FOCUSED);lv_obj_set_style_outline_color(obj,lv_color_hex(STC_COLOR_FG),LV_STATE_FOCUSED);
            lv_obj_t *label=lv_label_create(obj);lv_obj_set_style_text_font(label,&STC_FONT_BODY,0);
            lv_obj_set_style_text_color(label,lv_color_hex(n->fg),0);lv_label_set_text(label,n->text);lv_obj_center(label);
            lv_obj_add_event_cb(obj,action_event,LV_EVENT_CLICKED,ui);
        } else if(n->kind==STC_NODE_CAMERA) ui->camera=obj;
        if(strcmp(n->name,"scan")==0)ui->scan=obj;
    }
    image_update(ui);animate(ui);
}
stc_ui_t *stc_ui_create(lv_obj_t *parent,stc_retry_cb retry,void *user) {
    if(!parent)return NULL;
    lv_obj_update_layout(parent);
    if(lv_obj_get_content_width(parent)!=800 || lv_obj_get_content_height(parent)!=480)return NULL;
    stc_ui_t *ui=lv_malloc_zeroed(sizeof(*ui));if(!ui)return NULL;
    ui->retry=retry;ui->user=user;ui->previous_category=-1;
    ui->container=lv_obj_create(parent);lv_obj_remove_style_all(ui->container);lv_obj_set_size(ui->container,800,480);
    lv_obj_remove_flag(ui->container,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(ui->container,container_deleted,LV_EVENT_DELETE,ui);render(ui,STC_IDLE);
    lv_label_set_text(find(ui,"score"),"0");return ui;
}
void stc_ui_destroy(stc_ui_t *ui) {
    if(!ui)return;
    if(ui->scan)lv_anim_delete(ui->scan,scan_opacity);
    if(ui->container)lv_obj_delete(ui->container);
    /* Caller may release descriptors/pixels immediately after destroy. */
    if(ui->original)lv_image_cache_drop(ui->original);
    if(ui->cutout)lv_image_cache_drop(ui->cutout);
    lv_free(ui);
}
static bool valid_category(int id) {return id>=0 && id<STC_CATEGORY_COUNT;}
static bool probability(float v) {return isfinite(v)&&v>=0.0f&&v<=1.0f;}
static bool transition(stc_ui_t *ui,stc_state_t to) {
    if(to==STC_ERROR)return true;
    if(ui->state==STC_ERROR)return ui->acknowledged&&to==STC_IDLE;
    if(to==ui->state)return true;
    switch(ui->state){
    case STC_IDLE:return to==STC_CAPTURING;
    case STC_CAPTURING:return to==STC_CLASSIFYING;
    case STC_CLASSIFYING:return to==STC_RESULT_OK||to==STC_RESULT_RETRY;
    case STC_RESULT_OK:return to==STC_IDLE||to==STC_SUMMARY;
    case STC_RESULT_RETRY:return to==STC_IDLE||to==STC_CAPTURING;
    case STC_CATEGORY_GUIDE:return to==STC_IDLE||to==STC_LEARNING||to==STC_SUMMARY;
    case STC_LEARNING:return to==STC_IDLE||to==STC_CATEGORY_GUIDE;
    case STC_SUMMARY:return to==STC_IDLE||to==STC_CATEGORY_GUIDE;
    default:return false;
    }
}
static void error(stc_ui_t *ui,const char *code) {
    render(ui,STC_ERROR);ui->acknowledged=false;ui->pending_start=false;
    const char *title="暫時未能辨識", *hint="請檢查裝置，再按重試";
    if(code && strcmp(code,"CAMERA_OFFLINE")==0){title="相機未就緒";hint="請檢查相機，再按重試";}
    else if(code && strcmp(code,"NETWORK_OFFLINE")==0){title="連線中斷";hint="請檢查網絡，再按重試";}
    lv_label_set_text(find(ui,"prompt"),title);lv_label_set_text(find(ui,"hint"),hint);
    char ascii[25];unsigned i=0;for(;code&&code[i]&&i<sizeof(ascii)-1;i++)ascii[i]=(code[i]>=32&&code[i]<=126)?code[i]:'?';ascii[i]=0;
    lv_label_set_text(find(ui,"error-code"),i?ascii:"UNKNOWN_ERROR");
}
static void count_text(lv_obj_t *obj,uint32_t value) {
    char text[32];snprintf(text,sizeof(text),"%lu",(unsigned long)value);lv_label_set_text(obj,text);
    /* Keep all digits on screen for full uint32 counters, using the existing number font. */
    lv_obj_set_style_text_font(obj,value>99999?&STC_FONT_NUM_BODY:&STC_FONT_SCORE,0);
}
bool stc_ui_apply(stc_ui_t *ui,const stc_payload_t *p) {
    if(!ui || !ui->container)return false;
    if(!p || p->state<STC_IDLE || p->state>STC_SUMMARY){error(ui,"INVALID_STATE");return false;}
    if(!transition(ui,p->state)){error(ui,"INVALID_TRANSITION");return false;}
    if(p->state==STC_RESULT_OK && (!valid_category(p->category_id)||!probability(p->confidence))){error(ui,"INVALID_RESULT");return false;}
    if(p->state==STC_RESULT_RETRY && (!valid_category(p->candidates[0].category_id)||!valid_category(p->candidates[1].category_id)||p->candidates[0].category_id==p->candidates[1].category_id||!probability(p->candidates[0].p)||!probability(p->candidates[1].p))){error(ui,"INVALID_CANDIDATES");return false;}
    if(p->state==STC_ERROR){error(ui,p->error_code);return true;}
    if(p->state==STC_LEARNING && !valid_category(p->category_id)){error(ui,"INVALID_CATEGORY");return false;}
    ui->score=p->score;ui->streak=p->streak;if(p->state!=ui->state)ui->pending_start=false;
    if(p->state!=ui->state)render(ui,p->state);
    if(p->state==STC_IDLE){ui->acknowledged=false;count_text(find(ui,"score"),p->score);}
    if(p->state==STC_CLASSIFYING){char previous[64];if(ui->previous_category<0)snprintf(previous,sizeof(previous),"上一輪：尚無結果");else snprintf(previous,sizeof(previous),"上一輪：%s",stc_category_label(ui->previous_category));lv_label_set_text(find(ui,"previous"),previous);}
    if(p->state==STC_RESULT_OK){
        lv_obj_t *cat=find(ui,"category");lv_label_set_text(cat,stc_category_label(p->category_id));lv_obj_set_style_text_color(cat,lv_color_hex(category_color(p->category_id)),0);
        lv_obj_t *icon=find(ui,"cat/0");lv_image_set_src(icon,stc_icons[p->category_id][1]);lv_obj_set_style_image_recolor(icon,lv_color_hex(category_color(p->category_id)),0);
        char text[80];snprintf(text,sizeof(text),"信心值 %.0f%%",(double)p->confidence*100);lv_label_set_text(find(ui,"confidence"),text);
        count_text(find(ui,"score"),p->score);lv_obj_t *status=find(ui,"status");
        if(p->streak>99){snprintf(text,sizeof(text),"連對%lu",(unsigned long)p->streak);lv_obj_set_style_text_font(status,&STC_FONT_CAPTION,0);}
        else{snprintf(text,sizeof(text),"連對 %lu 次",(unsigned long)p->streak);lv_obj_set_style_text_font(status,&STC_FONT_BODY,0);}
        lv_label_set_text(status,text);
        /* Issue payload supplies total score, not a per-round award. Never invent +10. */
        lv_label_set_text(find(ui,"points"),"目前分數");
        lv_label_set_text(find(ui,"instruction"),p->lid_open?"桶蓋已開，請投放":"等待開蓋，請勿推動桶蓋");
        lv_label_set_text(find(ui,"tip"),stc_tips[p->category_id]);ui->previous_category=p->category_id;ui->category=p->category_id;
    }
    if(p->state==STC_RESULT_RETRY){for(int i=0;i<2;i++){char parent[24],text[24],iconname[16];snprintf(parent,sizeof(parent),"candidate/%d",i);int id=p->candidates[i].category_id;lv_label_set_text(within(ui,parent,"category"),stc_category_label(id));snprintf(text,sizeof(text),"%.0f%%",(double)p->candidates[i].p*100);lv_label_set_text(within(ui,parent,"probability"),text);snprintf(iconname,sizeof(iconname),"cat/%d",i==0?0:3);lv_obj_t *icon=within(ui,parent,iconname);lv_image_set_src(icon,stc_icons[id][0]);lv_obj_set_style_image_recolor(icon,lv_color_hex(category_color(id)),0);}}
    if(p->state==STC_LEARNING){
        lv_obj_t *cat=find(ui,"category");lv_label_set_text(cat,stc_category_label(p->category_id));lv_obj_set_style_text_color(cat,lv_color_hex(category_color(p->category_id)),0);
        lv_obj_t *icon=find(ui,"cat/0");lv_image_set_src(icon,stc_icons[p->category_id][1]);lv_obj_set_style_image_recolor(icon,lv_color_hex(category_color(p->category_id)),0);
        lv_label_set_text(find(ui,"tip"),stc_tips[p->category_id]);char text[96];snprintf(text,sizeof(text),"例子：%s",stc_examples[p->category_id]);lv_label_set_text(find(ui,"example"),text);
    }
    if(p->state==STC_SUMMARY){count_text(find(ui,"score"),p->score);count_text(find(ui,"streak"),p->streak);}
    return true;
}
static bool check_rendered_text(lv_obj_t *obj) {
    bool ok=true;
    if(lv_obj_check_type(obj,&lv_label_class)){
        const char *text=lv_label_get_text(obj);const lv_font_t *font=lv_obj_get_style_text_font(obj,0);
        uint32_t index=0;while(text[index]){
            uint32_t codepoint=lv_text_encoded_next(text,&index);
            if(codepoint=='\n'||codepoint=='\r'||codepoint=='\t')continue;
            lv_font_glyph_dsc_t glyph={0};
            if(!lv_font_get_glyph_dsc(font,&glyph,codepoint,0)||glyph.is_placeholder){fprintf(stderr,"MISSING_GLYPH U+%04lx\n",(unsigned long)codepoint);ok=false;}
        }
        lv_area_t bounds,parent_bounds;lv_obj_get_coords(obj,&bounds);lv_obj_get_coords(lv_obj_get_parent(obj),&parent_bounds);
        if(bounds.x1<parent_bounds.x1||bounds.y1<parent_bounds.y1||bounds.x2>parent_bounds.x2||bounds.y2>parent_bounds.y2){fprintf(stderr,"LABEL_OUTSIDE_PARENT: %s\n",text);ok=false;}
    }
    for(uint32_t i=0;i<lv_obj_get_child_count(obj);i++)if(!check_rendered_text(lv_obj_get_child(obj,i)))ok=false;
    return ok;
}
bool stc_ui_check_layout(stc_ui_t *ui) {
    if(!ui || !ui->container)return false;
    lv_obj_update_layout(ui->container);bool ok=true;
    for(unsigned i=0;i<ui->layout->count;i++){
        lv_obj_t *obj=ui->objects[i];const stc_layout_node_t *n=&ui->layout->nodes[i];lv_area_t bounds;lv_obj_get_coords(obj,&bounds);
        if(bounds.x1<0 || bounds.y1<0 || bounds.x2>=800 || bounds.y2>=480){fprintf(stderr,"OUT_OF_FRAME %s [%ld,%ld,%ld,%ld]\n",n->name,(long)bounds.x1,(long)bounds.y1,(long)bounds.x2,(long)bounds.y2);ok=false;}
        if(n->kind==STC_NODE_TEXT){lv_point_t size;lv_text_get_size(&size,lv_label_get_text(obj),lv_obj_get_style_text_font(obj,0),0,lv_obj_get_style_text_line_space(obj,0),n->w,LV_TEXT_FLAG_NONE);if(size.y>n->h){fprintf(stderr,"TEXT_OVERFLOW %s actual=%ld available=%d\n",n->name,(long)size.y,n->h);ok=false;}}
    }return check_rendered_text(ui->container) && ok;
}
