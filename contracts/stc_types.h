/* stc_types.h — 共用型別（contract v1.0.0）
 * 四個 seam 都用這裡的 enum。不要在其他地方另立類別順序。 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 桶位與類別索引共用同一順序（凍結） */
typedef enum {
    STC_BIN_RECYCLABLE = 0,
    STC_BIN_FOOD       = 1,
    STC_BIN_HAZARD     = 2,
    STC_BIN_OTHER      = 3,
    STC_BIN_COUNT      = 4
} stc_bin_t;

typedef enum {
    STC_OK             = 0,
    STC_ERR_TIMEOUT    = -1,
    STC_ERR_NOT_READY  = -2,
    STC_ERR_IO         = -3,
    STC_ERR_CONTRACT   = -4,   /* 模型／設定與契約不符 */
    STC_ERR_CANCELLED  = -5,
    STC_ERR_UNSUPPORTED= -6
} stc_err_t;

typedef enum { STC_SRC_REAL = 0, STC_SRC_STUB = 1 } stc_source_t;

/* 兩個運行模式：GAME 預設（雲端通道關），CHAT 才開小智／B1 */
typedef enum { STC_MODE_GAME = 0, STC_MODE_CHAT = 1 } stc_mode_t;

typedef struct {
    stc_bin_t    bin;        /* 預測類別 */
    uint8_t      class_idx;  /* 原始輸出索引 */
    float        conf;       /* 0..1 */
    bool         abstain;    /* conf < threshold */
    uint32_t     t_ms;       /* 抓拍完成的相對時間 */
    stc_source_t src;        /* real / stub */
} stc_label_t;

typedef struct {
    stc_bin_t bin;
    uint16_t  dist_mm;       /* 觸發時量到的距離 */
    uint32_t  t_ms;
} stc_drop_t;

typedef enum {
    STC_EV_PLAY = 0,     /* PLAY 鍵 */
    STC_EV_LABEL,        /* stc_label_t 就緒 */
    STC_EV_DROP,         /* stc_drop_t */
    STC_EV_INSERT_TIMEOUT,
    STC_EV_MODE_CHANGED  /* stc_mode_t */
} stc_ev_type_t;

typedef struct {
    stc_ev_type_t type;
    union {
        stc_label_t label;
        stc_drop_t  drop;
        stc_mode_t  mode;
    } u;
    uint32_t t_ms;
} stc_event_t;

#ifdef __cplusplus
}
#endif
