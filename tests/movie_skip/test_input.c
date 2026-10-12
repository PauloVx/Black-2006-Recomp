#include "pc_movie_skip.h"
#include "pc_input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
static int prompt;
static uint64_t clock_ms;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
/* No game action-map bytes are needed: this fixture exercises movie ownership
 * and the input module's menu layer, never gameplay action resolution. */
static int no_gameplay_control(int slot) { (void)slot; return -1; }
void pc_menu_set_movie_skip_prompt(int visible) { prompt = visible; }
static void time_at(uint64_t now) { clock_ms = now; pc_movie_skip_test_time(now); pc_input_test_set_time(now); }
static void reset(void)
{
    _putenv_s("RECOMP_BLACK_INPUT_MODE", "auto");
    _putenv_s("RECOMP_INPUT_SCRIPT", "");
    _putenv_s("RECOMP_INPUT_SCRIPT_BACKGROUND", "");
    _putenv_s("RECOMP_PAD_SCRIPT", "");
    pc_input_test_reset(); pc_input_set_control_resolver(no_gameplay_control);
    pc_input_set_filter_slot(0,NULL); pc_input_set_filter_slot(1,NULL);
    pc_movie_skip_test_reset(); pc_movie_skip_install();
    pc_input_set_movie_active_query(pc_movie_skip_active);
    time_at(100000);
}
static void test_enter_maps_to_xbox_a(void)
{
    uint8_t out[20] = {0,20};
    reset();
    pc_input_key_event(0x1C,0,1,0); /* physical Enter scancode */
    pc_input_apply_report(out,0);  /* no controller and no scripted presses */
    CHECK(out[4] == 0xFF);

    reset();
    memset(out,0,sizeof(out)); out[1]=20;
    pc_input_key_event(0x1C,1,1,0); /* extended keypad Enter */
    pc_input_apply_report(out,0);
    CHECK(out[4] == 0xFF);
    reset();
}
static void key(unsigned code,int down,int repeat) { pc_input_key_event(code,0,down,repeat); }
static void tap(unsigned code) { key(code,1,0); key(code,0,0); }
static void report(unsigned start,unsigned a,unsigned b)
{
    uint8_t out[20] = {0,20}; unsigned i;
    out[2]=(uint8_t)start; out[4]=(uint8_t)a; out[5]=(uint8_t)b;
    pc_input_apply_report(out,1);
    if(pc_movie_skip_active()) for(i=2;i<20;++i) CHECK(out[i]==0);
}
static int lower_keys;
static int lower_key(unsigned id,int down,int repeat) { (void)id;(void)down;(void)repeat;++lower_keys;return 1; }
static const PcInputFilter lower = { lower_key, NULL, NULL, NULL, NULL };

/* The movie consumes the release of a mouse button already held in an original
 * guest menu. Neither that hold nor its minimum-duration tap may return later. */
static void test_filtered_mouse_release(void)
{
    unsigned button, source, i;
    for (source = 0; source < 2; ++source) for (button = 0; button < 2; ++button) {
        uint8_t out[20] = {0,20};
        reset();
        if (source) {
            char script[96];
            snprintf(script,sizeof(script),"0:down:Mouse%u,100:up:Mouse%u",button+1,button+1);
            _putenv_s("RECOMP_INPUT_SCRIPT",script);
            pc_input_test_reset(); pc_movie_skip_install();
            pc_input_set_control_resolver(no_gameplay_control);
            pc_input_set_movie_active_query(pc_movie_skip_active);
            time_at(100000);
        } else pc_input_mouse_button_event((int)button,1);
        pc_input_apply_report(out,0);
        CHECK(out[4+button]==255); /* menu confirm/back before movie ownership */
        pc_movie_skip_update(1,PC_MOVIE_SKIP_CUTSCENE,1);
        if (source) {
            time_at(clock_ms+100);
            memset(out+2,0,18); pc_input_apply_report(out,0); /* scripted physical release */
        } else pc_input_mouse_button_event((int)button,0);
        pc_input_frame_complete(); pc_movie_skip_clear();
        time_at(clock_ms+1000);
        memset(out+2,0,18); pc_input_apply_report(out,0);
        for(i=2;i<20;++i) CHECK(out[i]==0);
        pc_input_mouse_button_event((int)button,1);
        memset(out+2,0,18); pc_input_apply_report(out,0);
        CHECK(out[4+button]==255); /* a fresh press belongs to the next menu */
    }
    reset();
}

