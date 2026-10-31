#include "stc_ui.h"
#include <SDL.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static stc_ui_t *ui;
static unsigned retries;
static bool reduced, toggle_motion, retry_command;
static unsigned demo_stage, demo_category;static uint32_t due;
static int requested=-1;
static int event_watch(void *user, SDL_Event *e){
    (void)user;
    if(e->type==SDL_KEYDOWN && !e->key.repeat){
        SDL_Keycode key=e->key.keysym.sym;
        if(key>=SDLK_1 && key<=SDLK_9) requested=(int)(key-SDLK_1);
        else if(key==SDLK_m) toggle_motion=true;
        else if(key==SDLK_r) retry_command=true;
    }return 1;
}
static void retry(void *unused){(void)unused;retries++;}
static void sdl_hal_init(int width,int height){
    lv_init();lv_display_t *d=lv_sdl_window_create(width,height);assert(d);
    lv_sdl_window_set_title(d,"Smart Trash Can | DEMO | 1-9 states, M motion, R retry");
    lv_indev_t *mouse=lv_sdl_mouse_create();lv_indev_set_display(mouse,d);
    lv_indev_t *keyboard=lv_sdl_keyboard_create();lv_group_t *group=lv_group_create();
    lv_group_set_default(group);lv_indev_set_group(keyboard,group);
    lv_obj_t *screen=lv_screen_active();lv_obj_remove_style_all(screen);
    ui=stc_ui_create(screen,retry,NULL);assert(ui);
}
static stc_payload_t payload(stc_state_t state){
    stc_payload_t p={.state=state,.category_id=0,.confidence=.93f,.score=120,.streak=4,
        .candidates={{0,.52f},{3,.43f}},.error_code="CAMERA_OFFLINE"};return p;
}
static void show(stc_state_t state,int category){
    if(stc_ui_state(ui)==STC_ERROR)stc_ui_retry(ui);
    stc_payload_t p=payload(STC_IDLE);
    if(stc_ui_state(ui)==STC_CAPTURING){p.state=STC_CLASSIFYING;assert(stc_ui_apply(ui,&p));}
    if(stc_ui_state(ui)==STC_CLASSIFYING){p.state=STC_RESULT_RETRY;assert(stc_ui_apply(ui,&p));}
    p.state=STC_IDLE;assert(stc_ui_apply(ui,&p));
    if(state==STC_ERROR){p.state=state;assert(stc_ui_apply(ui,&p));return;}
    if(state==STC_IDLE)return;
    p.state=STC_CAPTURING;assert(stc_ui_apply(ui,&p));if(state==STC_CAPTURING)return;
    p.state=STC_CLASSIFYING;assert(stc_ui_apply(ui,&p));if(state==STC_CLASSIFYING)return;
    p.state=state>=STC_CATEGORY_GUIDE?STC_RESULT_OK:state;p.category_id=category;assert(stc_ui_apply(ui,&p));
    if(state>=STC_CATEGORY_GUIDE){stc_ui_show_summary(ui);if(state==STC_SUMMARY)return;stc_ui_show_guide(ui);if(state==STC_LEARNING)stc_ui_show_learning(ui,category);}
}
static void demo_action(stc_action_t action,int category,void *user){
    (void)category;(void)user;
    if(action==STC_ACTION_START){show(STC_CAPTURING,0);demo_stage=1;due=SDL_GetTicks()+1200;}
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
static void press_button(const char *text){lv_obj_update_layout(lv_screen_active());lv_obj_t *button=button_named(lv_screen_active(),text);assert(button);lv_obj_send_event(button,LV_EVENT_CLICKED,NULL);}
static int self_test(void){
    unsigned cases=0;stc_ui_set_reduced_motion(ui,true);
    for(int s=0;s<9;s++){show((stc_state_t)s,0);assert(stc_ui_state(ui)==(stc_state_t)s);assert(stc_ui_check_layout(ui));cases++;}
    for(int cat=0;cat<4;cat++){show(STC_RESULT_OK,cat);assert(stc_ui_check_layout(ui));show(STC_LEARNING,cat);assert(stc_ui_check_layout(ui));cases+=2;}
    for(int a=0;a<4;a++)for(int b=0;b<4;b++)if(a!=b){show(STC_CLASSIFYING,0);stc_payload_t p=payload(STC_RESULT_RETRY);p.candidates[0].category_id=a;p.candidates[1].category_id=b;assert(stc_ui_apply(ui,&p));assert(stc_ui_check_layout(ui));cases++;}
    show(STC_IDLE,0);stc_payload_t p=payload(STC_RESULT_OK);assert(!stc_ui_apply(ui,&p));assert(stc_ui_state(ui)==STC_ERROR);cases++;
    p.state=STC_IDLE;assert(!stc_ui_apply(ui,&p));stc_ui_retry(ui);assert(stc_ui_apply(ui,&p));cases++;
    show(STC_CLASSIFYING,0);p=payload(STC_RESULT_OK);p.confidence=NAN;assert(!stc_ui_apply(ui,&p));cases++;
    show(STC_CLASSIFYING,0);p=payload(STC_RESULT_OK);p.category_id=4;assert(!stc_ui_apply(ui,&p));cases++;
    show(STC_CLASSIFYING,0);p=payload(STC_RESULT_RETRY);p.candidates[1].category_id=0;assert(!stc_ui_apply(ui,&p));cases++;
    show(STC_IDLE,0);p=payload((stc_state_t)77);assert(!stc_ui_apply(ui,&p));cases++;
    for(int s=0;s<9;s++)assert(stc_ui_state_from_string(stc_ui_state_name((stc_state_t)s))==(stc_state_t)s);
    assert(stc_ui_state_from_string("stats")==STC_ERROR);assert(stc_ui_state_from_string(NULL)==STC_ERROR);cases++;
    static const uint16_t pixels[4]={0xf800,0x07e0,0x001f,0xffff};
    static const lv_image_dsc_t original={.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565,.w=2,.h=2,.stride=4},.data_size=sizeof(pixels),.data=(const uint8_t*)pixels};
    show(STC_CAPTURING,0);stc_ui_set_images(ui,&original,NULL);lv_refr_now(lv_display_get_default());stc_ui_set_images(ui,NULL,NULL);assert(stc_ui_check_layout(ui));cases++;
    show(STC_CLASSIFYING,0);stc_ui_set_reduced_motion(ui,false);assert(lv_anim_count_running()==1);stc_ui_set_reduced_motion(ui,true);assert(lv_anim_count_running()==0);cases++;
    show(STC_IDLE,0);assert(!button_named(lv_screen_active(),"分類圖鑑"));
    p=payload(STC_CATEGORY_GUIDE);assert(!stc_ui_apply(ui,&p));cases++;
    show(STC_SUMMARY,0);press_button("繼續學分類");assert(stc_ui_state(ui)==STC_CATEGORY_GUIDE);
    /* Guide cards use SVG/text overlays; dispatch the underlying card's click event. */
    lv_obj_update_layout(lv_screen_active());
    lv_obj_t *root=lv_obj_get_child(lv_obj_get_child(lv_screen_active(),0),0);
    lv_obj_t *card=NULL;for(uint32_t i=0;i<lv_obj_get_child_count(root);i++){lv_obj_t *child=lv_obj_get_child(root,i);if(lv_obj_check_type(child,&lv_button_class)&&lv_obj_get_x(child)==24&&lv_obj_get_y(child)==136){card=child;break;}}
    assert(card);lv_obj_send_event(card,LV_EVENT_CLICKED,NULL);assert(stc_ui_state(ui)==STC_LEARNING);assert(!button_named(lv_screen_active(),"我想試一試"));press_button("返回主頁");assert(stc_ui_state(ui)==STC_IDLE);cases++;
    show(STC_LEARNING,3);press_button("返回圖鑑");press_button("返回成績");assert(stc_ui_state(ui)==STC_SUMMARY);cases++;
    show(STC_LEARNING,1);p=payload(STC_CAPTURING);assert(!stc_ui_apply(ui,&p));cases++;
    show(STC_RESULT_OK,0);press_button("完成");assert(stc_ui_state(ui)==STC_SUMMARY);assert(stc_ui_check_layout(ui));cases++;
    show(STC_ERROR,0);unsigned before=retries;press_button("重試");press_button("重試");assert(retries==before+1);cases++;
    show(STC_CLASSIFYING,0);p=payload(STC_RESULT_OK);p.score=UINT32_MAX;p.streak=UINT32_MAX;assert(stc_ui_apply(ui,&p));assert(stc_ui_check_layout(ui));stc_ui_show_summary(ui);assert(stc_ui_check_layout(ui));cases++;
    show(STC_IDLE,0);lv_obj_t *missing=lv_label_create(lv_obj_get_child(lv_screen_active(),0));lv_label_set_text(missing,"🦄");
    assert(!stc_ui_check_layout(ui));lv_obj_delete(missing);assert(stc_ui_check_layout(ui));cases++;
    for(int i=0;i<150;i++){show(STC_RESULT_OK,i%4);show(STC_RESULT_RETRY,0);}cases++;
    lv_obj_t *parent=lv_obj_create(lv_screen_active());lv_obj_remove_style_all(parent);lv_obj_set_size(parent,800,480);
    stc_ui_t *temporary=stc_ui_create(parent,NULL,NULL);assert(temporary);lv_obj_delete(parent);p=payload(STC_IDLE);assert(!stc_ui_apply(temporary,&p));stc_ui_destroy(temporary);cases++;
    printf("PASS %u state/data/layout/motion/image/lifecycle scenarios; nine states and four categories\n",cases);return 0;
}
int main(int argc,char **argv){
    SDL_SetHint(SDL_HINT_RENDER_DRIVER,"software");
    sdl_hal_init(800, 480);
    stc_ui_set_action_callback(ui,demo_action);
  lv_display_set_rotation(lv_display_get_default(), LV_DISPLAY_ROTATION_0);
    if(argc>1&&strcmp(argv[1],"--self-test")==0){int result=self_test();stc_ui_destroy(ui);lv_deinit();return result;}
    if(argc>2&&strcmp(argv[1],"--screenshots")==0){char path[1024];stc_ui_set_reduced_motion(ui,true);
        for(int s=0;s<9;s++){show((stc_state_t)s,0);assert(stc_ui_check_layout(ui));snprintf(path,sizeof(path),"%s/%s.bmp",argv[2],stc_ui_state_name((stc_state_t)s));assert(screenshot(path));}
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
