#!/bin/sh
# Overlay build, extracted verbatim from the Makefile recipe.
#
# A recipe reaches the shell as one command line, and Windows truncates that
# at 8191 characters with no diagnostic. The Spectranext overlay recipe
# expands to about 36,000 characters, so it was cut mid-argument and the
# tool that received the fragment failed on a half-written path
# ('file not found: build/spectranext/overlay_'). A script is read from a
# file and has no such limit.
#
# Each original recipe line is one subshell here, so a failure stops the
# build exactly where make would have stopped it.
# Inputs arrive as environment variables; see the caller in the Makefile.

# --- recipe block 1 ---
(
SLOT=$(grep '_overlay_code_slot ' ${BUILD_DIR}/${ZX_NAME}.map | sed -n 's/.*= \$\([0-9A-Fa-f]*\).*/\1/p' | head -1); \
if [ -z "$SLOT" ]; then \
	printf "[ERR] _overlay_code_slot not found in ${BUILD_DIR}/${ZX_NAME}.map\n"; \
	exit 1; \
fi; \
echo "  overlay_code_slot = 0x$SLOT"; \
${PYTHON} tools/gen_overlay_defs.py ${BUILD_DIR}/${ZX_NAME}.map > ${OVL_DEFS} || exit 1; \
echo "  overlay_defs.asm generated"; \
rm -f ${BUILD_DIR}/overlay_defs.o ${BUILD_DIR}/overlay_defs.o~; \
${ZX_Z80ASM} asm/overlay/rules/entry_rules.asm 2>&1 || exit 1; \
${ZX_Z80ASM} asm/overlay/rules/rules_stub.asm 2>&1 || exit 1; \
${ZX_Z80ASM} -b -r0x$SLOT -o=${BUILD_DIR}/${ZX_NAME}_RULES.OVL \
	asm/overlay/rules/entry_rules.o asm/overlay/rules/rules_stub.o ${OVL_DEFS} 2>&1 || exit 1; \
${ZX_Z80ASM} asm/overlay/board/entry_board.asm 2>&1 || exit 1; \
rm -f ${BUILD_DIR}/overlay_defs.o ${BUILD_DIR}/overlay_defs.o~; \
${ZX_Z80ASM} -b -r0x$SLOT -o=${BUILD_DIR}/${ZX_NAME}_BOARD.OVL \
	asm/overlay/board/entry_board.o ${OVL_DEFS} 2>&1 || exit 1; \
${ZX_Z80ASM} asm/overlay/gui_log/entry_gui_log.asm 2>&1 || exit 1; \
(cd ${BUILD_DIR} && ${ZCC} +z80 ${ZX_OVL_CFLAGS} -c ${BUILD_DIR_UP}src/spectrum/overlay/gui_log_ovl.c -o gui_log_ovl.o) 2>&1 || exit 1; \
rm -f ${BUILD_DIR}/overlay_defs.o ${BUILD_DIR}/overlay_defs.o~; \
${ZX_Z80ASM} -b -r0x$SLOT -o=${BUILD_DIR}/${ZX_NAME}_GUI_LOG.OVL \
	asm/overlay/gui_log/entry_gui_log.o ${BUILD_DIR}/gui_log_ovl.o ${OVL_DEFS} 2>&1 || exit 1; \
${ZX_Z80ASM} asm/overlay/input_edit/entry_input_edit.asm 2>&1 || exit 1; \
(cd ${BUILD_DIR} && ${ZCC} +z80 ${ZX_OVL_CFLAGS} -c ${BUILD_DIR_UP}src/spectrum/overlay/input_edit_ovl.c -o input_edit_ovl.o) 2>&1 || exit 1; \
rm -f ${BUILD_DIR}/overlay_defs.o ${BUILD_DIR}/overlay_defs.o~; \
${ZX_Z80ASM} -b -r0x$SLOT -o=${BUILD_DIR}/${ZX_NAME}_INPUT_EDIT.OVL \
	asm/overlay/input_edit/entry_input_edit.o ${BUILD_DIR}/input_edit_ovl.o ${OVL_DEFS} 2>&1 || exit 1; \
${ZX_Z80ASM} asm/overlay/mqtt_connect/entry_mqtt_connect.asm 2>&1 || exit 1; \
(cd ${BUILD_DIR} && ${ZCC} +z80 ${ZX_OVL_CFLAGS} ${MQTT_CONNECT_OVL_CFLAGS} -c ${BUILD_DIR_UP}src/spectrum/overlay/mqtt_connect_ovl.c -o mqtt_connect_ovl.o) 2>&1 || exit 1; \
rm -f ${BUILD_DIR}/overlay_defs.o ${BUILD_DIR}/overlay_defs.o~; \
${ZX_Z80ASM} -b -r0x$SLOT -o=${BUILD_DIR}/${ZX_NAME}_MQTT_CONNECT.OVL \
	asm/overlay/mqtt_connect/entry_mqtt_connect.o ${BUILD_DIR}/mqtt_connect_ovl.o ${OVL_DEFS} 2>&1 || exit 1; \
${ZX_Z80ASM} asm/overlay/mqtt_tx/entry_mqtt_tx.asm 2>&1 || exit 1; \
(cd ${BUILD_DIR} && ${ZCC} +z80 ${ZX_OVL_CFLAGS} -c ${BUILD_DIR_UP}src/spectrum/overlay/mqtt_tx_ovl.c -o mqtt_tx_ovl.o) 2>&1 || exit 1; \
rm -f ${BUILD_DIR}/overlay_defs.o ${BUILD_DIR}/overlay_defs.o~; \
${ZX_Z80ASM} -b -r0x$SLOT -o=${BUILD_DIR}/${ZX_NAME}_MQTT_TX.OVL \
	asm/overlay/mqtt_tx/entry_mqtt_tx.o ${BUILD_DIR}/mqtt_tx_ovl.o ${OVL_DEFS} 2>&1 || exit 1; \
${ZX_Z80ASM} asm/overlay/direct/entry_direct.asm 2>&1 || exit 1; \
(cd ${BUILD_DIR} && ${ZCC} +z80 ${ZX_OVL_CFLAGS} -c ${BUILD_DIR_UP}src/spectrum/overlay/direct_ovl.c -o direct_ovl.o) 2>&1 || exit 1; \
rm -f ${BUILD_DIR}/overlay_defs.o ${BUILD_DIR}/overlay_defs.o~; \
${ZX_Z80ASM} -b -r0x$SLOT -o=${BUILD_DIR}/${ZX_NAME}_DIRECT.OVL \
	asm/overlay/direct/entry_direct.o ${BUILD_DIR}/direct_ovl.o ${OVL_DEFS} 2>&1 || exit 1
) || exit 1