/* A controller can continue to produce reports after the window loses focus.
 * Neither unfocused presses nor the held button on focus return may skip. */
static void test_unfocused_pad(void)
{
    unsigned kind, button;
    for (kind = PC_MOVIE_SKIP_INTRO; kind <= PC_MOVIE_SKIP_CUTSCENE; ++kind) {
        for (button = 0; button < 3; ++button) {
            const unsigned start = button == 0 ? 0x10u : 0u;
            const unsigned a = button == 1 ? 255u : 0u;
            const unsigned b = button == 2 ? 255u : 0u;
            reset(); pc_movie_skip_update(1, (int)kind, 1);
            pc_input_focus_event(0);
            report(0,0,0); report(start,a,b); report(0,0,0); report(start,a,b);
            CHECK(!prompt); CHECK(!pc_movie_skip_take_request(1));
            pc_input_focus_event(1); report(start,a,b);
            CHECK(!prompt); CHECK(!pc_movie_skip_take_request(1));
            report(0,0,0); report(start,a,b);
            if (kind == PC_MOVIE_SKIP_INTRO) CHECK(pc_movie_skip_take_request(1));
            else {
                CHECK(prompt); CHECK(!pc_movie_skip_take_request(1));
                report(0,0,0); report(start,a,b);
                CHECK(pc_movie_skip_take_request(1));
            }
        }
    }
}

