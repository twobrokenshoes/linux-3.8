/* linux/drivers/video/s5pv210fb.c
 *	Copyright (c) 2004,2005 Arnaud Patard
 *	Copyright (c) 2004-2008 Ben Dooks
 *
 * S3C2410 LCD Framebuffer Driver
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * Driver based on skeletonfb.c, sa1100fb.c and others.
*/

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/io.h>

#include <asm/div64.h>

#include <asm/mach/map.h>
#include <mach/regs-lcd.h>
#include <mach/regs-gpio.h>
#include <mach/fb.h>

#include <plat/fb-s5pv210.h>
#include <plat/fb.h>

#ifdef CONFIG_PM
#include <linux/pm.h>
#endif

#include "s5pv210fb.h"

/* Debugging stuff */
#ifdef CONFIG_FB_S5PV210_DEBUG
static int debug	= 1;
#else
static int debug	= 0;
#endif

#define dprintk(msg...) \
do { \
	if (debug) \
		pr_debug(msg); \
} while (0)




int lcd_draw_pixel(struct fb_info *info,int row, int col, int color)
{
	volatile unsigned int * pixel = (int *)info->screen_base;

	*(pixel + row * info->var.xres + col) = color;	

	return 0;
} 

int lcd_clear_screen(struct fb_info *info,int color)
{
	int i, j;
		
	for (i = 0; i < info->var.yres; i++)
		for (j = 0; j < info->var.xres; j++)
		{
			lcd_draw_pixel(info,i, j, color);
		}

	return 0;
}







/*
 * s5pv210fb_map_video_memory():
 *	Allocates the DRAM memory for the frame buffer.  This buffer is
 *	remapped into a non-cached, non-buffered, memory region to
 *	allow palette and pixel writes to occur without flushing the
 *	cache.  Once this area is remapped, all virtual memory
 *	access to the video memory should occur at the new region.
 */
static int s5pv210fb_map_video_memory(struct fb_info *info)
{
	struct s5pv210fb_info *fbi = info->par;
	dma_addr_t map_dma;
	unsigned map_size = PAGE_ALIGN(info->fix.smem_len + PAGE_SIZE);

	dprintk("map_video_memory(fbi=%p) map_size %8x\n", fbi, map_size);

	info->screen_base = dma_alloc_writecombine(fbi->dev, map_size,
						   &map_dma, GFP_KERNEL);

	if (info->screen_base) {
	
		info->screen_size = map_size;
	
		/* prevent initial garbage on screen */
		dprintk("map_video_memory: clear %p:%08x\n",
			info->screen_base, map_size);
		memset(info->screen_base, 0x00, map_size);

		info->fix.smem_start = map_dma;

		dprintk("map_video_memory: dma=%08lx cpu=%p size=%08x\n",
			info->fix.smem_start, info->screen_base, map_size);
	}

	return info->screen_base ? 0 : -ENOMEM;
}

static inline void s5pv210fb_unmap_video_memory(struct fb_info *info)
{
	struct s5pv210fb_info *fbi = info->par;

	dma_free_writecombine(fbi->dev, PAGE_ALIGN(info->fix.smem_len),
			      info->screen_base, info->fix.smem_start);
}

/*
 * s5pv210fb_init_registers - Initialise all LCD-related registers
 */
