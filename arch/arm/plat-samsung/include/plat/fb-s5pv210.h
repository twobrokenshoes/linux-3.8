#ifndef __ASM_PLAT_FB_S5PV210_H
#define __ASM_PLAT_FB_S5PV210_H __FILE__

struct s5pv210_lcd_reg {
	unsigned long vidcon0;
	unsigned long vidcon1;
	unsigned long vidtcon0;
	unsigned long vidtcon1;
	unsigned long vidtcon2;
	unsigned long vidtcon3;
	unsigned long wincon0;
	unsigned long wincon2;
	unsigned long shadowcon;
	unsigned long vidosd0a;
	unsigned long vidosd0b;
	unsigned long vidosd0c;
	unsigned long vidw00addr0b0;
	unsigned long vidw00addr1b0;
	unsigned long vidw00addr2;
};

struct s5pv210_fb_mach_info{
	void (*set_power)(struct plat_lcd_data *,unsigned int);
	struct s5pv210_lcd_reg lcd_reg;
	unsigned long gpf0con;
	unsigned long gpf0con_mask;
	unsigned long gpf1con;
	unsigned long gpf1con_mask;
	unsigned long gpf2con;
	unsigned long gpf2con_mask;
	unsigned long gpf3con;
	unsigned long gpf3con_mask;
};


#endif
