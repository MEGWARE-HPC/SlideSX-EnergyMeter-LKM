#include "slidesx-energymeter.h"


/* =========================
 *         DECLARATIONS
 * ========================= */

static int energymeter_probe(struct usb_interface *interface, const struct usb_device_id *id);
static void energymeter_disconnect(struct usb_interface *interface);
static ssize_t energymeter_get_reading(struct device *dev, struct device_attribute *da, char *buf);


/* =========================
 *	 DATASTRUCTURES
 * ========================= */


/* define supported devices */
static struct usb_device_id devices_table[] = {
	{ USB_DEVICE(USB_DEVICE_VENDOR_ID, USB_DEVICE_PRODUCT_ID) },
	{}
};
MODULE_DEVICE_TABLE (usb, devices_table);

static struct usb_driver energymeter_usb = {
	.name       = "slidesx-em",
	.id_table   = devices_table,
	.probe      = energymeter_probe,
	.disconnect = energymeter_disconnect,
};

static dev_t energymeter_dev;

static struct cdev energymeter_cdev;

static struct class *energymeter_class;

static struct file_operations energymeter_fops = 
{
	.owner   = THIS_MODULE,
	.open    = NULL,
	.release = NULL,
	.read    = NULL,
	.write   = NULL
};

static struct usb_class_driver energymeter_usb_class = {
	.name       = "usb/slidesx-em%d",
	.fops       = &energymeter_fops,
	.minor_base = USB_MINOR_BASE,
};

/* sensors */
static SENSOR_DEVICE_ATTR(power_mw,   S_IRUGO, energymeter_get_reading, NULL, _GETPOWER);
static SENSOR_DEVICE_ATTR(current_ma, S_IRUGO, energymeter_get_reading, NULL, _GETCURRENT);
static SENSOR_DEVICE_ATTR(voltage_mv, S_IRUGO, energymeter_get_reading, NULL, _GETVOLTAGE);
static SENSOR_DEVICE_ATTR(energy_j,   S_IRUGO, energymeter_get_reading, NULL, _GETENERGY);


static struct attribute *energymeter_attrs[] = {
	&sensor_dev_attr_power_mw.dev_attr.attr,
	&sensor_dev_attr_current_ma.dev_attr.attr,
	&sensor_dev_attr_voltage_mv.dev_attr.attr,
	&sensor_dev_attr_energy_j.dev_attr.attr,
	NULL,
};
ATTRIBUTE_GROUPS(energymeter);

/* Spinlock for read operation */
static DEFINE_SPINLOCK(read_lock);


/* =========================
 *         FUNCTIONS
 * ========================= */

void _free_usb_energymeter_handle(struct kref *kref)
{
	struct usb_energymeter_handle *devctx = to_energymeter_dev(kref);
	if (devctx)
        {
                usb_put_dev(devctx->usb_dev);
		if (devctx->bulk_out_buff)
		{
			kfree(devctx->bulk_out_buff);
		}
		if (devctx->bulk_in_buff)
		{
			kfree(devctx->bulk_in_buff);
		}
                kfree(devctx);
		devctx = NULL;
        }

}