static int s5pv210fb_init_registers(struct fb_info *info)
{
	struct s5pv210fb_info *fbi = info->par;
	struct s5pv210_fb_mach_info *mach_info = fbi->dev->platform_data;
	unsigned long int_flags;
	unsigned int flags;
	void __iomem *gpf0con;
	void __iomem *gpd0con;
	unsigned int *display_control;
//	void __iomem *
//	void __iomem *tpal;

//	tpal = 	fbi->io + S5PV210_TPAL;

	/* Initialise LCD with values from haret */

	local_irq_save(int_flags);
	
	/*set the gpios related to lcd*/
	gpf0con = ioremap(0xe0200120,0x80);
	display_control = ioremap(0xe0107008,0x04);
	gpd0con = ioremap(0xE02000A0,0x08);
	
	dprintk("gpf0con		=0x%8lx\n",mach_info->gpf0con);
	dprintk("gpf1con		=0x%8lx\n",mach_info->gpf1con);
	dprintk("gpf2con		=0x%8lx\n",mach_info->gpf2con);
	dprintk("gpf3con		=0x%8lx\n",mach_info->gpf3con);
	
	flags = readl(gpf0con + 0x00);
	flags &= ~(mach_info->gpf0con_mask);
	flags |= mach_info->gpf0con;
	writel(flags,gpf0con + 0x00);
	
	flags = readl(gpf0con + 0x20);
	flags &= ~(mach_info->gpf1con_mask);
	flags |= mach_info->gpf1con;
	writel(flags,gpf0con + 0x20);
	
	flags = readl(gpf0con + 0x40);
	flags &= ~(mach_info->gpf2con_mask);
	flags |= mach_info->gpf2con;
	writel(flags,gpf0con + 0x40);
	
	flags = readl(gpf0con + 0x60);
	flags &= ~(mach_info->gpf3con_mask);
	flags |= mach_info->gpf3con;
	writel(flags,gpf0con + 0x60);
	
	flags = readl(gpd0con);
	flags &= ~((0x0f<<12)|(0x0f<<0));
	flags |= (0x01<<12)|(0x01<<0);
	writel(flags,gpd0con);
	
	flags = readl(gpd0con+4);
	flags |= (0x01<<3)|(0x01<<0);
	writel(flags,gpd0con+4);
	
	writel(2<<0,display_control);
	
/*	iounmap(gpf0con);
	iounmap(gpd0con);
	iounmap(display_control);*/
	
	local_irq_restore(int_flags);
	
//	writel(0x00, tpal);
	
	return 0;
}

static int s5pv210_set_window(struct fb_info *info)
{
	struct s5pv210fb_info *fbi = info->par;
	struct s5pv210_fb_mach_info *mach_info = fbi->dev->platform_data;
	void __iomem *regs = fbi->io;
	struct fb_var_screeninfo *var = &info->var;
	
	mach_info->lcd_reg.vidosd0a = S5PV210_LCD_LEFTTOPX(0x00)|
								  S5PV210_LCD_LEFTTOPY(0x00);
								  
	mach_info->lcd_reg.vidosd0b = S5PV210_LCD_LEFTTOPX(var->xres - 1)|
								  S5PV210_LCD_LEFTTOPY(var->yres - 1);
		  
	mach_info->lcd_reg.vidosd0c = var->xres * var->yres;
	
	mach_info->lcd_reg.vidw00addr0b0 = info->fix.smem_start;
	
	mach_info->lcd_reg.vidw00addr1b0 = info->fix.smem_start + ((var->xres * var->yres ) * (var->bits_per_pixel>>3));
	
//	mach_info->lcd_reg.vidw00addr2 = (var->xres*((var->bits_per_pixel == 16)?2:4));
	
	dprintk("vidosd0a	= 0x%08lx\n", mach_info->lcd_reg.vidosd0a);
	dprintk("vidosd0b	= 0x%08lx\n", mach_info->lcd_reg.vidosd0b);
	dprintk("vidosd0c	= 0x%08lx\n", mach_info->lcd_reg.vidosd0c);
	dprintk("vidw00addr0b0	= 0x%08lx\n", mach_info->lcd_reg.vidw00addr0b0);
	dprintk("vidw00addr1b0	= 0x%08lx\n", mach_info->lcd_reg.vidw00addr1b0);
//	dprintk("vidw00addr2	= 0x%08lx\n", mach_info->lcd_reg.vidw00addr2);
	
	writel(mach_info->lcd_reg.vidosd0a, regs + S5PV210_VIDOSD0A);
	writel(mach_info->lcd_reg.vidosd0b, regs + S5PV210_VIDOSD0B);
	writel(mach_info->lcd_reg.vidosd0c, regs + S5PV210_VIDOSD0C);
	writel(mach_info->lcd_reg.vidw00addr0b0, regs + S5PV210_VIDW00ADD0B0);
	writel(mach_info->lcd_reg.vidw00addr1b0, regs + S5PV210_VIDW00ADD1B0);
//	writel(mach_info->lcd_reg.vidw00addr2, regs + S5PV210_VIDW00ADD2);
	
	return 0;
}

