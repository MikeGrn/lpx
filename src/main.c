#include <stdio.h>
#include <libusb-1.0/libusb.h>
#include <string.h>

#if defined(_MSC_VER) && (_MSC_VER < 1900)
#define snprintf _snprintf
#endif

int verbose = 0;

static void print_endpoint_comp(const struct libusb_ss_endpoint_companion_descriptor *ep_comp)
{
	printf("      USB 3.0 Endpoint Companion:\n");
	printf("        bMaxBurst:        %d\n", ep_comp->bMaxBurst);
	printf("        bmAttributes:     0x%02x\n", ep_comp->bmAttributes);
	printf("        wBytesPerInterval: %d\n", ep_comp->wBytesPerInterval);
}

static void print_endpoint(const struct libusb_endpoint_descriptor *endpoint)
{
	int i, ret;

	printf("      Endpoint:\n");
	printf("        bEndpointAddress: %02xh\n", endpoint->bEndpointAddress);
	printf("        bmAttributes:     %02xh\n", endpoint->bmAttributes);
	printf("        wMaxPacketSize:   %d\n", endpoint->wMaxPacketSize);
	printf("        bInterval:        %d\n", endpoint->bInterval);
	printf("        bRefresh:         %d\n", endpoint->bRefresh);
	printf("        bSynchAddress:    %d\n", endpoint->bSynchAddress);

	for (i = 0; i < endpoint->extra_length;) {
		if (LIBUSB_DT_SS_ENDPOINT_COMPANION == endpoint->extra[i + 1]) {
			struct libusb_ss_endpoint_companion_descriptor *ep_comp;

			ret = libusb_get_ss_endpoint_companion_descriptor(NULL, endpoint, &ep_comp);
			if (LIBUSB_SUCCESS != ret) {
				continue;
			}

			print_endpoint_comp(ep_comp);

			libusb_free_ss_endpoint_companion_descriptor(ep_comp);
		}

		i += endpoint->extra[i];
	}
}

static void print_altsetting(const struct libusb_interface_descriptor *interface)
{
	uint8_t i;

	printf("    Interface:\n");
	printf("      bInterfaceNumber:   %d\n", interface->bInterfaceNumber);
	printf("      bAlternateSetting:  %d\n", interface->bAlternateSetting);
	printf("      bNumEndpoints:      %d\n", interface->bNumEndpoints);
	printf("      bInterfaceClass:    %d\n", interface->bInterfaceClass);
	printf("      bInterfaceSubClass: %d\n", interface->bInterfaceSubClass);
	printf("      bInterfaceProtocol: %d\n", interface->bInterfaceProtocol);
	printf("      iInterface:         %d\n", interface->iInterface);

	for (i = 0; i < interface->bNumEndpoints; i++)
		print_endpoint(&interface->endpoint[i]);
}

static void print_2_0_ext_cap(struct libusb_usb_2_0_extension_descriptor *usb_2_0_ext_cap)
{
	printf("    USB 2.0 Extension Capabilities:\n");
	printf("      bDevCapabilityType: %d\n", usb_2_0_ext_cap->bDevCapabilityType);
	printf("      bmAttributes:       0x%x\n", usb_2_0_ext_cap->bmAttributes);
}

static void print_ss_usb_cap(struct libusb_ss_usb_device_capability_descriptor *ss_usb_cap)
{
	printf("    USB 3.0 Capabilities:\n");
	printf("      bDevCapabilityType: %d\n", ss_usb_cap->bDevCapabilityType);
	printf("      bmAttributes:       0x%x\n", ss_usb_cap->bmAttributes);
	printf("      wSpeedSupported:    0x%x\n", ss_usb_cap->wSpeedSupported);
	printf("      bFunctionalitySupport: %d\n", ss_usb_cap->bFunctionalitySupport);
	printf("      bU1devExitLat:      %d\n", ss_usb_cap->bU1DevExitLat);
	printf("      bU2devExitLat:      %d\n", ss_usb_cap->bU2DevExitLat);
}

static void print_bos(libusb_device_handle *handle)
{
	struct libusb_bos_descriptor *bos;
	int ret;

	ret = libusb_get_bos_descriptor(handle, &bos);
	if (0 > ret) {
		return;
	}

	printf("  Binary Object Store (BOS):\n");
	printf("    wTotalLength:       %d\n", bos->wTotalLength);
	printf("    bNumDeviceCaps:     %d\n", bos->bNumDeviceCaps);

	if(bos->dev_capability[0]->bDevCapabilityType == LIBUSB_BT_USB_2_0_EXTENSION) {

		struct libusb_usb_2_0_extension_descriptor *usb_2_0_extension;
	        ret =  libusb_get_usb_2_0_extension_descriptor(NULL, bos->dev_capability[0],&usb_2_0_extension);
	        if (0 > ret) {
		        return;
	        }

                print_2_0_ext_cap(usb_2_0_extension);
                libusb_free_usb_2_0_extension_descriptor(usb_2_0_extension);
        }

	if(bos->dev_capability[0]->bDevCapabilityType == LIBUSB_BT_SS_USB_DEVICE_CAPABILITY) {

	        struct libusb_ss_usb_device_capability_descriptor *dev_cap;
		ret = libusb_get_ss_usb_device_capability_descriptor(NULL, bos->dev_capability[0],&dev_cap);
	        if (0 > ret) {
		        return;
	        }

	        print_ss_usb_cap(dev_cap);
	        libusb_free_ss_usb_device_capability_descriptor(dev_cap);
        }

	libusb_free_bos_descriptor(bos);
}

