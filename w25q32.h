#define W25_ID		0X9F		/* read Manufacturer id */
#define W25_WREN	0X06		/* latch the write enable */
#define W25_WRDI	0X04		/* reset the write enable */
#define W25_RDSR	0X05		/* read the status register */
#define W25_WRSR	0X01		/* write the status register */
#define W25_READ	0X03		/* read byte(s) */
#define W25_WRITE	0X02		/* write bytes / sector */
#define W25_MN_ID	0XEF		/* manufaturer id */


#define W25_SR_nRDY	0X01		/* nRDY = write-in-progress */
#define W25_SR_WEN	0X02		/* write enable(latched */

#define W25_SEC_ERASE	0X20		/* Erase sector */

#define W25_MAXADDRLEN	3		/* 24 bit address, upto 16MBytes */

#define W25_TIMEOUT	25		

#define IO_LIMIT	256		/* bytes */

struct w25_priv{
	char			name[15];
	struct spi_device	*spi;
	struct kobject		*w25_kobj;
	long int		offset;
	unsigned		size;
	unsigned		page_size;
	unsigned		addr_width;
	unsigned		block;
	unsigned		sector;
	unsigned		page;
};
	