/* These IDs are a synthetic action-map contract, not retail table bytes. */
static int background_control(int slot)
{
    return slot == 1 ? 4 : slot == 9 ? 23 : slot == 16 ? 22 : slot == 14 ? 19 : -1;
}
static unsigned audited_keys, audited_buttons, audited_wheels, audited_focus;
static int audit_key(unsigned id,int down,int repeat)
{ (void)id;(void)down;(void)repeat;++audited_keys;return 0; }
static int audit_button(int button,int down)
{ (void)button;(void)down;++audited_buttons;return 0; }
static int audit_wheel(int delta) { (void)delta;++audited_wheels;return 0; }
static void audit_focus(int focused) { (void)focused;++audited_focus; }
static const PcInputFilter audit = {audit_key,audit_button,audit_wheel,NULL,audit_focus};
static void configure_background(const char *script,const char *flag)
{
    reset();
    _putenv_s("RECOMP_INPUT_SCRIPT",script ? script : "");
    _putenv_s("RECOMP_INPUT_SCRIPT_BACKGROUND",flag ? flag : "");
    pc_input_test_reset();
    pc_input_set_control_resolver(background_control);
    time_at(100000);
}
static void clear_report(uint8_t out[20]) { memset(out,0,20);out[1]=20; }
static void test_background_script_isolation(void)
{
    uint8_t out[20];
    double yaw,pitch;
    const char *script="0:down:W,0:down:Mouse1,0:move:10:20,100:up:W,100:up:Mouse1,"
        "150:tap:Enter,180:wheel:1,200:down:W,220:focus:1,240:focus:0,300:up:W";
    configure_background(script,"1");
    audited_keys=audited_buttons=audited_wheels=audited_focus=0;
    pc_input_set_filter_slot(0,&audit);
    pc_input_gameplay_tick();
    CHECK(pc_input_gameplay_active());
    CHECK(!pc_input_capture_wanted());
    clear_report(out);pc_input_apply_report(out,0);
    CHECK(out[14]==0xFF && out[15]==0x7F && out[11]==255);
    CHECK(audited_keys==1 && audited_buttons==1 && audited_wheels==0);

    pc_input_capture_changed(1); /* a physical notification must preserve script look */
    CHECK(!pc_input_cursor_hidden_wanted());
    pc_input_focus_event(0);
    CHECK(pc_input_gameplay_active());
    CHECK(!pc_input_capture_wanted() && !pc_input_cursor_hidden_wanted());
    CHECK(audited_focus==0);
    pc_input_key_event(0x1C,0,1,0);pc_input_key_event(0x11,0,0,0);
    pc_input_mouse_button_event(0,0);pc_input_mouse_button_event(1,1);
    pc_input_wheel_event(1);pc_input_mouse_motion(1000,1000);
    CHECK(audited_keys==1 && audited_buttons==1 && audited_wheels==0);
    CHECK(pc_input_consume_look(&yaw,&pitch));
    CHECK(yaw < -0.599999 && yaw > -0.600001);
    CHECK(pitch > 1.199999 && pitch < 1.200001);
    CHECK(!pc_input_consume_look(&yaw,&pitch));
    pc_input_frame_complete();pc_input_frame_complete(); /* asynchronous camera gap */
    clear_report(out);pc_input_apply_report(out,0);
    CHECK(out[14]==0xFF && out[15]==0x7F && out[11]==255);
    CHECK(out[4]==0 && out[10]==0 && !(out[2]&0x10));
    pc_input_capture_changed(0);
    pc_input_focus_event(1); /* focus return must not discard the same script holds */
    CHECK(audited_focus==0 && !pc_input_capture_wanted());
    pc_input_key_event(0x1C,0,1,0);pc_input_mouse_button_event(1,1);
    pc_input_wheel_event(-1);pc_input_mouse_motion(1000,1000);
    CHECK(audited_keys==1 && audited_buttons==1 && audited_wheels==0);
    CHECK(!pc_input_consume_look(&yaw,&pitch));
    pc_input_focus_event(0);pc_input_gameplay_tick();
    time_at(100100);clear_report(out);pc_input_apply_report(out,0);
    CHECK(out[14]==0 && out[15]==0 && out[11]==0);
    time_at(100150);clear_report(out);pc_input_apply_report(out,0);
    CHECK(out[4]==255); /* virtual confirmation still passes through the filters */
    time_at(100180);clear_report(out);pc_input_apply_report(out,0);
    CHECK(out[7]==255 && audited_wheels==1);
    time_at(100200);clear_report(out);pc_input_apply_report(out,0);
    CHECK(out[14]==0xFF && out[15]==0x7F);
    time_at(100220);clear_report(out);pc_input_apply_report(out,0);
    time_at(100240);clear_report(out);pc_input_apply_report(out,0);
    CHECK(out[14]==0xFF && out[15]==0x7F && !(out[2]&0x10));
    CHECK(audited_focus==0 && !pc_input_capture_wanted());
    time_at(100300);clear_report(out);pc_input_apply_report(out,0);
    CHECK(out[14]==0 && out[15]==0);
    reset();
}
static void test_background_setting_fail_closed(void)
{
    uint8_t out[20];
    static const struct {const char *script,*flag;} cases[]={
        {"0:down:W",NULL},{"0:down:W","0"},{"0:down:W","true"},
        {"0:down:W","01"},{"0:down:W","1junk"},{"0:down:W","1 "},
        {NULL,"1"},{"","1"},{"not-a-script","1"},{"0:unknown:W","1"},
        {"0:down:NoSuchKey","1"},{"0:down:W,10:unknown:W","1"},
        {":down:W","1"},{"-1:down:W","1"},{"10:down:W,0:up:W","1"},
        {"0:move:bad:2","1"},{"0:move:1:bad","1"},{"0:wheel:bad","1"},
        {"0:focus:2","1"},{"0:down:W,","1"},
        {"18446744073709551616:down:W","1"},
        {"0:move:9999999999999999999999999999:2","1"}
    };
    unsigned i;
    for(i=0;i<sizeof(cases)/sizeof(cases[0]);++i) {
        configure_background(cases[i].script,cases[i].flag);
        audited_focus=0;pc_input_set_filter_slot(0,&audit);
        pc_input_gameplay_tick();
        CHECK(pc_input_capture_wanted());
        clear_report(out);pc_input_apply_report(out,0);
        pc_input_focus_event(0);
        CHECK(audited_focus==1);
        pc_input_focus_event(0);CHECK(audited_focus==1);
        CHECK(!pc_input_gameplay_active() && !pc_input_capture_wanted());
        clear_report(out);pc_input_apply_report(out,0);
        CHECK((out[2]&0x10)!=0 && out[11]==0 && out[14]==0 && out[15]==0);
    }
    /* Reset must remove the prior lab exception and restore physical input. */
    configure_background("0:down:W","1");
    reset();tap(0x1C);clear_report(out);pc_input_apply_report(out,0);
    CHECK(out[4]==255);
    reset();
}
static void test_background_movie_filter(void)
{
    uint8_t out[20];
    configure_background("0:tap:Enter,100:tap:Enter","1");
    pc_movie_skip_update(1,PC_MOVIE_SKIP_CUTSCENE,1);
    pc_input_focus_event(0);
    clear_report(out);pc_input_apply_report(out,0);
    CHECK(prompt && !pc_movie_skip_take_request(1));
    pc_input_key_event(0x1C,0,1,0);pc_input_key_event(0x1C,0,0,0);
    CHECK(prompt && !pc_movie_skip_take_request(1));
    time_at(100100);clear_report(out);pc_input_apply_report(out,0);
    CHECK(pc_movie_skip_take_request(1));
    reset();
}

