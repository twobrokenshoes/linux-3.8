#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/string.h>
#include <linux/delay.h>
/*
static const struct i2c_board_info i2c_rom[] __initdata = {
	{ I2C_BOARD_INFO("at24c02",0x50),},
};*/

#define	BUF_SIZE	20
#define	AT24C02_PAGE_SIZE	8

static struct i2c_client *myclient = NULL;

int at24c02_write_bytes(unsigned char reg,const char data[],int count)
{
	int len;
	int total_bytes = count;
	unsigned char buf[AT24C02_PAGE_SIZE + 1];
	
	len = (((reg + AT24C02_PAGE_SIZE)>> 3) << 3) -reg;
	buf[0] = reg;
	if(count <= len)
	{
		len = count;
	}
	memcpy(buf + 1, data, len);
	len = i2c_master_send(myclient, buf, len + 1) - 1;
	msleep(2);
//	printk("reg:%d count:%d len:%d \n", reg, count, len);
	data += len;
	count = count - len;
	while(count)
	{
		reg = reg + len;
		buf[0] = reg;
		len = (count > AT24C02_PAGE_SIZE)?AT24C02_PAGE_SIZE:count;
		memcpy(buf + 1, data , len);
		len = i2c_master_send(myclient, buf, len + 1) - 1;
		msleep(2);
//		printk("reg:%d count:%d len:%d \n", reg, count, len);
		data += len;
		count = count - len;
	}
	printk("%s: %d bytes was write successfully \n", __func__, total_bytes);
	return len;
}

int at24c02_read_bytes(unsigned char reg,char data[],int count)
{
	int len;
	
	data[BUF_SIZE] = 0;
	
	i2c_master_send(myclient, &reg, 1);
	
	len = i2c_master_recv(myclient, data, count);
	
	printk("%s: %d bytes was read successfully \n", __func__, len);
	return len;
}

static ssize_t at24c02_show(struct device* dev,struct device_attribute *attr,char *buf)
{
	unsigned char data[BUF_SIZE + 1];
	at24c02_read_bytes(0x00, data, BUF_SIZE);
	printk(KERN_INFO "read data: %s \n",data);
	return 0;
}
static ssize_t at24c02_store(struct device *dev,struct device_attribute *attr,const char *buf,size_t count)
{
	at24c02_write_bytes(0x00,buf, count);
	return count;
}

static DEVICE_ATTR(24c02, 0644, at24c02_show, at24c02_store);

static int at24c02_probe(struct i2c_client *client,const struct i2c_device_id *id)
{
	int ret = 0;
	printk("%s: come to the i2c probe \n",__func__);
	myclient = client;
	
	ret = sysfs_create_file(&client->dev.kobj,&dev_attr_24c02.attr);
	if(ret < 0)
	{
		printk("%s: create sysfs file fail\n",__func__);
		return ret;
	}
	
	return 0;
}

static int at24c02_remove(struct i2c_client *client)
{
	printk("%s: come to the i2c remove \n",__func__);
	return 0;
}

static struct i2c_device_id i2c_rom_id[] = {
	{"at24c02",0},
	{}
};
MODULE_DEVICE_TABLE(i2c, i2c_rom_id);

static struct of_device_id i2c_rom_match[] = {
	{
		.compatible = "at24c02"	,
		.data = 0,
	},
	{}
};

static struct i2c_driver at24c02_driver = {
	.driver = {
		.name = "at24c02",
		.owner = THIS_MODULE,
		.of_match_table = i2c_rom_match,
	},
	.probe = at24c02_probe,
	.remove = at24c02_remove,
	.id_table = i2c_rom_id,
};

int __init at24c02_init(void)
{
	int ret = 0;
	printk("%s is running \n",__func__);
/*	ret = i2c_register_board_info(0,i2c_rom,1);
	if(ret)
	{
		printk("%s: failed to register i2c board info \n", __func__);
		return ret;
	}*/
	
	ret = i2c_add_driver(&at24c02_driver);
	if(ret)
	{
		printk("%s: failed to register i2c driver \n", __func__);
		return ret;
	}
	return 0;
}

void __exit at24c02_exit(void)
{
	i2c_del_driver(&at24c02_driver);
//	i2c_unregister_board_info();
}

module_init(at24c02_init);
module_exit(at24c02_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("liukangfei");
