#ifndef KEYS_H
#define KEYS_H

#include <xcb/xcb.h>

// NOTE: all keycode was assume US-QWERTY layout

#define KEY_ALT XCB_MOD_MASK_1
#define KEY_SUPER XCB_MOD_MASK_4
#define KEY_SHIFT XCB_MOD_MASK_SHIFT
#define KEY_CTRL XCB_MOD_MASK_CONTROL

#define KEY_ENTER 36
#define KEY_ESCAPE 9
#define KEY_SPACE 65
#define KEY_BACKSPACE 22

#define KEY_UP 111
#define KEY_DOWN 116

#define KEY_MINUS 20
#define KEY_EQUAL 21

#define KEY_A 38
#define KEY_B 56
#define KEY_C 54
#define KEY_D 40
#define KEY_E 26
#define KEY_F 41
#define KEY_G 42
#define KEY_H 43
#define KEY_I 31
#define KEY_J 44
#define KEY_K 45
#define KEY_L 46
#define KEY_M 58
#define KEY_N 57
#define KEY_O 32
#define KEY_P 33
#define KEY_Q 24
#define KEY_R 27
#define KEY_S 39
#define KEY_T 28
#define KEY_U 30
#define KEY_V 55
#define KEY_W 25
#define KEY_X 53
#define KEY_Y 29
#define KEY_Z 52

#define KEY_1 10
#define KEY_2 11
#define KEY_3 12
#define KEY_4 13
#define KEY_5 14
#define KEY_6 15
#define KEY_7 16
#define KEY_8 17
#define KEY_9 18
#define KEY_0 19

#define KEY_F1 67
#define KEY_F2 68
#define KEY_F3 69
#define KEY_F4 70
#define KEY_F5 71
#define KEY_F6 72
#define KEY_F7 73
#define KEY_F8 74
#define KEY_F9 75
#define KEY_F10 76
#define KEY_F11 95
#define KEY_F12 96

#endif // KEYS_H