static ssize_t energymeter_get_reading(struct device *dev, struct device_attribute *da, char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct usb_energymeter_handle *devctx = dev_get_drvdata(dev);
	int transmitted;
	uint64_t result;
	unsigned char *ptr = (unsigned char *) &result;

	if (devctx == NULL) {
		printk(KERN_ERR "SlideSX Energymeter devctx = NULL!\n");
		return 0;
	}

	/* prevent concurrency problems */
	spin_lock(&read_lock);

	/* send the command to the energymeter      */
	/* attr->index contains the desired command */
	memset(devctx->bulk_out_buff, 0, devctx->bulk_out_buff_size);
	devctx->bulk_out_buff[0] = attr->index;

	if (usb_bulk_msg(devctx->usb_dev, usb_sndbulkpipe(devctx->usb_dev, devctx->endpnt_bulk_out_addr), devctx->bulk_out_buff, 1, &transmitted, 0) != 0)
	{
		spin_unlock(&read_lock);
		printk(KERN_ERR "SlideSX Energymeter could not send command to usb device\n");
		return 0;
	}	
	if (transmitted == 1)
	{
		/* command send - receive now */
		memset(devctx->bulk_in_buff, 0, devctx->bulk_in_buff_size);
		if (usb_bulk_msg(devctx->usb_dev, usb_rcvbulkpipe(devctx->usb_dev, devctx->endpnt_bulk_in_addr), devctx->bulk_in_buff, devctx->bulk_in_buff_size, &transmitted, 0) != 0)
		{
			spin_unlock(&read_lock);
			printk(KERN_ERR "SlideSX Energymeter could not receive data from usb device\n");
			return 0;
		}

		/* received at least 2 bytes? */
		if (transmitted >= 2)
		{
			if ((devctx->bulk_in_buff[0] == attr->index) &&	(devctx->bulk_in_buff[1] == _OK))
			{
				ptr[0] = devctx->bulk_in_buff[2];
				ptr[1] = devctx->bulk_in_buff[3];
				ptr[2] = devctx->bulk_in_buff[4];
				ptr[3] = devctx->bulk_in_buff[5];
				ptr[4] = devctx->bulk_in_buff[6];
				ptr[5] = devctx->bulk_in_buff[7];
				ptr[6] = devctx->bulk_in_buff[8];
				ptr[7] = devctx->bulk_in_buff[9];
				spin_unlock(&read_lock);
				return snprintf(buf, PAGE_SIZE, "%llu\n",  result);
			} else
			{
				spin_unlock(&read_lock);
				printk(KERN_ERR "SlideSX Energymeter received a message indicating an error\n");
				return 0;
			}
		} else
		{
			spin_unlock(&read_lock);
			printk(KERN_ERR "SlideSX Energymeter received a wrong message\n");
			return 0;
		}
	} else
	{
		spin_unlock(&read_lock);
		return 0;
	}
}

