/* 【10 启动记录存储接口】
 * 阅读导航：docs/代码阅读地图.md。
 */
#ifndef BOOT_JOURNAL_H
#define BOOT_JOURNAL_H
#include <stdbool.h>
#include <stdint.h>
#define BOOT_PAGE_WORDS 256u
typedef struct {
    const uint32_t *pages[2];
    bool (*erase)(const uint32_t *page);
    bool (*program)(const uint32_t *address,uint32_t value);
} boot_storage_t;
bool boot_journal_next(const boot_storage_t *storage,uint32_t *counter);
#endif
