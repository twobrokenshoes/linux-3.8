/*
 * SAMSUNG EXYNOS USB HOST OHCI Controller
 *
 * Copyright (C) 2011 Samsung Electronics Co.Ltd
 * Author: Jingoo Han <jg1.han@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/clk.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/platform_data/usb-s5pv210.h>
#include <plat/usb-phy.h>

struct s5pv210_ohci_hcd {
	struct device *dev;
	struct usb_hcd *hcd;
	struct clk *clk;
};

static int ohci_s5pv210_reset(struct usb_hcd *hcd)
{
	return ohci_init(hcd_to_ohci(hcd));
}

static int ohci_s5pv210_start(struct usb_hcd *hcd)
{
	struct ohci_hcd *ohci = hcd_to_ohci(hcd);
	int ret;

	ohci_dbg(ohci, "ohci_s5pv210_start, ohci:%p", ohci);
//	printk("ohci_s5pv210_start, ohci:%p \n", ohci);

	ret = ohci_run(ohci);
	if (ret < 0) {
		dev_err(hcd->self.controller, "can't start %s\n",
			hcd->self.bus_name);
		ohci_stop(hcd);
		return ret;
	}

	return 0;
}

static const struct hc_driver s5pv210_ohci_hc_driver = {
	.description		= hcd_name,
	.product_desc		= "EXYNOS OHCI Host Controller",
	.hcd_priv_size		= sizeof(struct ohci_hcd),

	.irq			= ohci_irq,
	.flags			= HCD_MEMORY|HCD_USB11,

	.reset			= ohci_s5pv210_reset,
	.start			= ohci_s5pv210_start,
	.stop			= ohci_stop,
	.shutdown		= ohci_shutdown,

	.get_frame_number	= ohci_get_frame,

	.urb_enqueue		= ohci_urb_enqueue,
	.urb_dequeue		= ohci_urb_dequeue,
	.endpoint_disable	= ohci_endpoint_disable,

	.hub_status_data	= ohci_hub_status_data,
	.hub_control		= ohci_hub_control,
#ifdef	CONFIG_PM
	.bus_suspend		= ohci_bus_suspend,
	.bus_resume		= ohci_bus_resume,
#endif
	.start_port_reset	= ohci_start_port_reset,
};

static u64 ohci_s5pv210_dma_mask = DMA_BIT_MASK(32);

static int s5pv210_ohci_probe(struct platform_device *pdev)
{
	struct s5pv210_ohci_platdata *pdata;
	struct s5pv210_ohci_hcd *s5pv210_ohci;
	struct usb_hcd *hcd;
	struct ohci_hcd *ohci;
	struct resource *res;
	int irq;
	int err;

	pdata = pdev->dev.platform_data;
	if (!pdata) {
		dev_err(&pdev->dev, "No platform data defined\n");
		return -EINVAL;
	}

	/*
	 * Right now device-tree probed devices don't get dma_mask set.
	 * Since shared usb code relies on it, set it here for now.
	 * Once we move to full device tree support this will vanish off.
	 */
	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &ohci_s5pv210_dma_mask;
	if (!pdev->dev.coherent_dma_mask)
		pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);

	s5pv210_ohci = devm_kzalloc(&pdev->dev, sizeof(struct s5pv210_ohci_hcd),
					GFP_KERNEL);
	if (!s5pv210_ohci)
		return -ENOMEM;

	s5pv210_ohci->dev = &pdev->dev;

	hcd = usb_create_hcd(&s5pv210_ohci_hc_driver, &pdev->dev,
					dev_name(&pdev->dev));
	if (!hcd) {
		dev_err(&pdev->dev, "Unable to create HCD\n");
		return -ENOMEM;
	}

	s5pv210_ohci->hcd = hcd;
	s5pv210_ohci->clk = devm_clk_get(&pdev->dev, "usb-host");

	if (IS_ERR(s5pv210_ohci->clk)) {
		dev_err(&pdev->dev, "Failed to get usbhost clock\n");
		err = PTR_ERR(s5pv210_ohci->clk);
		goto fail_clk;
	}

	err = clk_prepare_enable(s5pv210_ohci->clk);
	if (err)
		goto fail_clk;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Failed to get I/O memory\n");
		err = -ENXIO;
		goto fail_io;
	}

	hcd->rsrc_start = res->start;
	hcd->rsrc_len = resource_size(res);
	hcd->regs = devm_ioremap(&pdev->dev, res->start, hcd->rsrc_len);
	if (!hcd->regs) {
		dev_err(&pdev->dev, "Failed to remap I/O memory\n");
		err = -ENOMEM;
		goto fail_io;
	}

	irq = platform_get_irq(pdev, 0);
	if (!irq) {
		dev_err(&pdev->dev, "Failed to get IRQ\n");
		err = -ENODEV;
		goto fail_io;
	}

	if (pdata->phy_init)
		pdata->phy_init(pdev, S5P_USB_PHY_HOST);

	ohci = hcd_to_ohci(hcd);
	ohci_hcd_init(ohci);

	err = usb_add_hcd(hcd, irq, IRQF_SHARED);
	if (err) {
		dev_err(&pdev->dev, "Failed to add USB HCD\n");
		goto fail_io;
	}

	platform_set_drvdata(pdev, s5pv210_ohci);

	return 0;