/* s5pv210fb_activate_var
 *
 * activate (set) the controller from the given framebuffer
 * information
 */
static void s5pv210fb_activate_var(struct fb_info *info)
{
	struct s5pv210fb_info *fbi = info->par;
	struct s5pv210_fb_mach_info *mach_info = fbi->dev->platform_data;
	void __iomem *regs = fbi->io;
	struct fb_var_screeninfo *var = &info->var;


	dprintk("%s: var->xres  = %d\n", __func__, var->xres);
	dprintk("%s: var->yres  = %d\n", __func__, var->yres);
	dprintk("%s: var->bpp   = %d\n", __func__, var->bits_per_pixel);

	/* write new registers */
	mach_info->lcd_reg.vidcon0 &= ~((3<<26) | (1<<18) | (0xff<<6)  | (1<<2));
	mach_info->lcd_reg.vidcon0 |= ((var->pixclock<<6) | (1<<4) );
	
	mach_info->lcd_reg.vidcon1 &= ~(1<<7);
	mach_info->lcd_reg.vidcon1 |= ((1<<6) | (1<<5) );

	mach_info->lcd_reg.vidtcon0 = S5PV210_LCD_VBPD(var->upper_margin - 1)|
								  S5PV210_LCD_VFPD(var->lower_margin - 1)|
								  S5PV210_LCD_VSPW(var->vsync_len - 1);
	
	mach_info->lcd_reg.vidtcon1 = S5PV210_LCD_HBPD(var->left_margin - 1)|
								  S5PV210_LCD_HFPD(var->right_margin - 1)|
								  S5PV210_LCD_HSPW(var->hsync_len - 1);
							
	mach_info->lcd_reg.vidtcon2 = S5PV210_LCD_LINEVAL(var->yres - 1)|
								  S5PV210_LCD_HOZVAL(var->xres - 1);
							
	mach_info->lcd_reg.wincon0	= ((var->bits_per_pixel==16)?S5PV210_LCD_FRAME565:S5PV210_LCD_FRAME888)|S5PV210_LCD_WSWP_ENABLE;

	dprintk("new register set:\n");
	dprintk("vidcon0		= 0x%08lx\n", mach_info->lcd_reg.vidcon0);
	dprintk("vidcon1		= 0x%08lx\n", mach_info->lcd_reg.vidcon1);
	dprintk("vidtcon0	= 0x%08lx\n", mach_info->lcd_reg.vidtcon0);
	dprintk("vidtcon1	= 0x%08lx\n", mach_info->lcd_reg.vidtcon1);
	dprintk("vidtcon2	= 0x%08lx\n", mach_info->lcd_reg.vidtcon2);
	dprintk("wincon0		= 0x%08lx\n", mach_info->lcd_reg.wincon0);

	writel(mach_info->lcd_reg.vidcon0, regs + S5PV210_VIDCON0);
	writel(mach_info->lcd_reg.vidcon1, regs + S5PV210_VIDCON1);
	writel(mach_info->lcd_reg.vidtcon0, regs + S5PV210_VIDTCON0);
	writel(mach_info->lcd_reg.vidtcon1, regs + S5PV210_VIDTCON1);
	writel(mach_info->lcd_reg.vidtcon2, regs + S5PV210_VIDTCON2);
	writel(mach_info->lcd_reg.wincon0, regs + S5PV210_WINCON0);

	/* set lcd address pointers */
	s5pv210_set_window(info);

	mach_info->lcd_reg.shadowcon = S5PV210_LCD_CHANNEL0_ENABLE;
	mach_info->lcd_reg.vidcon0 |= S5PV210_LCD_ENVID;
	mach_info->lcd_reg.wincon0 |= S5PV210_LCD_WIN0_ENABLE;
	
	writel(mach_info->lcd_reg.shadowcon,regs + S5PV210_SHADOWCON);
	writel(mach_info->lcd_reg.vidcon0,regs + S5PV210_VIDCON0);
	writel(mach_info->lcd_reg.wincon0,regs + S5PV210_WINCON0);
}