static void print_interface(const struct libusb_interface *interface)
{
	int i;

	for (i = 0; i < interface->num_altsetting; i++)
		print_altsetting(&interface->altsetting[i]);
}

static void print_configuration(struct libusb_config_descriptor *config)
{
	uint8_t i;

	printf("  Configuration:\n");
	printf("    wTotalLength:         %d\n", config->wTotalLength);
	printf("    bNumInterfaces:       %d\n", config->bNumInterfaces);
	printf("    bConfigurationValue:  %d\n", config->bConfigurationValue);
	printf("    iConfiguration:       %d\n", config->iConfiguration);
	printf("    bmAttributes:         %02xh\n", config->bmAttributes);
	printf("    MaxPower:             %d\n", config->MaxPower);

	for (i = 0; i < config->bNumInterfaces; i++)
		print_interface(&config->interface[i]);
}

static int print_device(libusb_device *dev) {
	struct libusb_device_descriptor desc;
	libusb_device_handle *handle = NULL;
	char description[256];
	char string[256];
	int ret;
	uint8_t i;

	ret = libusb_get_device_descriptor(dev, &desc);
	if (ret < 0) {
		fprintf(stderr, "failed to get device descriptor");
		return -1;
	}

	ret = libusb_open(dev, &handle);
	if (LIBUSB_SUCCESS == ret) {
		if (desc.iManufacturer) {
			ret = libusb_get_string_descriptor_ascii(handle, desc.iManufacturer, string, sizeof(string));
			if (ret > 0)
				snprintf(description, sizeof(description), "%s - ", string);
			else
				snprintf(description, sizeof(description), "%04X - ",
				desc.idVendor);
		}
		else
			snprintf(description, sizeof(description), "%04X - ",
			desc.idVendor);

		if (desc.iProduct) {
			ret = libusb_get_string_descriptor_ascii(handle, desc.iProduct, string, sizeof(string));
			if (ret > 0)
				snprintf(description + strlen(description), sizeof(description) -
				strlen(description), "%s", string);
			else
				snprintf(description + strlen(description), sizeof(description) -
				strlen(description), "%04X", desc.idProduct);
		}
		else
			snprintf(description + strlen(description), sizeof(description) -
			strlen(description), "%04X", desc.idProduct);
	}
	else {
		snprintf(description, sizeof(description), "%04X - %04X",
			desc.idVendor, desc.idProduct);
	}

	printf("  Dev (bus %d, device %d): %s\n", 
		libusb_get_bus_number(dev), libusb_get_device_address(dev), description);

	if (handle) {
		if (desc.iSerialNumber) {
			ret = libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber, string, sizeof(string));
			if (ret > 0)
				printf("  - Serial Number: %s\n", string);
		}
	}

	for (i = 0; i < desc.bNumConfigurations; i++) {
		struct libusb_config_descriptor *config;
		ret = libusb_get_config_descriptor(dev, i, &config);
		if (LIBUSB_SUCCESS != ret) {
			printf("  Couldn't retrieve descriptors\n");
			continue;
		}

		print_configuration(config);

		libusb_free_config_descriptor(config);
	}

	if (handle && desc.bcdUSB >= 0x0201) {
		print_bos(handle);
	}

	if (handle)
		libusb_close(handle);

	return 0;
}

int main() {
	libusb_device **devs; //pointer to pointer of device, used to retrieve a list of devices
	libusb_context *ctx = NULL; //a libusb session
	int r; //for return values
	ssize_t cnt; //holding number of devices in list
	r = libusb_init(&ctx); //initialize a library session
	if(r < 0) {
		printf("Init Error %d\n", r);
		return 1;
	}
	libusb_set_debug(ctx, 3); //set verbosity level to 3, as suggested in the documentation
	cnt = libusb_get_device_list(ctx, &devs); //get the list of devices
	if(cnt < 0) {
		printf("Get device error\n");
	}
	printf("%zi devices in list\n", cnt);
	ssize_t i; //for iterating through the list
	//print_device(devs[4]);

	struct libusb_device_descriptor desc;
	libusb_device *dev = NULL;
	for(i = 0; i < cnt; i++) {
	    int ret = libusb_get_device_descriptor(devs[i], &desc);
	    if (ret < 0) {
			fprintf(stderr, "failed to get device descriptor");
			continue;
	    }
	    if (desc.idVendor == 0x0547 && desc.idProduct == 0x101F) {
	    	dev = devs[i];
		 	break;
		}
	}
	libusb_free_device_list(devs, 1); //free the list, unref the devices in it
    if (dev) {
		libusb_device_handle *handle = NULL;
		int ret = libusb_open(dev, &handle);
		if (LIBUSB_SUCCESS == ret) {
			printf("Device opened\n");
			unsigned char buffer[8];
			int len = 0;
			for (int j = 0; j < 100; j++) {
				ret = libusb_interrupt_transfer(handle, 0x88, buffer, sizeof(buffer), &len, 1000);
				printf("Interrupt res: %d, len: %d", ret, len);
				if (ret == 0) {
					printf(", data: [%d, %d, %d, %d, %d, %d]\n", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);
				}
				printf("\n");
			}
		}

		if (handle) {
			libusb_close(handle);
		}
	} else {
		printf("Device not found\n");
	}
	libusb_exit(ctx); //close the session
	return 0;
}