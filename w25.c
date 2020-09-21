#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/atomic.h>
#include <linux/capability.h>
#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/spi/spi.h>

#include "w25q32.h"

struct w25_priv *prv = NULL;

static ssize_t rw_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	u8 temp[260];
	struct spi_transfer t[2];
	struct spi_message m;
	unsigned int ret,status;

	pr_info("%s:invoked\n", __func__);
	memset(temp, '\0', sizeof(temp));

	ret = spi_read(prv->spi, temp, 1);
	if(ret < 0)
		pr_info("read signal is not enabled\n"); 

	temp[0] = (u8)W25_READ;
        temp[1] = prv->offset >> 16;
        temp[2] = prv->offset >> 8;
        temp[3] = prv->offset >> 0;

	memcpy(&temp[4], buf, 256);

        t[0].tx_buf = temp;
	t[0].len = 260;
	t[1].rx_buf = buf; 
        t[1].len = 256;

        spi_message_init(&m);
        spi_message_add_tail(&t[0], &m);
        spi_message_add_tail(&t[1], &m);

        status = spi_sync(prv->spi, &m);
        if(status < 0){
                pr_info("failed to transfer the data\n");
		return status;
        }

	return status;
}


static ssize_t offset_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	unsigned int ret;
	pr_info("%s:invoked\n",__func__);
	
	ret = sprintf(buf,"block:%d sector:%d page:%d\n",prv->block, prv->sector, prv->size);

	return ret;

}

static ssize_t rw_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	u8 temp[260];
	struct spi_transfer t;
	struct spi_message m;
	unsigned int status, ret;
	
	pr_info("%s:invoked\n",__func__);
	memset(temp, '\0', sizeof(temp));
	
	temp[0] = W25_WREN;
	ret = spi_write(prv->spi ,temp ,1);
	if(ret < 0)
		pr_info("write signal is not enabled\n");
	
	temp[0] = (u8) W25_WRITE;
	temp[1] = prv->offset >> 16; 
	temp[2] = prv->offset >> 8;
	temp[3] = prv->offset >> 0;

	memcpy(&temp[4], buf, 256);

        t.tx_buf = temp; 
        t.len = 260;

        spi_message_init(&m);
        spi_message_add_tail(&t, &m);

        status = spi_sync(prv->spi, &m);
        if(status < 0){
                pr_info("failed to transfer the data\n");
        }

	return status;

}

static ssize_t offset_store(struct kobject *kobj,struct kobj_attribute *attr, const char *buf,size_t count)
{
	unsigned int res1,res2,res3;

	pr_info("%s:invoked\n",__func__);
	sscanf(buf, "%x:%x:%x" ,&res1, &res2, &res3);
	
	if(res1>=0 && res1 <= 64)
	{
		prv->block = res1;
		pr_info("block:%d \n", prv->block);
	}

	else{
		pr_info("block is not available\nchoose the blocks from 0 - 64\n");
	}

	if(res2>=0 && res2 <= 16)
	{
		prv->sector = res2;
		pr_info("sector:%d \n", prv->sector);
	}

	else{
		pr_info("sector is not available\nchoose the sectors from 0 - 16\n");
	}

	if(res3>=0 && res3 <= 1024)
	{
		prv->page = res3;
		pr_info("page:%d \n", prv->page);
	}

	else{
		pr_info("page is not available\nchoose the pages from 0 - 1024\n");
	}

	prv->offset = (prv->block * 64) + (prv->sector * 16) + (prv->size * 16);
	return count;
}


static ssize_t er_store(struct kobject *kobj,struct kobj_attribute *attr, const char *buf,size_t count)
{
	struct spi_transfer t;
	struct spi_message m;
	unsigned int tmp, ret, status;
	unsigned char temp[4];

	sscanf(buf, "%x:%x",&prv->block,&prv->sector);
	
	if(prv->block <= 64 && prv->sector <=16){

		tmp = (prv->block*64) + (prv->sector*16);
	}
	else{
		return -ENOMEM;
	}	

	/* before erasing we must do write enable and we need to erase */
	temp[0] = W25_WREN;
	ret = spi_write(prv->spi, temp, 1);	
	if(ret < 0)
		pr_info("write signal is not enabled\n");
	
	temp[0] = (u8)W25_SEC_ERASE;	
	temp[1] = tmp >> 16;
	temp[2] = tmp >> 8;
	temp[3] = tmp >> 0;
	
	t.tx_buf = temp;
	t.len = 4;
	
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	
	status = spi_sync(prv->spi, &m);
	if(status){
		return status;
	}
	return 0;	
}		


static struct kobj_attribute rw_attribute =
__ATTR(rdwr,0664,rw_show,rw_store);
static struct kobj_attribute offset_attribute =
__ATTR(offset,0664,offset_show,offset_store);
static struct kobj_attribute er_attribute =
__ATTR(erase,0664,NULL,er_store);

static struct attribute *attrs[] = {
        &rw_attribute.attr,
        &offset_attribute.attr,
        &er_attribute.attr,
        NULL,/*need to NULL terminate the list of attributes*/
};

static struct attribute_group attr_group ={
        .attrs=attrs,
};


static int w25q32_probe(struct spi_device *spi)
{
	int ret;
	pr_info("device w25q32 probed\n");

	prv = (struct w25_priv *)kzalloc(sizeof(struct w25_priv),GFP_KERNEL);
        if(prv == NULL){
                pr_info("Requested memory not allocated\n");
                return -ENOMEM;
        }
	
	prv->spi = spi;
	
	if(device_property_read_u32(&spi->dev, "size",&prv->size)==0){
	 }
        else{
                dev_err(&spi->dev, "Error: missing \"size\" property\n");
                return -ENODEV;
        }

	if(device_property_read_u32(&spi->dev, "pagesize",&prv->page_size)==0){
	 }
        else{
                dev_err(&spi->dev, "Error: missing \"size\" property\n");
                return -ENODEV;
        }

	if(device_property_read_u32(&spi->dev, "address_width",&prv->addr_width)==0){
	 }
        else{
                dev_err(&spi->dev, "Error: missing \"size\" property\n");
                return -ENODEV;
        }

	 prv->w25_kobj = kobject_create_and_add("w25q32_flash", NULL /*kernel_kobj */);
        if(!prv->w25_kobj)
                return -ENOMEM;
        /*create the files associated with this kobject */
        ret = sysfs_create_group(prv->w25_kobj, &attr_group);
        if(ret)
                kobject_put(prv->w25_kobj);

	 pr_info("sysfs entry created for w25q32 device\n");
         return ret;

}

static int w25q32_remove(struct spi_device *spi)
{

	pr_info("%s:invoked\n",__func__);
	
	kfree(prv);
	kobject_put(prv->w25_kobj);
	return 0;
}


static const struct spi_device_id w25q32_id[] = {
	{ "w25q32", 0},
	{}
};

MODULE_DEVICE_TABLE(spi, w25q32_id);

static struct spi_driver w25q32_driver = {
	.driver = {
		.name = "winbond,w25q32",
	},
	.probe = w25q32_probe,
	.remove = w25q32_remove,
	//.id_table = w25q32_id_table,
};
module_spi_driver(w25q32_driver);


MODULE_DESCRIPTION("Driver for w25q32");
MODULE_AUTHOR("siree.org");
MODULE_LICENSE("GPL");





