/*
 *	s5pv210fb_check_var():
 *	Get the video params out of 'var'. If a value doesn't fit, round it up,
 *	if it's too big, return -EINVAL.
 *
 */
static int s5pv210fb_check_var(struct fb_var_screeninfo *var,
			       struct fb_info *info)
{
	struct s5pv210fb_info *fbi = info->par;
//	struct s5pv210_fb_mach_info *mach_info = fbi->dev->platform_data;
	struct s3c_fb_platdata *lcd_pdata = fbi->dev->parent->platform_data;
		
	dprintk("check_var(var=%p, info=%p)\n", var, info);

	
	

	/* it is always the size as the display */
	var->xres_virtual = lcd_pdata->win[0]->xres;
	var->yres_virtual = lcd_pdata->win[0]->yres;
	var->height = lcd_pdata->vtiming->yres;
	var->width = lcd_pdata->vtiming->xres;

	/* copy lcd settings */
	var->pixclock = lcd_pdata->vtiming->pixclock;
	var->left_margin = lcd_pdata->vtiming->left_margin;
	var->right_margin = lcd_pdata->vtiming->right_margin;
	var->upper_margin = lcd_pdata->vtiming->upper_margin;
	var->lower_margin = lcd_pdata->vtiming->lower_margin;
	var->vsync_len = lcd_pdata->vtiming->vsync_len;
	var->hsync_len = lcd_pdata->vtiming->hsync_len;

	var->transp.offset = 0;
	var->transp.length = 0;
	/* set r/g/b positions */
	switch (var->bits_per_pixel) {
	case 1:
	case 2:
	case 4:
		var->red.offset	= 0;
		var->red.length	= var->bits_per_pixel;
		var->green	= var->red;
		var->blue	= var->red;
		break;
	case 8:
		/* 8 bpp 332 */
		var->red.length		= 3;
		var->red.offset		= 5;
		var->green.length	= 3;
		var->green.offset	= 2;
		var->blue.length	= 2;
		var->blue.offset	= 0;
		break;
	case 12:
		/* 12 bpp 444 */
		var->red.length		= 4;
		var->red.offset		= 8;
		var->green.length	= 4;
		var->green.offset	= 4;
		var->blue.length	= 4;
		var->blue.offset	= 0;
		break;
	case 16:
		/* 16 bpp, 565 format */
		var->red.offset		= 11;
		var->green.offset	= 5;
		var->blue.offset	= 0;
		var->red.length		= 5;
		var->green.length	= 6;
		var->blue.length	= 5;
		break;
	case 24:
	case 32:
		/* 24 bpp 888 and 8 dummy */
		var->red.length		= 8;
		var->red.offset		= 16;
		var->green.length	= 8;
		var->green.offset	= 8;
		var->blue.length	= 8;
		var->blue.offset	= 0;
		break;
	default:;
	}
	return 0;
}
/**************************START*********************************
*
*copy from http://www.armbbs.net/forum.php?mod=viewthread&tid=19629 for bootlogo
*
****************************************************************/

static inline unsigned int chan_to_field(unsigned int chan, struct fb_bitfield *bf)
{
        chan &= 0xffff;
        chan >>= 16 - bf->length;
        return chan << bf->offset;
}
//s5pv210_setcolreg

