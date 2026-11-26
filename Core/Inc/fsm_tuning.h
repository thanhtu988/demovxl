/*
 * fsm_tuning.h  (C) -- may trang thai che do CHINH GIO (TUNING)
 *
 * Trang thai = den dang duoc chon de chinh: TUNE_RED -> TUNE_GREEN -> TUNE_AMBER -> TUNE_RED
 *   B2   : chon den ke tiep
 *   B3   : +1 s  (giu >= 1 s: tang lien tuc 5 lan/s, nho button_held() cua B)
 *   B4   : -1 s  (giu >= 1 s: giam lien tuc)
 *   USER : Luu -> kiem tra hop le; dung thi ghi vao g_time_*, sai thi giu gia tri cu
 *   B1   : KHONG xu ly o day -- fsm_mode (A) bat B1 de ve AUTO.
 *
 * Nguoi dung chi sua tren BAN SAO TAM; g_time_* chi doi khi Luu hop le.
 * Rang buoc: T_DO = T_XANH + T_VANG, T_XANH >= 1, T_VANG >= 1, T_DO <= 99.
 */
#ifndef INC_FSM_TUNING_H_
#define INC_FSM_TUNING_H_

#include <stdint.h>

#define TUNE_VALUE_MIN   0U     /* cho phep chinh xuong 0 de minh hoa bat loi khi Luu */
#define TUNE_VALUE_MAX   99U    /* LCD hien 2 chu so                                   */
#define TUNE_MSG_MS      2000U  /* thoi gian hien SAVED / ERR                          */

typedef enum {
    TUNE_RED = 0,
    TUNE_GREEN,
    TUNE_AMBER
} tune_state_t;

typedef enum {
    TUNE_OK = 0,
    TUNE_ERR_MIN,      /* T_XANH hoac T_VANG < 1   */
    TUNE_ERR_SUM,      /* T_DO != T_XANH + T_VANG  */
    TUNE_ERR_MAX       /* T_DO > 99                */
} tune_result_t;

void          fsm_tuning_enter(void);   /* hanh dong entry (A goi khi MANUAL -> TUNING) */
void          fsm_tuning_run(void);     /* A goi trong fsm_mode_run khi g_mode == MODE_TUNING */
void          fsm_tuning_exit(void);    /* hanh dong exit  (A goi khi TUNING -> AUTO)    */
tune_state_t  fsm_tuning_state(void);

/* ham thuan -- dung lai trong bao cao va test PC */
tune_result_t fsm_tuning_validate(uint8_t red, uint8_t green, uint8_t amber);

#endif /* INC_FSM_TUNING_H_ */