# --- recipe block 2 ---
(
SLOT=$(grep '_overlay_code_slot ' ${BUILD_DIR}/${ZX_NAME}.map | sed -n 's/.*= \$\([0-9A-Fa-f]*\).*/\1/p' | head -1); \
ovl_size=$(wc -c < ${BUILD_DIR}/${ZX_NAME}_RULES.OVL); \
hints_size=0; \
board_size=$(wc -c < ${BUILD_DIR}/${ZX_NAME}_BOARD.OVL); \
gui_log_size=$(wc -c < ${BUILD_DIR}/${ZX_NAME}_GUI_LOG.OVL); \
input_edit_size=$(wc -c < ${BUILD_DIR}/${ZX_NAME}_INPUT_EDIT.OVL); \
mqtt_connect_size=$(wc -c < ${BUILD_DIR}/${ZX_NAME}_MQTT_CONNECT.OVL); \
mqtt_tx_size=$(wc -c < ${BUILD_DIR}/${ZX_NAME}_MQTT_TX.OVL); \
direct_size=$(wc -c < ${BUILD_DIR}/${ZX_NAME}_DIRECT.OVL); \
${ZX_Z80ASM} asm/overlay/menu_config/entry_menu_config.asm 2>&1 || exit 1; \
rm -f ${BUILD_DIR}/overlay_defs.o ${BUILD_DIR}/overlay_defs.o~; \
${ZX_Z80ASM} -b -r0x$SLOT -o=${BUILD_DIR}/${ZX_NAME}_MENU_CONFIG.OVL \
	asm/overlay/menu_config/entry_menu_config.o ${OVL_DEFS} 2>&1 || exit 1; \
menu_config_size=$(wc -c < ${BUILD_DIR}/${ZX_NAME}_MENU_CONFIG.OVL); \
${ZX_Z80ASM} asm/overlay/menu_logic/entry_menu_logic.asm 2>&1 || exit 1; \
(cd ${BUILD_DIR} && ${ZCC} +z80 ${ZX_OVL_CFLAGS} -c ${BUILD_DIR_UP}src/spectrum/overlay/status_ovl.c -o status_ovl.o) 2>&1 || exit 1; \
rm -f ${BUILD_DIR}/overlay_defs.o ${BUILD_DIR}/overlay_defs.o~; \
${ZX_Z80ASM} -b -r0x$SLOT -o=${BUILD_DIR}/${ZX_NAME}_MENU_LOGIC.OVL \
	asm/overlay/menu_logic/entry_menu_logic.o ${BUILD_DIR}/status_ovl.o ${OVL_DEFS} 2>&1 || exit 1; \
menu_logic_size=$(wc -c < ${BUILD_DIR}/${ZX_NAME}_MENU_LOGIC.OVL); \
${ZX_Z80ASM} asm/overlay/fileui/entry_fileui.asm 2>&1 || exit 1; \
if [ -n "${ESX_FILEUI_ASM}" ]; then ${ZX_Z80ASM} ${ESX_FILEUI_ASM} 2>&1 || exit 1; fi; \
(cd ${BUILD_DIR} && ${ZCC} +z80 ${ZX_OVL_CFLAGS} -c ${BUILD_DIR_UP}src/spectrum/overlay/fileui_ovl.c -o fileui_ovl.o) 2>&1 || exit 1; \
rm -f ${BUILD_DIR}/overlay_defs.o ${BUILD_DIR}/overlay_defs.o~; \
${ZX_Z80ASM} -b -r0x$SLOT -o=${BUILD_DIR}/${ZX_NAME}_FILEUI.OVL \
	asm/overlay/fileui/entry_fileui.o ${BUILD_DIR}/fileui_ovl.o ${ESX_FILEUI_OBJ} ${OVL_DEFS} 2>&1 || exit 1; \
fileui_size=$(wc -c < ${BUILD_DIR}/${ZX_NAME}_FILEUI.OVL); \
${ZX_Z80ASM} asm/overlay/setup/entry_setup.asm 2>&1 || exit 1; \
${ZX_Z80ASM} ${EDIT_BUF_OVL} 2>&1 || exit 1; \
rm -f ${BUILD_DIR}/overlay_defs.o ${BUILD_DIR}/overlay_defs.o~; \
${ZX_Z80ASM} -b -r0x$SLOT -o=${BUILD_DIR}/${ZX_NAME}_SETUP.OVL \
	asm/overlay/setup/entry_setup.o asm/overlay/edit/edit_buf.o ${OVL_DEFS} 2>&1 || exit 1; \
setup_size=$(wc -c < ${BUILD_DIR}/${ZX_NAME}_SETUP.OVL); \
${ZX_Z80ASM} asm/overlay/saveload/entry_saveload.asm 2>&1 || exit 1; \
if [ -n "${ESX_SAVELOAD_ASM}" ]; then ${ZX_Z80ASM} ${ESX_SAVELOAD_ASM} 2>&1 || exit 1; fi; \
(cd ${BUILD_DIR} && ${ZCC} +z80 ${ZX_OVL_CFLAGS} -c ${BUILD_DIR_UP}src/spectrum/overlay/saveload_ovl.c -o saveload_ovl.o) 2>&1 || exit 1; \
if [ -n "${SPXN_ATOMIC_SRC}" ]; then (cd ${BUILD_DIR} && ${ZCC} +z80 ${ZX_OVL_CFLAGS} -c ${SPXN_ATOMIC_SRC} -o spxf_replace_ovl.o) 2>&1 || exit 1; fi; \
rm -f ${BUILD_DIR}/overlay_defs.o ${BUILD_DIR}/overlay_defs.o~; \
${ZX_Z80ASM} -b -r0x$SLOT -o=${BUILD_DIR}/${ZX_NAME}_SAVELOAD.OVL \
	asm/overlay/saveload/entry_saveload.o ${BUILD_DIR}/saveload_ovl.o ${SPXN_ATOMIC_OBJ} ${ESX_SAVELOAD_OBJ} ${OVL_DEFS} 2>&1 || exit 1; \
saveload_size=$(wc -c < ${BUILD_DIR}/${ZX_NAME}_SAVELOAD.OVL); \
${ZX_Z80ASM} asm/overlay/restore/entry_restore.asm 2>&1 || exit 1; \
(cd ${BUILD_DIR} && ${ZCC} +z80 ${ZX_OVL_CFLAGS} -c ${BUILD_DIR_UP}src/spectrum/overlay/restore_ovl.c -o restore_ovl.o) 2>&1 || exit 1; \
rm -f ${BUILD_DIR}/overlay_defs.o ${BUILD_DIR}/overlay_defs.o~; \
${ZX_Z80ASM} -b -r0x$SLOT -o=${BUILD_DIR}/${ZX_NAME}_RESTORE.OVL \
	asm/overlay/restore/entry_restore.o ${BUILD_DIR}/restore_ovl.o ${OVL_DEFS} 2>&1 || exit 1; \
restore_size=$(wc -c < ${BUILD_DIR}/${ZX_NAME}_RESTORE.OVL); \
${ZX_Z80ASM} asm/overlay/about/entry_about.asm 2>&1 || exit 1; \
rm -f ${BUILD_DIR}/overlay_defs.o ${BUILD_DIR}/overlay_defs.o~; \
${ZX_Z80ASM} -b -r0x$SLOT -o=${BUILD_DIR}/${ZX_NAME}_ABOUT.OVL \
	asm/overlay/about/entry_about.o ${OVL_DEFS} 2>&1 || exit 1; \
about_size=$(wc -c < ${BUILD_DIR}/${ZX_NAME}_ABOUT.OVL); \
${ZX_Z80ASM} asm/overlay/control/entry_control.asm 2>&1 || exit 1; \
(cd ${BUILD_DIR} && ${ZCC} +z80 ${ZX_OVL_CFLAGS} -c ${BUILD_DIR_UP}src/spectrum/overlay/control_ovl.c -o control_ovl.o) 2>&1 || exit 1; \
(cd ${BUILD_DIR} && ${ZCC} +z80 ${ZX_OVL_CFLAGS} -c ${BUILD_DIR_UP}${GAME_PROTOCOL_MACH_SRC} -o game_protocol_mach_ovl.o) 2>&1 || exit 1; \
rm -f ${BUILD_DIR}/overlay_defs.o ${BUILD_DIR}/overlay_defs.o~; \
${ZX_Z80ASM} -b -r0x$SLOT -o=${BUILD_DIR}/${ZX_NAME}_CONTROL.OVL \
	asm/overlay/control/entry_control.o ${BUILD_DIR}/control_ovl.o ${BUILD_DIR}/game_protocol_mach_ovl.o ${OVL_DEFS} 2>&1 || exit 1; \
control_size=$(wc -c < ${BUILD_DIR}/${ZX_NAME}_CONTROL.OVL); \
${ZX_Z80ASM} asm/overlay/config/entry_config.asm 2>&1 || exit 1; \
(cd ${BUILD_DIR} && ${ZCC} +z80 ${ZX_OVL_CFLAGS} -c ${BUILD_DIR_UP}src/spectrum/overlay/config_ovl.c -o config_ovl.o) 2>&1 || exit 1; \
rm -f ${BUILD_DIR}/overlay_defs.o ${BUILD_DIR}/overlay_defs.o~; \
${ZX_Z80ASM} -b -r0x$SLOT -o=${BUILD_DIR}/${ZX_NAME}_CONFIG.OVL \
	asm/overlay/config/entry_config.o ${BUILD_DIR}/config_ovl.o ${SPXN_ATOMIC_OBJ} ${ESX_SAVELOAD_OBJ} ${OVL_DEFS} 2>&1 || exit 1; \
config_size=$(wc -c < ${BUILD_DIR}/${ZX_NAME}_CONFIG.OVL); \
${ZX_Z80ASM} asm/overlay/time_config/entry_time_config.asm 2>&1 || exit 1; \
rm -f ${BUILD_DIR}/overlay_defs.o ${BUILD_DIR}/overlay_defs.o~; \
${ZX_Z80ASM} -b -r0x$SLOT -o=${BUILD_DIR}/${ZX_NAME}_TIME_CONFIG.OVL \
	asm/overlay/time_config/entry_time_config.o asm/overlay/edit/edit_buf.o ${OVL_DEFS} 2>&1 || exit 1; \
time_config_size=$(wc -c < ${BUILD_DIR}/${ZX_NAME}_TIME_CONFIG.OVL); \
time_size=0; \
if [ "${NET_BACKEND}" = "spectranext" ]; then \
	${ZX_Z80ASM} asm/overlay/time/entry_time.asm 2>&1 || exit 1; \
	(cd ${BUILD_DIR} && ${ZCC} +z80 ${ZX_OVL_CFLAGS} ${SPXN_TIME_OVL_CFLAGS} -c ${BUILD_DIR_UP}src/spectrum/overlay/time_ovl.c -o time_ovl.o) 2>&1 || exit 1; \
	(cd ${BUILD_DIR} && ${ZCC} +z80 ${ZX_OVL_CFLAGS} ${SPXN_TIME_OVL_CFLAGS} -c ${SPXN_UDP_SRC} -o spxudp_ovl.o) 2>&1 || exit 1; \
	(cd ${BUILD_DIR} && ${ZCC} +z80 ${ZX_OVL_CFLAGS} ${SPXN_TIME_OVL_CFLAGS} -c ${SPXN_TIME_SRC} -o spxtime_ovl.o) 2>&1 || exit 1; \
	rm -f ${BUILD_DIR}/overlay_defs.o ${BUILD_DIR}/overlay_defs.o~; \
	${ZX_Z80ASM} -b -r0x$SLOT -o=${BUILD_DIR}/${ZX_NAME}_TIME.OVL \
		asm/overlay/time/entry_time.o ${BUILD_DIR}/time_ovl.o ${BUILD_DIR}/spxudp_ovl.o ${BUILD_DIR}/spxtime_ovl.o ${OVL_DEFS} 2>&1 || exit 1; \
	time_size=$(wc -c < ${BUILD_DIR}/${ZX_NAME}_TIME.OVL); \
fi; \
status_size=0; \
${PYTHON} tools/gen_overlay_atlas.py --build-dir ${BUILD_DIR} --name ${ZX_NAME} --out ${ZX_OVL} --asm-out ${OVL_ATLAS_TABLE} --changed-stamp ${BUILD_DIR}/overlay_atlas_table.changed --sizes-out ${BUILD_DIR}/overlay_sizes.json ${OVERLAY_ATLAS_FLAGS} || exit 1; \
atlas_changed=$(cat ${BUILD_DIR}/overlay_atlas_table.changed 2>/dev/null || echo 0); \
if [ "$atlas_changed" = "1" ] && [ "${ATLAS_FINAL}" != "1" ]; then \
	printf "[INFO] overlay atlas table changed; rebuilding resident with baked offsets\\n"; \
	${MAKE} ATLAS_FINAL=1 ${ZX_OVL} || exit 1; \
	exit 0; \
fi; \
if [ "$atlas_changed" = "1" ] && [ "${ATLAS_FINAL}" = "1" ]; then \
	printf "[ERR] overlay atlas table changed during final pass\\n"; \
	exit 1; \
fi; \
ovl_total=$(wc -c < ${ZX_OVL}); \
printf "[OK] ${ZX_NAME}.OVL atlas: RULES $ovl_size bytes, BOARD $board_size bytes, GUI_LOG $gui_log_size bytes, INPUT_EDIT $input_edit_size bytes, MQTT_CONNECT $mqtt_connect_size bytes, MQTT_TX $mqtt_tx_size bytes, DIRECT $direct_size bytes, MENU_CONFIG $menu_config_size bytes, MENU_LOGIC $menu_logic_size bytes, HINTS $hints_size bytes, SETUP $setup_size bytes, FILEUI $fileui_size bytes, STATUS $status_size bytes, SAVELOAD $saveload_size bytes, RESTORE $restore_size bytes, ABOUT $about_size bytes, CONTROL $control_size bytes, CONFIG $config_size bytes, TIME_CONFIG $time_config_size bytes, TIME $time_size bytes, total $ovl_total bytes\n"
) || exit 1
