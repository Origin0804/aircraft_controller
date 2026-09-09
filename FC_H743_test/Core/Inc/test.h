#ifndef TEST_H
#define TEST_H

#ifdef __cplusplus
extern "C" {
#endif

/* 批量外设自检：跑一遍并打印 OK/FAIL 到 USART1 控制台 */
void Test_Run(void);

#ifdef __cplusplus
}
#endif

#endif /* TEST_H */
