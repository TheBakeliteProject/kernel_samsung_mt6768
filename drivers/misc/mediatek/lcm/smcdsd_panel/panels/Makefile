obj-$(CONFIG_SMCDSD_PANEL) += ../smcdsd_panel_init.o ../smcdsd_panel.o

obj-$(CONFIG_SMCDSD_PANEL) += ../../../../../gpu/drm/panel/smcdsd_panel/smcdsd_regmap.o ../../../../../gpu/drm/panel/smcdsd_panel/smcdsd_supply.o

obj-$(CONFIG_SMCDSD_PANEL) += ../smcdsd_board.o ../smcdsd_notify.o ../smcdsd_abd.o

obj-$(CONFIG_SMCDSD_DPUI) += dpui.o
obj-$(CONFIG_SMCDSD_MDNIE) += mdnie.o

obj-$(and $(CONFIG_SMCDSD_MDNIE),$(CONFIG_SMCDSD_LCD_DEBUG),$(subst undefined,y,$(origin CONFIG_SAMSUNG_PRODUCT_SHIP))) += dd_mdnie.o
obj-$(and $(CONFIG_SMCDSD_LCD_DEBUG),$(subst undefined,y,$(origin CONFIG_SAMSUNG_PRODUCT_SHIP))) += dd_lcd.o dd_backlight.o dd_param.o

obj-$(CONFIG_SMCDSD_PANEL_A32)	+= s6e3fc3_a32.o
obj-$(CONFIG_SMCDSD_PANEL_M32)	+= s6e3fc3_m32.o
obj-$(CONFIG_SMCDSD_PANEL_A22)	+= s6e3fc3_a22.o s6e3fc3_a32.o
obj-$(CONFIG_SMCDSD_PANEL_M22)	+= s6e3fc3_m22.o

obj-$(CONFIG_SMCDSD_PANEL_A12)	+= ili9882n_a12.o nt36525b_a12.o nt36525b_dtc_a12.o nt36525b_boe_a12.o nt36675_a12.o

obj-$(CONFIG_SMCDSD_PANEL_A12U)	+= ili9882n_a12.o nt36525b_a12.o nt36525b_dtc_a12.o nt36525b_boe_a12.o nt36675_a12.o

obj-$(CONFIG_SMCDSD_PANEL_A13VE)	+= a13ve_nt36672c_csot.o a13ve_hx83112f_boe.o