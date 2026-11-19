#include "stc_ui.h"
#include "src/misc/cache/instance/lv_image_cache.h"
#include "src/draw/lv_image_decoder_private.h"
#include "src/misc/lv_iter.h"
#include "src/misc/cache/lv_cache_entry.h"
#include <SDL.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static stc_ui_t *ui;
static unsigned retries, asks;
static bool reduced, toggle_motion, retry_command;
static unsigned demo_stage, demo_category;static uint32_t due;
static int requested=-1, selected_bin=-1;
static int event_watch(void *user, SDL_Event *e){
    (void)user;
    if(e->type==SDL_KEYDOWN && !e->key.repeat){
        SDL_Keycode key=e->key.keysym.sym;
        if(key>=SDLK_1 && key<=SDLK_9) requested=(int)(key-SDLK_1);
        else if(key==SDLK_0) requested=STC_SCAN_READY;
        else if(key==SDLK_m) toggle_motion=true;
        else if(key==SDLK_r) retry_command=true;
    }return 1;
}
static void retry(void *unused){(void)unused;retries++;}
static void sdl_hal_init(int width,int height){
    lv_init();lv_display_t *d=lv_sdl_window_create(width,height);assert(d);
    lv_sdl_window_set_title(d,"Smart Trash Can | REPLAY | 1-9 states, 0 scan ready, M motion, R retry");
    lv_indev_t *mouse=lv_sdl_mouse_create();lv_indev_set_display(mouse,d);
    lv_indev_t *keyboard=lv_sdl_keyboard_create();lv_group_t *group=lv_group_create();
    lv_group_set_default(group);lv_indev_set_group(keyboard,group);
    lv_obj_t *screen=lv_screen_active();lv_obj_remove_style_all(screen);
    ui=stc_ui_create(screen,retry,NULL);assert(ui);
}
static stc_payload_t payload(stc_state_t state){
    stc_payload_t p={.state=state,.category_id=0,.confidence=.93f,.score=120,.streak=4,
        .candidates={{0,.52f},{3,.43f}},.error_code="CAMERA_OFFLINE",
        .round=3,.rounds_total=5,.debug_flags=STC_DEBUG_REPLAY|STC_DEBUG_VIRTUAL_HAND,
        .learning={.scanned_bin=0,.dropped_bin=1,.correct_bin=0,
                   .outcome=STC_ROUND_WRONG,.round_delta=-5}};return p;
}
static void show(stc_state_t state,int category){
    stc_payload_t p=payload(STC_IDLE);
    if(stc_ui_state(ui)==STC_ERROR){stc_ui_retry(ui);assert(stc_ui_apply(ui,&p));}
    stc_state_t current=stc_ui_state(ui);
    if(current==STC_CAPTURING){p=payload(STC_CLASSIFYING);assert(stc_ui_apply(ui,&p));current=STC_CLASSIFYING;}
    if(current==STC_CLASSIFYING){p=payload(STC_RESULT_RETRY);assert(stc_ui_apply(ui,&p));current=STC_RESULT_RETRY;}
    if(current==STC_RESULT_RETRY){p=payload(STC_IDLE);assert(stc_ui_apply(ui,&p));}
    else if(current==STC_SCAN_READY||current==STC_RESULT_OK||current==STC_BIN_CHOICE||current==STC_LEARNING||current==STC_SUMMARY||current==STC_CATEGORY_GUIDE||current==STC_CHAT){
        p=payload(STC_IDLE);assert(stc_ui_apply(ui,&p));
    }
    if(state==STC_ERROR){p=payload(state);assert(stc_ui_apply(ui,&p));return;}
    if(state==STC_IDLE){return;}
    if(state==STC_CHAT){p=payload(STC_CHAT);assert(stc_ui_apply(ui,&p));return;}
    p=payload(STC_SCAN_READY);assert(stc_ui_apply(ui,&p));if(state==STC_SCAN_READY)return;
    p=payload(STC_CAPTURING);assert(stc_ui_apply(ui,&p));if(state==STC_CAPTURING)return;
    p=payload(STC_CLASSIFYING);assert(stc_ui_apply(ui,&p));if(state==STC_CLASSIFYING)return;
    p=payload(state==STC_RESULT_RETRY?STC_RESULT_RETRY:STC_RESULT_OK);
    if(state!=STC_RESULT_RETRY){p.category_id=category;p.learning.scanned_bin=category;p.learning.correct_bin=category;}
    assert(stc_ui_apply(ui,&p));
    if(state==STC_RESULT_OK||state==STC_RESULT_RETRY)return;
    if(state==STC_BIN_CHOICE){p=payload(STC_BIN_CHOICE);assert(stc_ui_apply(ui,&p));return;}
    p=payload(STC_LEARNING);p.category_id=category;p.learning.scanned_bin=category;p.learning.correct_bin=category;
    assert(stc_ui_apply(ui,&p));
    if(state==STC_LEARNING)return;
    p=payload(STC_SUMMARY);assert(stc_ui_apply(ui,&p));
    if(state==STC_SUMMARY)return;
    p=payload(STC_CATEGORY_GUIDE);assert(stc_ui_apply(ui,&p));
}
static void demo_action(stc_action_t action,int category,void *user){
    (void)category;(void)user;
    if(action==STC_ACTION_ASK){asks++;return;}
    if(action==STC_ACTION_START){show(STC_SCAN_READY,0);demo_stage=0;}
    else if(action==STC_ACTION_SCAN){show(STC_CAPTURING,0);demo_stage=1;due=SDL_GetTicks()+1200;}
    else if(action==STC_ACTION_NEXT){show(STC_SCAN_READY,0);demo_stage=0;}
    else if(action==STC_ACTION_SELECT_BIN){selected_bin=category;}
    else {
        demo_stage=0;
        if(action==STC_ACTION_GUIDE)stc_ui_show_guide(ui);
        else if(action==STC_ACTION_LEARN)stc_ui_show_learning(ui,category);
        else if(action==STC_ACTION_FINISH)stc_ui_show_summary(ui);
        else if(action==STC_ACTION_HOME){stc_payload_t p=payload(STC_IDLE);stc_ui_apply(ui,&p);}
    }
}
static bool screenshot(const char *path){
    for(int i=0;i<3;i++){lv_timer_handler();SDL_Delay(20);}lv_refr_now(lv_display_get_default());
    SDL_Renderer *renderer=lv_sdl_window_get_renderer(lv_display_get_default());int w,h;
    if(!renderer||SDL_GetRendererOutputSize(renderer,&w,&h)!=0)return false;
    SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_ARGB8888);
    if(!surface)return false;
    bool ok=SDL_RenderReadPixels(renderer,NULL,surface->format->format,surface->pixels,surface->pitch)==0;
    if(ok)ok=SDL_SaveBMP(surface,path)==0;
    SDL_FreeSurface(surface);return ok;
}
static lv_obj_t *button_named(lv_obj_t *parent,const char *text){
    for(uint32_t i=0;i<lv_obj_get_child_count(parent);i++){
        lv_obj_t *child=lv_obj_get_child(parent,i);
        if(lv_obj_check_type(child,&lv_button_class)){
            lv_obj_t *label=lv_obj_get_child(child,0);
            if(label && lv_obj_check_type(label,&lv_label_class) && strcmp(lv_label_get_text(label),text)==0)return child;
        }
        lv_obj_t *result=button_named(child,text);if(result)return result;
    }return NULL;
}
static unsigned button_count(lv_obj_t *parent){
    unsigned count=0;
    for(uint32_t i=0;i<lv_obj_get_child_count(parent);i++){
        lv_obj_t *child=lv_obj_get_child(parent,i);
        if(lv_obj_check_type(child,&lv_button_class))count++;
        count+=button_count(child);
    }return count;
}
static lv_obj_t *label_named(lv_obj_t *parent,const char *text){
    for(uint32_t i=0;i<lv_obj_get_child_count(parent);i++){
        lv_obj_t *child=lv_obj_get_child(parent,i);
        if(lv_obj_check_type(child,&lv_label_class)&&strcmp(lv_label_get_text(child),text)==0)return child;
        lv_obj_t *result=label_named(child,text);if(result)return result;
    }return NULL;
}
static void dump_labels(lv_obj_t *parent){
    for(uint32_t i=0;i<lv_obj_get_child_count(parent);i++){
        lv_obj_t *child=lv_obj_get_child(parent,i);
        if(lv_obj_check_type(child,&lv_label_class))fprintf(stderr,"LABEL: %s\n",lv_label_get_text(child));
        dump_labels(child);
    }
}
static void press_button(const char *text){lv_obj_update_layout(lv_screen_active());lv_obj_t *button=button_named(lv_screen_active(),text);assert(button);lv_obj_send_event(button,LV_EVENT_CLICKED,NULL);}
static bool image_cached(const void *source){
    lv_iter_t *iter=lv_image_cache_iter_create();assert(iter);
    /* Cache iteration copies data plus its entry metadata. */
    void *entry=lv_malloc(lv_cache_entry_get_size(sizeof(lv_image_cache_data_t)));assert(entry);bool found=false;
    while(lv_iter_next(iter,entry)==LV_RESULT_OK)if(((lv_image_cache_data_t *)entry)->src==source)found=true;
    lv_free(entry);lv_iter_destroy(iter);return found;
}
static int self_test(void){
    unsigned cases=0;stc_ui_set_reduced_motion(ui,true);
    for(int s=0;s<12;s++){show((stc_state_t)s,0);assert(stc_ui_state(ui)==(stc_state_t)s);assert(stc_ui_check_layout(ui));cases++;}
    show(STC_SCAN_READY,0);stc_payload_t p=payload(STC_SCAN_READY);p.debug_flags=
        STC_DEBUG_LIVE_CAMERA|STC_DEBUG_DEV_FLOW|STC_DEBUG_VIRTUAL_HAND;
    assert(stc_ui_apply(ui,&p));
    if(!label_named(lv_screen_active(),"DEV 39/40 | LIVE CAMERA | VIRTUAL HAND"))dump_labels(lv_screen_active());
    assert(label_named(lv_screen_active(),"DEV 39/40 | LIVE CAMERA | VIRTUAL HAND"));cases++;
    p=payload(STC_SCAN_READY);p.debug_flags=
        STC_DEBUG_LIVE_CAMERA|STC_DEBUG_DEV_FLOW|STC_DEBUG_TOUCH_BINS;
    assert(stc_ui_apply(ui,&p));
    assert(label_named(lv_screen_active(),"DEV 39/40 | LIVE CAMERA | TOUCH BINS"));cases++;
    show(STC_BIN_CHOICE,0);selected_bin=-1;press_button("廚餘");
    assert(selected_bin==1);assert(button_count(lv_screen_active())==4);cases++;
    show(STC_CLASSIFYING,0);p=payload(STC_BIN_CHOICE);
    p.debug_flags=STC_DEBUG_REPLAY|STC_DEBUG_REAL_HAND;
    assert(stc_ui_apply(ui,&p));
    lv_obj_t *real_bin_button=button_named(lv_screen_active(),"可回收物");
    assert(real_bin_button && lv_obj_has_flag(real_bin_button,LV_OBJ_FLAG_HIDDEN));
    assert(label_named(lv_screen_active(),"請將卡投入桶內"));cases++;
    show(STC_CLASSIFYING,0);
    assert(label_named(lv_screen_active(),"你估會係邊一類？"));cases++;
    for(int cat=0;cat<4;cat++){show(STC_RESULT_OK,cat);assert(stc_ui_check_layout(ui));show(STC_LEARNING,cat);assert(stc_ui_check_layout(ui));cases+=2;}
    for(int a=0;a<4;a++)for(int b=0;b<4;b++)if(a!=b){show(STC_CLASSIFYING,0);stc_payload_t p=payload(STC_RESULT_RETRY);p.candidates[0].category_id=a;p.candidates[1].category_id=b;assert(stc_ui_apply(ui,&p));assert(stc_ui_check_layout(ui));cases++;}
    show(STC_IDLE,0);p=payload(STC_RESULT_OK);assert(!stc_ui_apply(ui,&p));assert(stc_ui_state(ui)==STC_ERROR);cases++;
    p.state=STC_IDLE;assert(!stc_ui_apply(ui,&p));stc_ui_retry(ui);assert(stc_ui_apply(ui,&p));cases++;
    show(STC_CLASSIFYING,0);p=payload(STC_BIN_CHOICE);
    assert(stc_ui_apply(ui,&p));assert(stc_ui_state(ui)==STC_BIN_CHOICE);cases++;
    show(STC_SCAN_READY,0);p=payload(STC_LEARNING);
    assert(stc_ui_apply(ui,&p));assert(stc_ui_state(ui)==STC_LEARNING);cases++;
    show(STC_CAPTURING,0);p=payload(STC_SCAN_READY);
    assert(stc_ui_apply(ui,&p));assert(stc_ui_state(ui)==STC_SCAN_READY);cases++;
    show(STC_CAPTURING,0);p=payload(STC_LEARNING);
    assert(stc_ui_apply(ui,&p));assert(stc_ui_state(ui)==STC_LEARNING);cases++;
    show(STC_CLASSIFYING,0);p=payload(STC_RESULT_OK);p.confidence=NAN;assert(!stc_ui_apply(ui,&p));cases++;
    show(STC_CLASSIFYING,0);p=payload(STC_RESULT_OK);p.category_id=4;assert(!stc_ui_apply(ui,&p));cases++;
    show(STC_CLASSIFYING,0);p=payload(STC_RESULT_RETRY);p.candidates[1].category_id=0;assert(!stc_ui_apply(ui,&p));cases++;
    show(STC_IDLE,0);p=payload((stc_state_t)77);assert(!stc_ui_apply(ui,&p));cases++;
    for(int s=0;s<12;s++)assert(stc_ui_state_from_string(stc_ui_state_name((stc_state_t)s))==(stc_state_t)s);
    assert(stc_ui_state_from_string("stats")==STC_ERROR);assert(stc_ui_state_from_string(NULL)==STC_ERROR);cases++;
    static const uint16_t pixels[4]={0xf800,0x07e0,0x001f,0xffff};
    static const lv_image_dsc_t original={.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565,.w=2,.h=2,.stride=4},.data_size=sizeof(pixels),.data=(const uint8_t*)pixels};
    show(STC_CAPTURING,0);stc_ui_set_images(ui,&original,NULL);lv_refr_now(lv_display_get_default());stc_ui_set_images(ui,NULL,NULL);assert(stc_ui_check_layout(ui));cases++;
    show(STC_CLASSIFYING,0);stc_ui_set_reduced_motion(ui,false);assert(lv_anim_count_running()==1);stc_ui_set_reduced_motion(ui,true);assert(lv_anim_count_running()==0);cases++;
    show(STC_IDLE,0);assert(button_named(lv_screen_active(),"分類圖鑑"));
    press_button("分類圖鑑");assert(stc_ui_state(ui)==STC_CATEGORY_GUIDE);
    unsigned ask_before=asks;press_button("問綠豆");assert(asks==ask_before+1);cases++;
    show(STC_SUMMARY,0);assert(label_named(lv_screen_active(),"做得好，分類小達人！"));press_button("繼續學分類");assert(stc_ui_state(ui)==STC_CATEGORY_GUIDE);
    assert(button_named(lv_screen_active(),"返回主頁"));
    /* Guide cards use SVG/text overlays; dispatch the underlying card's click event. */
    lv_obj_update_layout(lv_screen_active());
    lv_obj_t *root=lv_obj_get_child(lv_obj_get_child(lv_screen_active(),0),0);
    lv_obj_t *card=NULL;for(uint32_t i=0;i<lv_obj_get_child_count(root);i++){lv_obj_t *child=lv_obj_get_child(root,i);if(lv_obj_check_type(child,&lv_button_class)&&lv_obj_get_x(child)==24&&lv_obj_get_y(child)==136){card=child;break;}}
    assert(card);lv_obj_send_event(card,LV_EVENT_CLICKED,NULL);assert(stc_ui_state(ui)==STC_LEARNING);
    assert(button_count(lv_screen_active())==1);assert(button_named(lv_screen_active(),"下一回合"));
    assert(!button_named(lv_screen_active(),"返回主頁"));assert(!button_named(lv_screen_active(),"返回圖鑑"));
    press_button("下一回合");assert(stc_ui_state(ui)==STC_SCAN_READY);cases++;
    show(STC_LEARNING,3);assert(button_count(lv_screen_active())==1);press_button("下一回合");assert(stc_ui_state(ui)==STC_SCAN_READY);cases++;
    show(STC_LEARNING,1);p=payload(STC_CAPTURING);assert(!stc_ui_apply(ui,&p));cases++;
    const stc_round_outcome_t learning_outcomes[]={
        STC_ROUND_CORRECT,STC_ROUND_WRONG,STC_ROUND_TIMEOUT,
        STC_ROUND_ABSTAIN,STC_ROUND_NO_SCAN};
    for(unsigned i=0;i<sizeof(learning_outcomes)/sizeof(learning_outcomes[0]);i++){
        show(STC_RESULT_OK,0);p=payload(STC_LEARNING);
        p.learning.outcome=learning_outcomes[i];
        if(p.learning.outcome==STC_ROUND_CORRECT){p.learning.dropped_bin=0;p.learning.round_delta=10;}
        else if(p.learning.outcome==STC_ROUND_TIMEOUT){p.learning.dropped_bin=-1;p.learning.round_delta=0;}
        else if(p.learning.outcome==STC_ROUND_ABSTAIN||p.learning.outcome==STC_ROUND_NO_SCAN){
            p.learning.scanned_bin=-1;p.learning.correct_bin=-1;
            p.learning.dropped_bin=-1;p.learning.round_delta=0;
        }
        assert(stc_ui_apply(ui,&p));assert(stc_ui_check_layout(ui));cases++;
    }
    show(STC_RESULT_OK,0);assert(stc_ui_check_layout(ui));cases++;
    show(STC_ERROR,0);unsigned before=retries;press_button("重試");press_button("重試");assert(retries==before+1);
    show(STC_ERROR,0);before=retries;press_button("返回主頁");assert(retries==before+1);cases++;
    show(STC_CLASSIFYING,0);p=payload(STC_RESULT_OK);p.score=UINT32_MAX;p.streak=UINT32_MAX;assert(stc_ui_apply(ui,&p));assert(stc_ui_check_layout(ui));stc_ui_show_summary(ui);assert(stc_ui_check_layout(ui));cases++;
    show(STC_IDLE,0);lv_obj_t *missing=lv_label_create(lv_obj_get_child(lv_screen_active(),0));lv_label_set_text(missing,"🦄");
    assert(!stc_ui_check_layout(ui));lv_obj_delete(missing);assert(stc_ui_check_layout(ui));cases++;
    for(int i=0;i<150;i++){show(STC_RESULT_OK,i%4);show(STC_RESULT_RETRY,0);}cases++;
    {
        lv_image_cache_resize(8192,true);
        lv_obj_t *image_parent=lv_obj_create(lv_screen_active());lv_obj_remove_style_all(image_parent);lv_obj_set_size(image_parent,800,480);
        stc_ui_t *image_ui=stc_ui_create(image_parent,NULL,NULL);assert(image_ui);
        static const uint32_t alpha_pixels[4]={0x80800000,0x80008000,0x80000080,0x80808080};
        static const lv_image_dsc_t alpha_image={.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_ARGB8888,.w=2,.h=2,.stride=8},.data_size=sizeof(alpha_pixels),.data=(const uint8_t*)alpha_pixels};
        stc_ui_set_images(image_ui,&alpha_image,NULL);
        lv_image_decoder_dsc_t decoded;lv_image_decoder_args_t options={.premultiply=true};
        assert(lv_image_decoder_open(&decoded,&alpha_image,&options)==LV_RESULT_OK);lv_image_decoder_close(&decoded);
        assert(image_cached(&alpha_image));
        stc_ui_destroy(image_ui);assert(!image_cached(&alpha_image));lv_obj_delete(image_parent);
        lv_image_cache_resize(0,true);cases++;
    }
    lv_obj_t *parent=lv_obj_create(lv_screen_active());lv_obj_remove_style_all(parent);lv_obj_set_size(parent,800,480);
    stc_ui_t *temporary=stc_ui_create(parent,NULL,NULL);assert(temporary);lv_obj_delete(parent);p=payload(STC_IDLE);assert(!stc_ui_apply(temporary,&p));stc_ui_destroy(temporary);cases++;
    printf("PASS %u state/data/layout/motion/image/lifecycle scenarios; twelve states and four categories\n",cases);return 0;
}
int main(int argc,char **argv){
    SDL_SetHint(SDL_HINT_RENDER_DRIVER,"software");
    sdl_hal_init(800, 480);
    stc_ui_set_action_callback(ui,demo_action);
  lv_display_set_rotation(lv_display_get_default(), LV_DISPLAY_ROTATION_0);
    if(argc>1&&strcmp(argv[1],"--self-test")==0){int result=self_test();stc_ui_destroy(ui);lv_deinit();return result;}
    if(argc>2&&strcmp(argv[1],"--screenshots")==0){char path[1024];stc_ui_set_reduced_motion(ui,true);
        for(int s=0;s<12;s++){show((stc_state_t)s,0);assert(stc_ui_check_layout(ui));snprintf(path,sizeof(path),"%s/%s.bmp",argv[2],stc_ui_state_name((stc_state_t)s));assert(screenshot(path));}
        for(int c=1;c<4;c++){show(STC_RESULT_OK,c);assert(stc_ui_check_layout(ui));snprintf(path,sizeof(path),"%s/result-cat-%d.bmp",argv[2],c);assert(screenshot(path));show(STC_LEARNING,c);assert(stc_ui_check_layout(ui));snprintf(path,sizeof(path),"%s/learning-cat-%d.bmp",argv[2],c);assert(screenshot(path));}
        show(STC_CLASSIFYING,0);stc_payload_t max=payload(STC_RESULT_OK);max.score=UINT32_MAX;max.streak=UINT32_MAX;assert(stc_ui_apply(ui,&max));assert(stc_ui_check_layout(ui));snprintf(path,sizeof(path),"%s/result-max.bmp",argv[2]);assert(screenshot(path));stc_ui_show_summary(ui);snprintf(path,sizeof(path),"%s/summary-max.bmp",argv[2]);assert(screenshot(path));
        stc_ui_destroy(ui);lv_deinit();return 0;}
    if(argc>2&&strcmp(argv[1],"--screenshot")==0){assert(screenshot(argv[2]));stc_ui_destroy(ui);lv_deinit();return 0;}
    /* SDL driver consumes its events; an event watch forwards demo shortcuts. */
    printf("DEMO 800x480 RGB565 rotation=0; no camera, AI, servo or network is driven.\n");fflush(stdout);
    SDL_AddEventWatch(event_watch,NULL);
    while(lv_display_get_next(NULL)){
        if(requested>=0){demo_stage=0;show((stc_state_t)requested,0);requested=-1;}
        if(demo_stage && SDL_GetTicks()>=due){
            stc_payload_t p=payload(demo_stage==1?STC_CLASSIFYING:STC_RESULT_OK);p.category_id=(int)(demo_category%4);stc_ui_apply(ui,&p);
            if(demo_stage==1){demo_stage=2;due=SDL_GetTicks()+900;}else{demo_stage=0;demo_category++;}
        }
        if(toggle_motion){reduced=!reduced;stc_ui_set_reduced_motion(ui,reduced);toggle_motion=false;}
        if(retry_command){stc_ui_retry(ui);retry_command=false;}
        lv_timer_handler();SDL_Delay(5);
    }
    SDL_DelEventWatch(event_watch,NULL);
    lv_deinit();return 0;
}
