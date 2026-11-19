#ifndef STC_LABELS_H
#define STC_LABELS_H
/* Single source of category order, issue #3 section 4. */
#define STC_CATEGORY_LIST(X) \
    X(0, "可回收物") \
    X(1, "廚餘") \
    X(2, "有害") \
    X(3, "其他")
#define STC_CATEGORY_COUNT 4
#endif
