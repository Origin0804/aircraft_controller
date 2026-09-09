/**
  ******************************************************************************
  * @file    fc/fc_app.h
  * @brief   FC 应用入口（main() 的 USER CODE 调用点）
  ******************************************************************************
  */
#ifndef FC_APP_H
#define FC_APP_H

#ifdef __cplusplus
extern "C" {
#endif

void FC_Init(void);
void FC_Loop(void);

#ifdef __cplusplus
}
#endif

#endif /* FC_APP_H */