int main(void)
{
    unsigned eligible[] = {0x01,0x1c,0x39}; unsigned i;
    for(i=0;i<3;++i) {
        reset(); pc_movie_skip_update(1,PC_MOVIE_SKIP_INTRO,1);
        tap(eligible[i]); CHECK(!prompt); CHECK(pc_movie_skip_take_request(1)); CHECK(!pc_movie_skip_take_request(1));
        reset(); pc_movie_skip_update(1,PC_MOVIE_SKIP_CUTSCENE,1);
        key(eligible[i],1,0); CHECK(prompt); CHECK(!pc_movie_skip_take_request(1));
        key(eligible[i],1,1); key(eligible[i],1,0); CHECK(prompt); CHECK(!pc_movie_skip_take_request(1));
        key(eligible[i],0,0); tap(eligible[i]); CHECK(!prompt); CHECK(pc_movie_skip_take_request(1));
    }
    reset(); pc_movie_skip_update(1,PC_MOVIE_SKIP_CUTSCENE,1); tap(0x1c); CHECK(prompt);
    time_at(clock_ms+3001); pc_movie_skip_update(1,PC_MOVIE_SKIP_CUTSCENE,1);
    CHECK(!prompt); tap(0x1c); CHECK(prompt); CHECK(!pc_movie_skip_take_request(1));
    /* Refresh the movie while the confirmation itself expires. */
    reset(); pc_movie_skip_update(1,PC_MOVIE_SKIP_CUTSCENE,1); tap(0x01);
    for(i=0;i<30;++i) { time_at(clock_ms+100); pc_movie_skip_update(1,PC_MOVIE_SKIP_CUTSCENE,1); }
    CHECK(!prompt); tap(0x01); CHECK(prompt); CHECK(!pc_movie_skip_take_request(1));
    reset(); pc_movie_skip_update(1,PC_MOVIE_SKIP_CUTSCENE,1); tap(0x01); CHECK(prompt);
    pc_movie_skip_update(2,PC_MOVIE_SKIP_CUTSCENE,1); CHECK(!prompt); tap(0x01); CHECK(prompt); CHECK(!pc_movie_skip_take_request(2));
    pc_movie_skip_update(2,PC_MOVIE_SKIP_CUTSCENE,0); CHECK(!prompt); CHECK(!pc_movie_skip_active());
    reset(); pc_movie_skip_update(1,PC_MOVIE_SKIP_INTRO,1); key(0x01,1,0); CHECK(pc_movie_skip_take_request(1));
    pc_movie_skip_update(2,PC_MOVIE_SKIP_INTRO,1); key(0x01,1,1); CHECK(!pc_movie_skip_take_request(2));
    key(0x01,0,0); tap(0x01); CHECK(pc_movie_skip_take_request(2));
    reset(); key(0x01,1,0); pc_movie_skip_update(1,PC_MOVIE_SKIP_INTRO,1);
    key(0x01,1,1); CHECK(!pc_movie_skip_take_request(1)); key(0x01,0,0); tap(0x01); CHECK(pc_movie_skip_take_request(1));
    reset(); pc_movie_skip_update(1,PC_MOVIE_SKIP_CUTSCENE,1); key(0x01,1,0); key(0x1c,1,0);
    CHECK(!pc_movie_skip_take_request(1)); key(0x01,0,0); key(0x1c,0,0); tap(0x1c); CHECK(pc_movie_skip_take_request(1));
    reset(); pc_movie_skip_update(1,PC_MOVIE_SKIP_CUTSCENE,1); tap(0x01);
    pc_input_focus_event(0); CHECK(!prompt); pc_input_focus_event(1);
    key(0x01,1,1); CHECK(!pc_movie_skip_take_request(1)); key(0x01,0,0); tap(0x01); CHECK(prompt); CHECK(!pc_movie_skip_take_request(1));
    reset(); pc_movie_skip_update(1,PC_MOVIE_SKIP_CUTSCENE,1); tap(0x01);
    time_at(clock_ms+501); CHECK(!pc_movie_skip_active()); CHECK(!prompt); CHECK(!pc_movie_skip_take_request(1));
    reset(); pc_movie_skip_update(1,PC_MOVIE_SKIP_NONE,1); tap(0x01); CHECK(!prompt); CHECK(!pc_movie_skip_active());
    reset(); pc_movie_skip_update(1,PC_MOVIE_SKIP_CUTSCENE,1); tap(0x13); CHECK(!prompt); CHECK(!pc_movie_skip_take_request(1));
    for(i=0;i<3;++i) {
        unsigned start=i==0?0x10:0,a=i==1?255:0,b=i==2?255:0;
        reset(); report(0,0,0); pc_movie_skip_update(1,PC_MOVIE_SKIP_CUTSCENE,1);
        report(start,a,b); CHECK(prompt); CHECK(!pc_movie_skip_take_request(1));
        report(start,a,b); CHECK(!pc_movie_skip_take_request(1));
        report(0,0,0); report(start,a,b); CHECK(pc_movie_skip_take_request(1));
        /* Held confirming input stays neutral on the next gameplay report. */
        { uint8_t out[20]={0,20}; out[2]=(uint8_t)start;out[4]=(uint8_t)a;out[5]=(uint8_t)b; pc_input_apply_report(out,1); CHECK(out[2]==0&&out[4]==0&&out[5]==0); }
        report(0,0,0); pc_movie_skip_update(2,PC_MOVIE_SKIP_INTRO,1); report(start,a,b); CHECK(pc_movie_skip_take_request(2));
    }
    reset(); report(0x10,0,0); pc_movie_skip_update(1,PC_MOVIE_SKIP_INTRO,1);
    report(0x10,0,0); CHECK(!pc_movie_skip_take_request(1)); report(0,0,0); report(0x10,0,0); CHECK(pc_movie_skip_take_request(1));
    reset(); pc_movie_skip_update(1,PC_MOVIE_SKIP_CUTSCENE,1);
    pc_input_mouse_button_event(0,1); CHECK(prompt); pc_input_mouse_button_event(0,1); CHECK(!pc_movie_skip_take_request(1));
    pc_input_mouse_button_event(0,0); pc_input_mouse_button_event(0,1); CHECK(pc_movie_skip_take_request(1));
    reset(); lower_keys=0; pc_input_set_filter_slot(0,&lower); pc_input_set_filter_slot(1,&lower);
    pc_movie_skip_update(1,PC_MOVIE_SKIP_CUTSCENE,1); tap(0x01); CHECK(prompt); CHECK(lower_keys==0);
    tap(0x57); CHECK(lower_keys==2); pc_movie_skip_clear(); tap(0x13); CHECK(lower_keys==4);
    reset(); pc_movie_skip_update(1,PC_MOVIE_SKIP_CUTSCENE,1); tap(0x01); tap(0x01);
    CHECK(!pc_movie_skip_take_request(2)); CHECK(pc_movie_skip_take_request(1));
    test_enter_maps_to_xbox_a();
    test_filtered_mouse_release();
    test_unfocused_pad();
    test_background_script_isolation();
    test_background_setting_fail_closed();
    test_background_movie_filter();
    printf("PASS: %u movie skip checks\n",checks); return 0;
}
