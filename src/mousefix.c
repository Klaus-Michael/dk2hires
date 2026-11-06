#include "dk2hires.h"

#define MOUSE_SENSITIVITY 0x00759c21u
#define FRONTEND_COMPONENT 0x0070d6d8u
#define INPUT_MANAGER_CALLBACK 0x0079cffcu
#define MOUSE_MOTION 0x005dd4a0u
#define GET_MOUSE_DELTA 0x005589f0u
#define OLD_MINIMUM 5

/* The sliders display a raw value of 20 as 100%. Values below the old
 * minimum add a reduction while keeping existing sensitivity settings. */
static int mouse_speed(void)
{
    int speed = *(volatile const UNALIGNED int *)MOUSE_SENSITIVITY;
    return speed >= 1 && speed < OLD_MINIMUM ? speed : OLD_MINIMUM;
}

/* The unused EDX argument lets the C fastcall bridge match the game's
 * thiscall ABI, which passes the object in ECX and the event on the stack. */
typedef void (__fastcall *mouse_motion_fn)(void *mouse, void *unused, void *event);

static void __fastcall menu_mouse_motion(BYTE *mouse, void *unused, void *event)
{
    const BYTE *callback = *(const BYTE *volatile *)INPUT_MANAGER_CALLBACK;
    int speed = mouse_speed();
    mouse_motion_fn motion = (mouse_motion_fn)MOUSE_MOTION;
    float *gain = (float *)(mouse + 0x30);

    /* The native callback context points at the active front-end component.
     * Scale before the engine accumulates motion and computes hit positions. */
    if (speed < OLD_MINIMUM && callback &&
        *(const DWORD *)(callback + 0x10) == FRONTEND_COMPONENT) {
        float original = *gain;
        *gain = original * speed / OLD_MINIMUM;
        motion(mouse, unused, event);
        *gain = original;
    } else {
        motion(mouse, unused, event);
    }
}

static POINT *__cdecl possession_mouse_delta(void)
{
    static POINT result, remainder;
    static int previous_speed;
    POINT *delta = ((POINT *(__cdecl *)(void))GET_MOUSE_DELTA)();
    int speed = mouse_speed();
    LONGLONG x, y;

    if (speed != previous_speed) {
        remainder.x = remainder.y = 0;
        previous_speed = speed;
    }
    if (speed == OLD_MINIMUM) return delta;

    x = (LONGLONG)delta->x * speed + remainder.x;
    y = (LONGLONG)delta->y * speed + remainder.y;
    result.x = (LONG)(x / OLD_MINIMUM);
    result.y = (LONG)(y / OLD_MINIMUM);
    remainder.x = (LONG)(x % OLD_MINIMUM);
    remainder.y = (LONG)(y % OLD_MINIMUM);
    return &result;
}

void mousefix_install(void)
{
    static const DWORD menu_slider = 0x006b6d3e, game_slider = 0x00695cfe;
    static const DWORD mouse_slot = 0x006728c4, updater_slot = 0x006724bc;
    static const DWORD possession_site = 0x0040e93d;
    static const DWORD range[] = {OLD_MINIMUM, 20};
    static const DWORD original_motion = MOUSE_MOTION, minimum = 1;
    static const BYTE original_call[] = {0xe8, 0xae, 0xa0, 0x14, 0x00};
    DWORD hook = (DWORD)menu_mouse_motion;
    DWORD offset = (DWORD)possession_mouse_delta - (possession_site + 5);
    BYTE call[] = {0xe8, (BYTE)offset, (BYTE)(offset >> 8),
                   (BYTE)(offset >> 16), (BYTE)(offset >> 24)};

    if (!bytes_match(menu_slider, (const BYTE *)range, sizeof(range)) ||
        !bytes_match(game_slider, (const BYTE *)range, sizeof(range)) ||
        !bytes_match(mouse_slot, (const BYTE *)&original_motion, sizeof(DWORD)) ||
        !bytes_match(updater_slot, (const BYTE *)&original_motion, sizeof(DWORD)) ||
        !bytes_match(possession_site, original_call, sizeof(original_call))) {
        dk_log("mousefix: game bytes did not match");
        return;
    }
    if (write_code(mouse_slot, (const BYTE *)&hook, sizeof(hook)) &&
        write_code(updater_slot, (const BYTE *)&hook, sizeof(hook)) &&
        write_code(possession_site, call, sizeof(call)) &&
        write_code(menu_slider, (const BYTE *)&minimum, sizeof(minimum)) &&
        write_code(game_slider, (const BYTE *)&minimum, sizeof(minimum))) {
        dk_log("mousefix: 5%% minimum for front-end and possession");
        return;
    }
    write_code(game_slider, (const BYTE *)range, sizeof(minimum));
    write_code(menu_slider, (const BYTE *)range, sizeof(minimum));
    write_code(possession_site, original_call, sizeof(original_call));
    write_code(updater_slot, (const BYTE *)&original_motion, sizeof(DWORD));
    write_code(mouse_slot, (const BYTE *)&original_motion, sizeof(DWORD));
    dk_log("mousefix: could not install patches");
}