unsigned long pseudo_palette[16];
static int s5pv210fb_setcolreg(unsigned int regno, unsigned int red,
                             unsigned int green, unsigned int blue,
                             unsigned int transp, struct fb_info *info)
{
        unsigned int val;
       
        if (regno > 16)
                return 1;

        /* 用red,green,blue三原色构造出val */
        val  = chan_to_field(red,        &info->var.red);
        val |= chan_to_field(green, &info->var.green);
        val |= chan_to_field(blue,        &info->var.blue);
       
        //((u32 *)(info->pseudo_palette))[regno] = val;
        pseudo_palette[regno] = val;
        return 0;
}
/***************************END**********************************/


/* s5pv210fb_lcd_enable
 *
 * shutdown the lcd controller
 */
static void s5pv210fb_lcd_enable(struct s5pv210fb_info *fbi, int enable)
{
	struct s5pv210_fb_mach_info *mach_info = fbi->dev->platform_data;
//	struct s3c_fb_platdata *lcd_pdata = fbi->dev->parent->platform_data;
	void __iomem *regs = fbi->io;
	unsigned long flags;

	local_irq_save(flags);

	if (enable)
	{
		mach_info->lcd_reg.shadowcon |= S5PV210_LCD_CHANNEL0_ENABLE;
		mach_info->lcd_reg.vidcon0 |= S5PV210_LCD_ENVID;
		mach_info->lcd_reg.wincon0 |= S5PV210_LCD_WIN0_ENABLE;
	}
	else
	{
		mach_info->lcd_reg.shadowcon &= ~S5PV210_LCD_CHANNEL0_ENABLE;
		mach_info->lcd_reg.vidcon0 &= ~S5PV210_LCD_ENVID;
		mach_info->lcd_reg.wincon0 &= ~S5PV210_LCD_WIN0_ENABLE;
	}
	
	writel(mach_info->lcd_reg.shadowcon,regs + S5PV210_SHADOWCON);
	writel(mach_info->lcd_reg.vidcon0,regs + S5PV210_VIDCON0);
	writel(mach_info->lcd_reg.wincon0,regs + S5PV210_WINCON0);

	local_irq_restore(flags);	
}

/*
 *      s5pv210fb_blank
 *	@blank_mode: the blank mode we want.
 *	@info: frame buffer structure that represents a single frame buffer
 *
 *	Blank the screen if blank_mode != 0, else unblank. Return 0 if
 *	blanking succeeded, != 0 if un-/blanking failed due to e.g. a
 *	video mode which doesn't support it. Implements VESA suspend
 *	and powerdown modes on hardware that supports disabling hsync/vsync:
 *
 *	Returns negative errno on error, or zero on success.
 *
 */
static int s5pv210fb_blank(int blank_mode, struct fb_info *info)
{
	struct s5pv210fb_info *fbi = info->par;
//	void __iomem *tpal_reg = fbi->io;

	dprintk("blank(mode=%d, info=%p)\n", blank_mode, info);

//	tpal_reg = tpal_reg + 

	if (blank_mode == FB_BLANK_POWERDOWN)
		s5pv210fb_lcd_enable(fbi, 0);
	else
		s5pv210fb_lcd_enable(fbi, 1);

	return 0;
}

#ifdef CONFIG_CPU_FREQ

static int s5pv210fb_cpufreq_transition(struct notifier_block *nb,
					unsigned long val, void *data)
{
	struct s5pv210fb_info *info;
	struct fb_info *fbinfo;
	long delta_f;

	info = container_of(nb, struct s5pv210fb_info, freq_transition);
	fbinfo = platform_get_drvdata(to_platform_device(info->dev));

	/* work out change, <0 for speed-up */
	delta_f = info->clk_rate - clk_get_rate(info->clk);

	if ((val == CPUFREQ_POSTCHANGE && delta_f > 0) ||
	    (val == CPUFREQ_PRECHANGE && delta_f < 0)) {
		info->clk_rate = clk_get_rate(info->clk);
		s5pv210fb_activate_var(fbinfo);
	}

	return 0;
}

