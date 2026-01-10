#ifndef COMMAND_HANDLER_FUNCTIONS_H
#define COMMAND_HANDLER_FUNCTIONS_H

/* Action handler: Origin/Undo all movements */
int handle_origin_action(const char *json, const char *json_path);

/* Action handler: Back/Undo last movement */
int handle_back_action(const char *json, const char *json_path);

/* Action handler: Test sequence */
int handle_test_action(const char *json_path, int curAz, int curEl);

/* Action handler: Calibrate to 0,90 */
int handle_calibrate_action(const char *json_path, int curAz, int curEl);

/* Action handler: Force 0-90 position */
int handle_force_action(const char *json_path, int curAz, int curEl);

/* Action handler: Simple movements (L, R, U, D) */
int handle_movement_action(char action, int step, const char *json, const char *json_path, int curAz, int curEl);

#endif // COMMAND_HANDLER_FUNCTIONS_H