fail_io:
	clk_disable_unprepare(s5pv210_ohci->clk);
fail_clk:
	usb_put_hcd(hcd);
	return err;
}

static int s5pv210_ohci_remove(struct platform_device *pdev)
{
	struct s5pv210_ohci_platdata *pdata = pdev->dev.platform_data;
	struct s5pv210_ohci_hcd *s5pv210_ohci = platform_get_drvdata(pdev);
	struct usb_hcd *hcd = s5pv210_ohci->hcd;

	usb_remove_hcd(hcd);

	if (pdata && pdata->phy_exit)
		pdata->phy_exit(pdev, S5P_USB_PHY_HOST);

	clk_disable_unprepare(s5pv210_ohci->clk);

	usb_put_hcd(hcd);

	return 0;
}

static void s5pv210_ohci_shutdown(struct platform_device *pdev)
{
	struct s5pv210_ohci_hcd *s5pv210_ohci = platform_get_drvdata(pdev);
	struct usb_hcd *hcd = s5pv210_ohci->hcd;

	if (hcd->driver->shutdown)
		hcd->driver->shutdown(hcd);
}

#ifdef CONFIG_PM
static int s5pv210_ohci_suspend(struct device *dev)
{
	struct s5pv210_ohci_hcd *s5pv210_ohci = dev_get_drvdata(dev);
	struct usb_hcd *hcd = s5pv210_ohci->hcd;
	struct ohci_hcd *ohci = hcd_to_ohci(hcd);
	struct platform_device *pdev = to_platform_device(dev);
	struct s5pv210_ohci_platdata *pdata = pdev->dev.platform_data;
	unsigned long flags;
	int rc = 0;

	/*
	 * Root hub was already suspended. Disable irq emission and
	 * mark HW unaccessible, bail out if RH has been resumed. Use
	 * the spinlock to properly synchronize with possible pending
	 * RH suspend or resume activity.
	 */
	spin_lock_irqsave(&ohci->lock, flags);
	if (ohci->rh_state != OHCI_RH_SUSPENDED &&
			ohci->rh_state != OHCI_RH_HALTED) {
		rc = -EINVAL;
		goto fail;
	}

	clear_bit(HCD_FLAG_HW_ACCESSIBLE, &hcd->flags);

	if (pdata && pdata->phy_exit)
		pdata->phy_exit(pdev, S5P_USB_PHY_HOST);

	clk_disable_unprepare(s5pv210_ohci->clk);

fail:
	spin_unlock_irqrestore(&ohci->lock, flags);

	return rc;
}

static int s5pv210_ohci_resume(struct device *dev)
{
	struct s5pv210_ohci_hcd *s5pv210_ohci = dev_get_drvdata(dev);
	struct usb_hcd *hcd = s5pv210_ohci->hcd;
	struct platform_device *pdev = to_platform_device(dev);
	struct s5pv210_ohci_platdata *pdata = pdev->dev.platform_data;

	clk_prepare_enable(s5pv210_ohci->clk);

	if (pdata && pdata->phy_init)
		pdata->phy_init(pdev, S5P_USB_PHY_HOST);

	ohci_resume(hcd, false);

	return 0;
}
#else
#define s5pv210_ohci_suspend	NULL
#define s5pv210_ohci_resume	NULL
#endif

static const struct dev_pm_ops s5pv210_ohci_pm_ops = {
	.suspend	= s5pv210_ohci_suspend,
	.resume		= s5pv210_ohci_resume,
};

#ifdef CONFIG_OF
static const struct of_device_id s5pv210_ohci_match[] = {
	{ .compatible = "samsung,s5pv210-ohci" },
	{},
};
MODULE_DEVICE_TABLE(of, s5pv210_ohci_match);
#endif

static struct platform_driver s5pv210_ohci_driver = {
	.probe		= s5pv210_ohci_probe,
	.remove		= s5pv210_ohci_remove,
	.shutdown	= s5pv210_ohci_shutdown,
	.driver = {
		.name	= "s5pv210-ohci",
		.owner	= THIS_MODULE,
		.pm	= &s5pv210_ohci_pm_ops,
		.of_match_table	= of_match_ptr(s5pv210_ohci_match),
	}
};

MODULE_ALIAS("platform:s5pv210-ohci");
MODULE_AUTHOR("Jingoo Han <jg1.han@samsung.com>");