static inline int s5pv210fb_cpufreq_register(struct s5pv210fb_info *info)
{
	info->freq_transition.notifier_call = s5pv210fb_cpufreq_transition;

	return cpufreq_register_notifier(&info->freq_transition,
					 CPUFREQ_TRANSITION_NOTIFIER);
}

static inline void s5pv210fb_cpufreq_deregister(struct s5pv210fb_info *info)
{
	cpufreq_unregister_notifier(&info->freq_transition,
				    CPUFREQ_TRANSITION_NOTIFIER);
}

#else
static inline int s5pv210fb_cpufreq_register(struct s5pv210fb_info *info)
{
	return 0;
}

static inline void s5pv210fb_cpufreq_deregister(struct s5pv210fb_info *info)
{
}
#endif

#ifdef CONFIG_PM

/* suspend and resume support for the lcd controller */
static int s5pv210fb_suspend(struct platform_device *dev, pm_message_t state)
{
	struct fb_info	   *fbinfo = platform_get_drvdata(dev);
	struct s5pv210fb_info *info = fbinfo->par;

	s5pv210fb_lcd_enable(info, 0);

	/* sleep before disabling the clock, we need to ensure
	 * the LCD DMA engine is not going to get back on the bus
	 * before the clock goes off again (bjd) */

	usleep_range(1000, 1100);
	clk_disable(info->clk);

	return 0;
}

static int s5pv210fb_resume(struct platform_device *dev)
{
	struct fb_info	   *fbinfo = platform_get_drvdata(dev);
	struct s5pv210fb_info *info = fbinfo->par;

	clk_enable(info->clk);
	usleep_range(1000, 1100);

	s5pv210fb_init_registers(fbinfo);

	/* re-activate our display after resume */
	s5pv210fb_activate_var(fbinfo);
	s5pv210fb_blank(FB_BLANK_UNBLANK, fbinfo);

	return 0;
}

#else
#define s5pv210fb_suspend NULL
#define s5pv210fb_resume  NULL
#endif







/*
 *      s3c2410fb_set_par - Alters the hardware state.
 *      @info: frame buffer structure that represents a single frame buffer
 *
 */
static int s5pv210fb_set_par(struct fb_info *info)
{
	struct fb_var_screeninfo *var = &info->var;

	switch (var->bits_per_pixel) {
	case 32:
	case 16:
	case 12:
		info->fix.visual = FB_VISUAL_TRUECOLOR;
		break;
	case 1:
		info->fix.visual = FB_VISUAL_MONO01;
		break;
	default:
		info->fix.visual = FB_VISUAL_PSEUDOCOLOR;
		break;
	}

	info->fix.line_length = (var->xres_virtual * var->bits_per_pixel) / 8;

	/* activate this new configuration */

	s5pv210fb_activate_var(info);
	return 0;
}

static int s5pv210fb_debug_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", debug ? "on" : "off");
}

static int s5pv210fb_debug_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t len)
{
	if (len < 1)
		return -EINVAL;

	if (strnicmp(buf, "on", 2) == 0 ||
	    strnicmp(buf, "1", 1) == 0) {
		debug = 1;
		dev_dbg(dev, "s5pv210fb: Debug On");
	} else if (strnicmp(buf, "off", 3) == 0 ||
		   strnicmp(buf, "0", 1) == 0) {
		debug = 0;
		dev_dbg(dev, "s5pv210fb: Debug Off");
	} else {
		return -EINVAL;
	}

	return len;
}

static DEVICE_ATTR(debug, 0666, s5pv210fb_debug_show, s5pv210fb_debug_store);

