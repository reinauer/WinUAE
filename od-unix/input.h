#ifndef WINUAE_OD_UNIX_INPUT_H
#define WINUAE_OD_UNIX_INPUT_H

void unix_input_mouse_motion(int dx, int dy);
void unix_input_mouse_button(int button, bool pressed);
void unix_input_mouse_wheel(int x, int y);
void unix_input_set_mouse_active(bool active);
bool unix_input_get_mouse_active(void);

#endif /* WINUAE_OD_UNIX_INPUT_H */
