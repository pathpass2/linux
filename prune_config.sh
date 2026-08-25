#!/bin/bash

# 禁用所有 panel 驱动
scripts/config --disable CONFIG_DRM_PANEL
scripts/config --disable CONFIG_DRM_PANEL_SIMPLE
scripts/config --disable CONFIG_DRM_PANEL_EDP

# 禁用特定品牌 panel
scripts/config --disable CONFIG_DRM_PANEL_ABT_Y030XX067A
scripts/config --disable CONFIG_DRM_PANEL_ARM_VERSATILE
scripts/config --disable CONFIG_DRM_PANEL_BOE_BF060Y8M_AJ0
scripts/config --disable CONFIG_DRM_PANEL_BOE_HIMAX8279D
scripts/config --disable CONFIG_DRM_PANEL_BOE_TV101WUM_NL6
scripts/config --disable CONFIG_DRM_PANEL_ELIDA_KD35T133
scripts/config --disable CONFIG_DRM_PANEL_FEIXIN_K101_IM2BA02
scripts/config --disable CONFIG_DRM_PANEL_FEIYANG_FY07024DI26A30D
scripts/config --disable CONFIG_DRM_PANEL_DSI_CM
scripts/config --disable CONFIG_DRM_PANEL_LVDS
scripts/config --disable CONFIG_DRM_PANEL_ILITEK_IL9322
scripts/config --disable CONFIG_DRM_PANEL_ILITEK_ILI9341
scripts/config --disable CONFIG_DRM_PANEL_ILITEK_ILI9881C
scripts/config --disable CONFIG_DRM_PANEL_INNOLUX_EJ030NA
scripts/config --disable CONFIG_DRM_PANEL_INNOLUX_P079ZCA
scripts/config --disable CONFIG_DRM_PANEL_JADARD_JD9365DA_H3
scripts/config --disable CONFIG_DRM_PANEL_JDI_LT070ME05000
scripts/config --disable CONFIG_DRM_PANEL_JDI_R63452
scripts/config --disable CONFIG_DRM_PANEL_KHADAS_TS050
scripts/config --disable CONFIG_DRM_PANEL_KINGDISPLAY_KD097D04
scripts/config --disable CONFIG_DRM_PANEL_LEADTEK_LTK500HD1829
scripts/config --disable CONFIG_DRM_PANEL_LG_LB035Q02
scripts/config --disable CONFIG_DRM_PANEL_NEC_NL8048HL11
scripts/config --disable CONFIG_DRM_PANEL_NEWVISION_NV3052C
scripts/config --disable CONFIG_DRM_PANEL_NOVATEK_NT35510
scripts/config --disable CONFIG_DRM_PANEL_NOVATEK_NT35950
scripts/config --disable CONFIG_DRM_PANEL_NOVATEK_NT36672A
scripts/config --disable CONFIG_DRM_PANEL_NOVATEK_NT39016
scripts/config --disable CONFIG_DRM_PANEL_OLIMEX_LCD_OLINUXINO
scripts/config --disable CONFIG_DRM_PANEL_ORISETECH_OTM8009A
scripts/config --disable CONFIG_DRM_PANEL_PANASONIC_VVX10F034N00
scripts/config --disable CONFIG_DRM_PANEL_RASPBERRYPI_TOUCHSCREEN
scripts/config --disable CONFIG_DRM_PANEL_RAYDIUM_RM67191
scripts/config --disable CONFIG_DRM_PANEL_RAYDIUM_RM68200
scripts/config --disable CONFIG_DRM_PANEL_RONBO_RB070D30
scripts/config --disable CONFIG_DRM_PANEL_SAMSUNG_S6E88A0_AMS452EF01
scripts/config --disable CONFIG_DRM_PANEL_SAMSUNG_ATNA33XC20
scripts/config --disable CONFIG_DRM_PANEL_SAMSUNG_DB7430
scripts/config --disable CONFIG_DRM_PANEL_SAMSUNG_S6D16D0
scripts/config --disable CONFIG_DRM_PANEL_SAMSUNG_S6D27A1
scripts/config --disable CONFIG_DRM_PANEL_SAMSUNG_S6E3HA2
scripts/config --disable CONFIG_DRM_PANEL_SAMSUNG_S6E63J0X03
scripts/config --disable CONFIG_DRM_PANEL_SAMSUNG_SOFEF00
scripts/config --disable CONFIG_DRM_PANEL_SEIKO_43WVF1G
scripts/config --disable CONFIG_DRM_PANEL_SHARP_LQ101R1SX01
scripts/config --disable CONFIG_DRM_PANEL_SHARP_LS037V7DW01
scripts/config --disable CONFIG_DRM_PANEL_SHARP_LS043T1LE01
scripts/config --disable CONFIG_DRM_PANEL_SHARP_LS060T1SX01
scripts/config --disable CONFIG_DRM_PANEL_SITRONIX_ST7701
scripts/config --disable CONFIG_DRM_PANEL_SONY_ACX565AKM
scripts/config --disable CONFIG_DRM_PANEL_SONY_TULIP_TRULY_NT35521
scripts/config --disable CONFIG_DRM_PANEL_TDO_TL070WSH30
scripts/config --disable CONFIG_DRM_PANEL_TPO_TD028TTEC1
scripts/config --disable CONFIG_DRM_PANEL_TPO_TD043MTEA1
scripts/config --disable CONFIG_DRM_PANEL_TPO_TPG110
scripts/config --disable CONFIG_DRM_PANEL_TRULY_NT35597_WQXGA
scripts/config --disable CONFIG_DRM_PANEL_WIDECHIPS_WS2401
scripts/config --disable CONFIG_DRM_PANEL_XINPENG_XPP055C272
scripts/config --disable CONFIG_DRM_PANEL_MIPI_DBI
scripts/config --disable CONFIG_DRM_PANEL_ORIENTATION_QUIRKS

# 重新生成配置
make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- olddefconfig