static struct fb_ops s5pv210fb_ops = {
	.owner		= THIS_MODULE,
	.fb_check_var	= s5pv210fb_check_var,
	.fb_set_par	= s5pv210fb_set_par,
	.fb_blank	= s5pv210fb_blank,
	.fb_setcolreg	= s5pv210fb_setcolreg,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
};

static const char driver_name[] = "s5pv210-lcd";

static int s5pv210fb_probe(struct platform_device *pdev)
{
	struct s5pv210fb_info *info;
//	struct s3c2410fb_display *display;
	struct fb_info *fbinfo;
//	struct s3c2410fb_mach_info *mach_info;
	struct s3c_fb_platdata *lcd_pdata;
//	struct plat_lcd_data *plat_lcd;
	struct resource *res;
	int ret;
//	int irq;
//	int i;
	int size;

	lcd_pdata = pdev->dev.parent->platform_data;
	if (lcd_pdata == NULL) {
		dev_err(&pdev->dev,
			"no platform data for lcd, cannot attach\n");
		return -EINVAL;
	}

	fbinfo = framebuffer_alloc(sizeof(struct s5pv210fb_info), &pdev->dev);
	if (!fbinfo)
		return -ENOMEM;

	platform_set_drvdata(pdev, fbinfo);

	info = fbinfo->par;
	info->dev = &pdev->dev;


	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "failed to get memory registers\n");
		ret = -ENXIO;
		goto dealloc_fb;
	}

	size = resource_size(res);
	info->mem = request_mem_region(res->start, size, pdev->name);
	if (info->mem == NULL) {
		dev_err(&pdev->dev, "failed to get memory region\n");
		ret = -ENOENT;
		goto dealloc_fb;
	}
	
	dprintk("regs start=	%8lx\n",(long unsigned int)res->start);
	dprintk("regs size =	%8lx\n",(long unsigned int)size);

	info->io = ioremap(res->start, size);
	if (info->io == NULL) {
		dev_err(&pdev->dev, "ioremap() of registers failed\n");
		ret = -ENXIO;
		goto release_mem;
	}

	strcpy(fbinfo->fix.id, driver_name);

	fbinfo->fix.type	    = FB_TYPE_PACKED_PIXELS;
	fbinfo->fix.type_aux	    = 0;
	fbinfo->fix.xpanstep	    = 0;
	fbinfo->fix.ypanstep	    = 0;
	fbinfo->fix.ywrapstep	    = 0;
	fbinfo->fix.accel	    = FB_ACCEL_NONE;

	fbinfo->var.nonstd	    = 0;
	fbinfo->var.activate	    = FB_ACTIVATE_NOW;
	fbinfo->var.accel_flags     = 0;
	fbinfo->var.vmode	    = FB_VMODE_NONINTERLACED;

	fbinfo->fbops		    = &s5pv210fb_ops;
	fbinfo->flags		    = FBINFO_FLAG_DEFAULT;
	fbinfo->pseudo_palette      = pseudo_palette;