static int energymeter_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
	struct usb_energymeter_handle *devctx = NULL;
	int retval = -ENOMEM;
	int i;
	struct usb_host_interface *host_if;
	struct usb_endpoint_descriptor *endpoint;

	/* allocate data structures */
	devctx = kzalloc(sizeof(struct usb_energymeter_handle), GFP_KERNEL);
	if (!devctx)
	{
		printk(KERN_ERR "SlideSX Energymeter driver runs out of memory\n");
		return -ENOMEM;
	}

	kref_init(&devctx->kref);

	/* get / set the device and interface structure */
	devctx->usb_dev = usb_get_dev(interface_to_usbdev(interface));
	devctx->usb_iface = interface;

	/* change alternative setting of the interface */
	if ((retval = usb_set_interface(devctx->usb_dev, USB_INTERFACE, USB_ALT_SETTING)) < 0)
	{
		printk(KERN_ERR "SlideSX Energymeter could not change alternative settings\n");
		goto ERROR;
	}

	/* reset for possible future errors */
	retval = -ENOMEM;

	/* get our endpoints - we need one IN and one OUT in bulk mode (use the first found endpoints) */
	host_if = interface->cur_altsetting;

	for (i = 0; i < host_if->desc.bNumEndpoints; ++i)
	{
		endpoint = &host_if->endpoint[i].desc;
		
		/* BULK OUT? */
		if ( 	(!devctx->endpnt_bulk_out_addr) &&
			!(endpoint->bEndpointAddress & USB_DIR_IN) &&
			((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_BULK) )
		{
			devctx->endpnt_bulk_out_addr = endpoint->bEndpointAddress;
			devctx->bulk_out_buff_size = endpoint->wMaxPacketSize;
			devctx->bulk_out_buff = kzalloc(devctx->bulk_out_buff_size, GFP_KERNEL);
			if (!devctx->bulk_out_buff)
			{
				printk(KERN_ERR "SlideSX Energymeter failed to allocate buffer for bulk out endpoint\n");
				goto ERROR;
			}
		}

		/* BULK IN? */
		if (	(!devctx->endpnt_bulk_in_addr) &&
			(endpoint->bEndpointAddress & USB_DIR_IN) &&
			((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_BULK) )
		{
			devctx->endpnt_bulk_in_addr = endpoint->bEndpointAddress;
			devctx->bulk_in_buff_size = endpoint->wMaxPacketSize;
			devctx->bulk_in_buff = kzalloc(devctx->bulk_in_buff_size, GFP_KERNEL);
			if (!devctx->bulk_in_buff)
			{
				printk(KERN_ERR "SlideSX Energymeter failed to allocate buffer for bulk in endpoint\n");
				goto ERROR;
			}
		}
	}

	/* bi-directional communication possible? */
	if ( !devctx->endpnt_bulk_out_addr || !devctx->endpnt_bulk_in_addr )
	{
		printk(KERN_ERR "SlideSX Energymeter could not found two bulk endpoints\n");
		goto ERROR;
	}

	/* usb driver needs to retrieve the local data structure later in lifecycle */
	usb_set_intfdata(interface, devctx);

	/* register device now since it is ready for operation */
	retval = usb_register_dev(interface, &energymeter_usb_class);
	if (retval)
	{
		printk(KERN_ERR "SlideSX Energymeter could not get a minor number\n");
		usb_set_intfdata(interface, NULL);
		goto ERROR;
	}

	/* register with hwmon to create sysfs files */
	devctx->hwmon_dev = devm_hwmon_device_register_with_groups(&devctx->usb_dev->dev, "sem", devctx, energymeter_groups);
	if (PTR_ERR_OR_ZERO(devctx->hwmon_dev))
	{
		printk(KERN_ERR "SlideSX Energymeter could not register the hwmon device\n");
		usb_set_intfdata(interface, NULL);
		retval = PTR_ERR_OR_ZERO(devctx->hwmon_dev);
		goto ERROR;
	}
	
	/* register as platform device and reference our local data structure in there */
	devctx->pdevice = platform_device_register_simple("sem", devctx->usb_dev->portnum, NULL, 0);
	if (devctx->pdevice == NULL)
	{
		printk(KERN_ERR "SlideSX Energymeter could not allocate a platform device\n");
		usb_set_intfdata(interface, NULL);
		retval = -1;
		goto ERROR;
	}
	dev_set_drvdata(&devctx->pdevice->dev, devctx);

	/* create the hwmon sensor files in /sys/devices/platform/... */
	retval = device_create_file(&devctx->pdevice->dev, &sensor_dev_attr_power_mw.dev_attr);
	if (retval)
	{
		printk(KERN_ERR "SlideSX Energymeter could not create new file %s\n", sensor_dev_attr_power_mw.dev_attr.attr.name);
		usb_set_intfdata(interface, NULL);
		goto ERROR;
	}
	
	retval = device_create_file(&devctx->pdevice->dev, &sensor_dev_attr_energy_j.dev_attr);
	if (retval)
	{
		printk(KERN_ERR "SlideSX Energymeter could not create new file %s\n", sensor_dev_attr_energy_j.dev_attr.attr.name);
		usb_set_intfdata(interface, NULL);
		goto ERROR;
	}

	retval = device_create_file(&devctx->pdevice->dev, &sensor_dev_attr_voltage_mv.dev_attr);
	if (retval)
	{
		printk(KERN_ERR "SlideSX Energymeter could not create new file %s\n", sensor_dev_attr_voltage_mv.dev_attr.attr.name);
		usb_set_intfdata(interface, NULL);
		goto ERROR;
	}

	retval = device_create_file(&devctx->pdevice->dev, &sensor_dev_attr_current_ma.dev_attr);
	if (retval)
	{
		printk(KERN_ERR "SlideSX Energymeter could not create new file %s\n", sensor_dev_attr_current_ma.dev_attr.attr.name);
		usb_set_intfdata(interface, NULL);
		goto ERROR;
	}

	return 0;

ERROR:
	kref_put(&devctx->kref, _free_usb_energymeter_handle);
	return retval;
}

static void energymeter_disconnect(struct usb_interface *interface)
{
	struct usb_energymeter_handle *devctx = usb_get_intfdata(interface);

	/* unregister platform driver */
	if (devctx->pdevice != NULL)
	{
		platform_device_unregister(devctx->pdevice);
	}

	/* unregister hwmon device */
	if (devctx->hwmon_dev != NULL)
	{
		device_unregister(devctx->hwmon_dev);
	}

	/* reset the data structure */	
	usb_set_intfdata(interface, NULL);

	/* give back minor number */
	usb_deregister_dev(interface, &energymeter_usb_class);

	printk(KERN_ALERT "SlideSX Energymeter has been disconnect from the host\n");
	kref_put(&devctx->kref, _free_usb_energymeter_handle);
}

static int energymeter_init(void)
{
	int retval;
	struct device *err_device;

	printk(KERN_ALERT "MEGWARE SlideSX Energymeter kernel module loaded\n");


	/*  create the character device   */
	/* allocate major + minor numbers */
	if ((retval = alloc_chrdev_region(&energymeter_dev, ENERGYMETER_FIRST_MINOR , ENERGYMETER_MINOR_COUNT, "slidesx-em")) < 0)
	{
		printk(KERN_ERR "SlideSX Energymeter failed to get major and minor numbers\n");
		return retval;
	}

	/* register character device functions and add it to the system */
	cdev_init(&energymeter_cdev, &energymeter_fops);

	if ((retval = cdev_add(&energymeter_cdev, energymeter_dev, ENERGYMETER_MINOR_COUNT)) < 0)
	{
		printk(KERN_ERR "SlideSX Energymeter failed to register character device\n");
		unregister_chrdev_region(energymeter_dev, ENERGYMETER_MINOR_COUNT);
		return retval;
	}

	/* create files in /dev and sysfs */
	if (IS_ERR(energymeter_class = class_create(THIS_MODULE, "megware")))
	{
		printk(KERN_ERR "SlideSX Energymeter failed to create class for character device\n");
		cdev_del(&energymeter_cdev);
		unregister_chrdev_region(energymeter_dev, ENERGYMETER_MINOR_COUNT);
		return PTR_ERR(energymeter_class);
	}

	if (IS_ERR(err_device = device_create(energymeter_class, NULL, energymeter_dev, NULL, "slidesx-em")))
	{
		printk(KERN_ERR "SlideSX Energymeter failed to create device\n");
		class_destroy(energymeter_class);
		cdev_del(&energymeter_cdev);
		unregister_chrdev_region(energymeter_dev, ENERGYMETER_MINOR_COUNT);
		return PTR_ERR(err_device);
	}	


	/*       USB functionality     */
	/* register with USB subsystem */
	if ((retval = usb_register(&energymeter_usb)) < 0)
	{
		printk(KERN_ERR "SlideSX Energymeter USB registration failed");
		device_destroy(energymeter_class, energymeter_dev);
		class_destroy(energymeter_class);
		cdev_del(&energymeter_cdev);
		unregister_chrdev_region(energymeter_dev, ENERGYMETER_MINOR_COUNT);
		return retval;
	}

	return 0;
}

static void energymeter_exit(void)
{
	printk(KERN_ALERT "MEGWARE SlideSX Energymeter kernel module unloaded\n");
	
	/* deregister from USB subsystem */
	usb_deregister(&energymeter_usb);

	/* destroy character device */
	device_destroy(energymeter_class, energymeter_dev);
	class_destroy(energymeter_class);
	cdev_del(&energymeter_cdev);
	unregister_chrdev_region(energymeter_dev, ENERGYMETER_MINOR_COUNT);
}