/*	ret = request_irq(irq, s5pv210fb_irq, 0, pdev->name, info);
	if (ret) {
		dev_err(&pdev->dev, "cannot get irq %d - err %d\n", irq, ret);
		ret = -EBUSY;
		goto release_regs;
	}*/

	info->clk = clk_get(NULL, "lcd");
	if (IS_ERR(info->clk)) {
		dev_err(&pdev->dev, "failed to get lcd clock source\n");
		ret = PTR_ERR(info->clk);
		goto release_irq;
	}

	clk_enable(info->clk);
	dprintk("got and enabled clock\n");

	usleep_range(1000, 1100);

	info->clk_rate = clk_get_rate(info->clk);
	
	{
		unsigned long smem_len = lcd_pdata->win[0]->xres;
		smem_len *= lcd_pdata->win[0]->yres;
		smem_len *= lcd_pdata->win[0]->max_bpp/8;
		if(fbinfo->fix.smem_len < smem_len)
		{
			fbinfo->fix.smem_len = smem_len;
		}
	}

	/* Initialize video memory */
	ret = s5pv210fb_map_video_memory(fbinfo);
	if (ret) {
		dev_err(&pdev->dev, "Failed to allocate video RAM: %d\n", ret);
		ret = -ENOMEM;
		goto release_clock;
	}

	dprintk("got video memory\n");

	fbinfo->var.xres = lcd_pdata->win[0]->xres;
	fbinfo->var.yres = lcd_pdata->win[0]->yres;
	fbinfo->var.bits_per_pixel = lcd_pdata->win[0]->default_bpp;

	s5pv210fb_init_registers(fbinfo);

	s5pv210fb_check_var(&fbinfo->var, fbinfo);
	
	s5pv210fb_activate_var(fbinfo);
	lcd_clear_screen(fbinfo,0x0000ffff);

	ret = s5pv210fb_cpufreq_register(info);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register cpufreq\n");
		goto free_video_memory;
	}

	ret = register_framebuffer(fbinfo);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register framebuffer device: %d\n",
			ret);
		goto free_cpufreq;
	}

	/* create device files */
	ret = device_create_file(&pdev->dev, &dev_attr_debug);
	if (ret)
		dev_err(&pdev->dev, "failed to add debug attribute\n");

	dev_info(&pdev->dev, "fb%d: %s frame buffer device\n",
		fbinfo->node, fbinfo->fix.id);

	dev_err(&pdev->dev, "s5pv210 fb init successfully\n");
	return 0;

free_cpufreq:
	s5pv210fb_cpufreq_deregister(info);
free_video_memory:
	s5pv210fb_unmap_video_memory(fbinfo);
release_clock:
	clk_disable(info->clk);
	clk_put(info->clk);
release_irq:
//	free_irq(irq, info);
//release_regs:
	iounmap(info->io);
release_mem:
	release_mem_region(res->start, size);
dealloc_fb:
	platform_set_drvdata(pdev, NULL);
	framebuffer_release(fbinfo);
	return ret;
}


/*
 *  Cleanup
 */
static int s5pv210fb_remove(struct platform_device *pdev)
{
	struct fb_info *fbinfo = platform_get_drvdata(pdev);
	struct s5pv210fb_info *info = fbinfo->par;
	int irq;

	unregister_framebuffer(fbinfo);
	s5pv210fb_cpufreq_deregister(info);

	s5pv210fb_lcd_enable(info, 0);
	usleep_range(1000, 1100);

	s5pv210fb_unmap_video_memory(fbinfo);

	if (info->clk) {
		clk_disable(info->clk);
		clk_put(info->clk);
		info->clk = NULL;
	}

	irq = platform_get_irq(pdev, 0);
	free_irq(irq, info);

	iounmap(info->io);

	release_mem_region(info->mem->start, resource_size(info->mem));

	platform_set_drvdata(pdev, NULL);
	framebuffer_release(fbinfo);

	return 0;
}

static struct platform_driver s5pv210_driver = {
	.probe		= s5pv210fb_probe,
	.remove		= s5pv210fb_remove,
	.suspend	= s5pv210fb_suspend,
	.resume		= s5pv210fb_resume,
	.driver		= {
		.name	= "s5pv210-lcd",
		.owner	= THIS_MODULE,
	},
};

int __init s5pv210fb_init(void)
{
	int ret = platform_driver_register(&s5pv210_driver);
	return ret;
}

static void __exit s5pv210fb_cleanup(void)
{
	platform_driver_unregister(&s5pv210_driver);
}

module_init(s5pv210fb_init);
module_exit(s5pv210fb_cleanup);

MODULE_AUTHOR("Arnaud Patard <arnaud.patard@rtp-net.org>");
MODULE_AUTHOR("Ben Dooks <ben-linux@fluff.org>");
MODULE_DESCRIPTION("Framebuffer driver for the s5pv210");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:s5pv210-lcd");
