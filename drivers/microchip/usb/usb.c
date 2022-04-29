/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* Note: Do not try to use verbose logging with the USB as the console
 * (use a flexcom instead), as this will cause recursive calls.
 */

#include <assert.h>
#include <drivers/console.h>
#include <drivers/delay_timer.h>
#include <errno.h>
#include <lib/mmio.h>
#include <string.h>

#include "lan966x_regs.h"
#include "lan966x_targets.h"
#include "lan966x_private.h"

#define TIMEOUT_US_1S		U(1000000)

#define CDC_DEV_DESC_CLASS 0x02
#define CDC_DEV_DESC_SUBCLASS 0x00
#define CDC_DEV_DESC_PROTOCOL 0x00

#define CDC_COMMIF_DESC_CLASS 0x02
#define CDC_COMMIF_DESC_ABSTRACTCONTROLMODEL 0x02
#define CDC_COMMIF_DESC_NOPROTOCOL 0x00

#define CDC_GEN_DESC_INTERFACE 0x24
#define CDC_GEN_DESC_ENDPOINT 0x25

#define CDC_GEN_DESC_HEADER 0x00
#define CDC_GEN_DESC_CALLMANAGEMENT 0x01
#define CDC_GEN_DESC_ABSTRACTCONTROLMANAGEMENT 0x02
#define CDC_GEN_DESC_UNION 0x06

#define CDC_DATAIF_DESC_CLASS 0x0A
#define CDC_DATAIF_DESC_SUBCLASS 0x00
#define CDC_DATAIF_DESC_NOPROTOCOL 0x00

#define CDC_GEN_REQ_SETLINECODING 0x20
#define CDC_GEN_REQ_GETLINECODING 0x21
#define CDC_GEN_REQ_SETCONTROLLINESTATE 0x22

#define USB_GEN_DESC_DEVICE 1
#define USB_GEN_DESC_CONFIGURATION 2
#define USB_GEN_DESC_STRING 3
#define USB_GEN_DESC_INTERFACE 4
#define USB_GEN_DESC_ENDPOINT 5
#define USB_GEN_DESC_DEVICEQUALIFIER 6
#define USB_GEN_DESC_OTHERSPEEDCONFIGURATION 7
#define USB_GEN_DESC_INTERFACEPOWER 8
#define USB_GEN_DESC_OTG 9
#define USB_GEN_DESC_DEBUG 10
#define USB_GEN_DESC_INTERFACEASSOCIATION 11

#define USB_GEN_REQ_GETSTATUS 0
#define USB_GEN_REQ_CLEARFEATURE 1
#define USB_GEN_REQ_SETFEATURE 3
#define USB_GEN_REQ_SETADDRESS 5
#define USB_GEN_REQ_GETDESCRIPTOR 6
#define USB_GEN_REQ_SETDESCRIPTOR 7
#define USB_GEN_REQ_GETCONFIGURATION 8
#define USB_GEN_REQ_SETCONFIGURATION 9
#define USB_GEN_REQ_GETINTERFACE 10
#define USB_GEN_REQ_SETINTERFACE 11
#define USB_GEN_REQ_SYNCHFRAME 12

#define USB_EPT_DESC_CONTROL 0
#define USB_EPT_DESC_ISOCHRONOUS 1
#define USB_EPT_DESC_BULK 2
#define USB_EPT_DESC_INTERRUPT 3

#define USB_CONFIG_BUS_NOWAKEUP 0x80
#define USB_CONFIG_SELF_NOWAKEUP 0xC0
#define USB_CONFIG_BUS_WAKEUP 0xA0
#define USB_CONFIG_SELF_WAKEUP 0xE0

#define UDPHSEPT_NUMBER 16
#define UDPHSDMA_NUMBER 7

#define USB_RX_BUFFER_SIZE 128

#define UDPHS_EPTCFG_EPT_SIZE_8 0x0
#define UDPHS_EPTCFG_EPT_SIZE_16 0x1
#define UDPHS_EPTCFG_EPT_SIZE_32 0x2
#define UDPHS_EPTCFG_EPT_SIZE_64 0x3
#define UDPHS_EPTCFG_EPT_SIZE_128 0x4
#define UDPHS_EPTCFG_EPT_SIZE_256 0x5
#define UDPHS_EPTCFG_EPT_SIZE_512 0x6
#define UDPHS_EPTCFG_EPT_SIZE_1024 0x7

#define EP_OUT 1
#define EP_IN 2
#define EP_INTER 3

#define MAXPACKETCTRL (uint16_t)(device_descriptor[7])
#define MAXPACKETSIZEOUT                                                       \
	(uint16_t)(usb_device_config[57] + (usb_device_config[58] << 8))
#define OSCMAXPACKETSIZEOUT                                                    \
	(uint16_t)(usb_other_speed_config[57] +                                \
		   (usb_other_speed_config[58] << 8))
#define MAXPACKETSIZEIN                                                        \
	(uint16_t)(usb_device_config[64] + (usb_device_config[65] << 8))
#define OSCMAXPACKETSIZEIN                                                     \
	(uint16_t)(usb_other_speed_config[64] +                                \
		   (usb_other_speed_config[65] << 8))
#define MAXPACKETSIZEINTER                                                     \
	(uint16_t)(usb_device_config[41] + (usb_device_config[42] << 8))
#define OSCMAXPACKETSIZEINTER                                                  \
	(uint16_t)(usb_other_speed_config[41] +                                \
		   (usb_other_speed_config[42] << 8))

union usb_request {
	uint32_t data32[2];
	uint16_t data16[4];
	uint8_t data8[8];
	struct {
		uint8_t bm_request_type; /* Characteristics of the request */
		uint8_t b_request; /* Specific request */
		uint16_t w_value; /* field that varies according to request */
		uint16_t w_index; /* field that varies according to request */
		uint16_t w_length; /* Number of bytes to transfer if Data */
	} request;
};

struct cdc {
	uintptr_t base;
	uintptr_t ept_fifos;
	uintptr_t cpubase;
	uint8_t current_configuration;
	uint8_t current_connection;
	uint8_t set_line;
	union usb_request *setup;
	uint8_t rx_buffer[USB_RX_BUFFER_SIZE];
	uint16_t rx_buffer_begin;
	uint16_t rx_buffer_end;
};

struct cdc_line_coding {
	uint32_t dw_dte_rate; /* Baudrate */
	uint8_t b_char_format; /* Stop bit */
	uint8_t b_parity_type; /* Parity */
	uint8_t b_data_bits; /* data bits */
};

static struct cdc setup_cdc;

static union usb_request setup_payload;

static struct cdc_line_coding const line = {
	115200, /* baudrate */
	0, /* 1 Stop Bit */
	0, /* None Parity */
	8 /* 8 Data bits */
};

static uint8_t const device_descriptor[] = {
	/* Device descriptor */
	18, /* bLength */
	USB_GEN_DESC_DEVICE, /* bDescriptorType */
	0x00, /* bcdUSBL */
	0x02,
	CDC_DEV_DESC_CLASS, /* bDeviceClass:    CDC class code */
	CDC_DEV_DESC_SUBCLASS, /* bDeviceSubclass: CDC class sub code */
	CDC_DEV_DESC_PROTOCOL, /* bDeviceProtocol: CDC Device protocol */
	64, /* bMaxPacketSize0 */
	0x24, /* idVendorL */
	0x04,
	0x62, /* idProductL */
	0x96,
	0x10, /* bcdDeviceL */
	0x01,
	0, /* No string descriptor for manufacturer */
	0x00, /* iProduct */
	0, /* No string descriptor for serial number */
	1 /* Device has 1 possible configuration */
};

static uint8_t usb_device_config[] = {
	/* ============== CONFIGURATION 1 =========== */
	/* Table 9-10. Standard Configuration Descriptor */
	9, /* size of this descriptor in bytes */
	USB_GEN_DESC_CONFIGURATION, /* CONFIGURATION descriptor type */
	67, /* total length of data returned 2 EP + Control + OTG */
	0x00, 2, /* There are two interfaces in this configuration */
	1, /* This is configuration #1 */
	0, /* No string descriptor for this configuration */
	USB_CONFIG_SELF_NOWAKEUP, /* Configuration characteristics */
	50, /* 100mA */

	/* Communication Class Interface Descriptor Requirement */
	/* Table 9-12. Standard Interface Descriptor */
	9, /* Size of this descriptor in bytes */
	USB_GEN_DESC_INTERFACE, /* INTERFACE Descriptor Type */
	0, /* This is interface #0 */
	0, /* This is alternate setting #0 for this interface */
	1, /* This interface uses 1 endpoint */
	CDC_COMMIF_DESC_CLASS, /* bInterfaceClass */
	CDC_COMMIF_DESC_ABSTRACTCONTROLMODEL, /* bInterfaceSubclass */
	CDC_COMMIF_DESC_NOPROTOCOL, /* bInterfaceProtocol */
	0, /* No string descriptor for this interface */

	/* 5.2.3.1 Header Functional Descriptor (usbcdc11.pdf) */
	5, /* bFunction Length */
	CDC_GEN_DESC_INTERFACE, /* bDescriptor type: CS_INTERFACE */
	CDC_GEN_DESC_HEADER, /* bDescriptor subtype: Header Func Desc */
	0x10, /* bcdCDC: CDC Class Version 1.10 */
	0x01,

	/* 5.2.3.2 Call Management Functional Descriptor (usbcdc11.pdf) */
	5, /* bFunctionLength */
	CDC_GEN_DESC_INTERFACE, /* bDescriptor Type: CS_INTERFACE */
	CDC_GEN_DESC_CALLMANAGEMENT, /* bDescriptor Subtype: Call Management Func Desc */
	0x00, /* bmCapabilities: D1 + D0 */
	0x01, /* bDataInterface: Data Class Interface 1 */

	/* 5.2.3.3 Abstract Control Management Functional Descriptor (usbcdc11.pdf) */
	4, /* bFunctionLength */
	CDC_GEN_DESC_INTERFACE, /* bDescriptor Type: CS_INTERFACE */
	CDC_GEN_DESC_ABSTRACTCONTROLMANAGEMENT, /* bDescriptor Subtype: ACM Func Desc */
	0x00, /* bmCapabilities */

	/* 5.2.3.8 Union Functional Descriptor (usbcdc11.pdf) */
	5, /* bFunctionLength */
	CDC_GEN_DESC_INTERFACE, /* bDescriptorType: CS_INTERFACE */
	CDC_GEN_DESC_UNION, /* bDescriptor Subtype: Union Func Desc */
	0, /* Number of master interface is #0 */
	1, /* First slave interface is #1 */

	/* Endpoint 1 descriptor */
	/* Table 9-13. Standard Endpoint Descriptor */
	7, /* bLength */
	USB_GEN_DESC_ENDPOINT, /* bDescriptorType */
	0x80 | EP_INTER, /* bEndpointAddress, Endpoint EP_INTER - IN */
	USB_EPT_DESC_INTERRUPT, /* bmAttributes      INT */
	0x40, 0x00, /* wMaxPacketSize = 64 */
	0x10, /* Endpoint is polled every 16ms */

	/* Table 9-12. Standard Interface Descriptor */
	9, /* bLength */
	USB_GEN_DESC_INTERFACE, /* bDescriptorType */
	1, /* This is interface #1 */
	0, /* This is alternate setting #0 for this interface */
	2, /* This interface uses 2 endpoints */
	CDC_DATAIF_DESC_CLASS, CDC_DATAIF_DESC_SUBCLASS,
	CDC_DATAIF_DESC_NOPROTOCOL,
	0, /* No string descriptor for this interface */

	/* First alternate setting */
	/* Table 9-13. Standard Endpoint Descriptor */
	7, /* bLength */
	USB_GEN_DESC_ENDPOINT, /* bDescriptorType */
	EP_OUT, /* bEndpointAddress, Endpoint EP_OUT - OUT */
	USB_EPT_DESC_BULK, /* bmAttributes      BULK */
	0x00, 0x02, /* wMaxPacketSize = 512 */
	0, /* Must be 0 for full-speed bulk endpoints */

	/* Table 9-13. Standard Endpoint Descriptor */
	7, /* bLength */
	USB_GEN_DESC_ENDPOINT, /* bDescriptorType */
	0x80 | EP_IN, /* bEndpointAddress, Endpoint EP_IN - IN */
	USB_EPT_DESC_BULK, /* bmAttributes      BULK */
	0x00, 0x02, /* wMaxPacketSize = 512 */
	0 /* Must be 0 for full-speed bulk endpoints */
};

static uint8_t usb_other_speed_config[] = {
	/* ============== CONFIGURATION 1 =========== */
	/* Table 9-10. Standard Configuration Descriptor */
	0x09, /* size of this descriptor in bytes */
	USB_GEN_DESC_OTHERSPEEDCONFIGURATION, /* CONFIGURATION descriptor type */
	67, /* total length of data returned 2 EP + Control */
	0x00, 0x02, /* There are two interfaces in this configuration */
	0x01, /* This is configuration #1 */
	0x00, /* No string descriptor for this configuration */
	USB_CONFIG_SELF_NOWAKEUP, /* Configuration characteristics */
	50, /* 100mA */

	/* Communication Class Interface Descriptor Requirement */
	/* Table 9-12. Standard Interface Descriptor */
	9, /* Size of this descriptor in bytes */
	USB_GEN_DESC_INTERFACE, /* INTERFACE Descriptor Type */
	0, /* This is interface #0 */
	0, /* This is alternate setting #0 for this interface */
	1, /* This interface uses 1 endpoint */
	CDC_COMMIF_DESC_CLASS, /* bInterfaceClass */
	CDC_COMMIF_DESC_ABSTRACTCONTROLMODEL, /* bInterfaceSubclass */
	CDC_COMMIF_DESC_NOPROTOCOL, /* bInterfaceProtocol */
	0x00, /* No string descriptor for this interface */

	/* 5.2.3.1 Header Functional Descriptor (usbcdc11.pdf) */
	5, /* bFunction Length */
	CDC_GEN_DESC_INTERFACE, /* bDescriptor type: CS_INTERFACE */
	CDC_GEN_DESC_HEADER, /* bDescriptor subtype: Header Func Desc */
	0x10, /* bcdCDC: CDC Class Version 1.10 */
	0x01,

	/* 5.2.3.2 Call Management Functional Descriptor (usbcdc11.pdf) */
	5, /* bFunctionLength */
	CDC_GEN_DESC_INTERFACE, /* bDescriptor Type: CS_INTERFACE */
	CDC_GEN_DESC_CALLMANAGEMENT, /* bDescriptor Subtype: Call Management Func Desc */
	0x00, /* bmCapabilities: D1 + D0 */
	0x01, /* bDataInterface: Data Class Interface 1 */

	/* 5.2.3.3 Abstract Control Management Functional Descriptor (usbcdc11.pdf) */
	4, /* bFunctionLength */
	CDC_GEN_DESC_INTERFACE, /* bDescriptor Type: CS_INTERFACE */
	CDC_GEN_DESC_ABSTRACTCONTROLMANAGEMENT, /* bDescriptor Subtype: ACM Func Desc */
	0x00, /* bmCapabilities */

	/* 5.2.3.8 Union Functional Descriptor (usbcdc11.pdf) */
	5, /* bFunctionLength */
	CDC_GEN_DESC_INTERFACE, /* bDescriptorType: CS_INTERFACE */
	CDC_GEN_DESC_UNION, /* bDescriptor Subtype: Union Func Desc */
	0, /* Number of master interface is #0 */
	1, /* First slave interface is #1 */

	/* Endpoint 1 descriptor */
	/* Table 9-13. Standard Endpoint Descriptor */
	7, /* bLength */
	USB_GEN_DESC_ENDPOINT, /* bDescriptorType */
	0x80 | EP_INTER, /* bEndpointAddress, Endpoint EP_INTER - IN */
	USB_EPT_DESC_INTERRUPT, /* bmAttributes      INT */
	0x40, 0x00, /* wMaxPacketSize */
	0x10, /* bInterval, for HS value between 0x01 and 0x10 = 16ms here */

	/* Table 9-12. Standard Interface Descriptor */
	9, /* bLength */
	USB_GEN_DESC_INTERFACE, /* bDescriptorType */
	1, /* This is interface #1 */
	0, /* This is alternate setting #0 for this interface */
	2, /* This interface uses 2 endpoints */
	CDC_DATAIF_DESC_CLASS, CDC_DATAIF_DESC_SUBCLASS,
	CDC_DATAIF_DESC_NOPROTOCOL,
	0, /* No string descriptor for this interface */

	/* First alternate setting */
	/* Table 9-13. Standard Endpoint Descriptor */
	7, /* bLength */
	USB_GEN_DESC_ENDPOINT, /* bDescriptorType */
	EP_OUT, /* bEndpointAddress, Endpoint EP_OUT - OUT */
	USB_EPT_DESC_BULK, /* bmAttributes      BULK */
	0x40, 0x00, /* wMaxPacketSize = 64 */
	0, /* Must be 0 for full-speed bulk endpoints */

	/* Table 9-13. Standard Endpoint Descriptor */
	7, /* bLength */
	USB_GEN_DESC_ENDPOINT, /* bDescriptorType */
	0x80 | EP_IN, /* bEndpointAddress, Endpoint EP_IN - IN */
	USB_EPT_DESC_BULK, /* bmAttributes      BULK */
	0x40, 0x00, /* wMaxPacketSize = 64 */
	0 /* Must be 0 for full-speed bulk endpoints */
};

static bool lan966x_usb_epts_up(struct cdc *cdc)
{
	return ((mmio_read_32(UDPHS0_UDPHS_EPTCFG0(cdc->base)) &
		 UDPHS0_UDPHS_EPTCFG0_EPT_MAPD_EPTCFG0_M) &&
		(mmio_read_32(UDPHS0_UDPHS_EPTCFG1(cdc->base)) &
		 UDPHS0_UDPHS_EPTCFG1_EPT_MAPD_EPTCFG1_M) &&
		(mmio_read_32(UDPHS0_UDPHS_EPTCFG2(cdc->base)) &
		 UDPHS0_UDPHS_EPTCFG2_EPT_MAPD_EPTCFG2_M) &&
		(mmio_read_32(UDPHS0_UDPHS_EPTCFG3(cdc->base)) &
		 UDPHS0_UDPHS_EPTCFG3_EPT_MAPD_EPTCFG3_M));
}

static bool lan966x_usb_tx_wait(struct cdc *cdc)
{
	VERBOSE("lan966x: %s\n", __func__);
	/* Set the TXRDY indication */
	mmio_write_32(UDPHS0_UDPHS_EPTSETSTA0(cdc->base),
		      UDPHS0_UDPHS_EPTSETSTA0_TXRDY_EPTSETSTA0(1));
	/* Wait until the TXRDY goes low (no TX packet ongoing) or the
	 * device is suspended
	 */
	while (((mmio_read_32(UDPHS0_UDPHS_EPTSTA0(cdc->base)) &
		 UDPHS0_UDPHS_EPTSTA0_TXRDY_EPTSTA0_M) != 0) &&
	       ((mmio_read_32(UDPHS0_UDPHS_INTSTA(cdc->base)) &
		 UDPHS0_UDPHS_INTSTA_DET_SUSPD_INTSTA_M) !=
		UDPHS0_UDPHS_INTSTA_DET_SUSPD_INTSTA_M))
		;
	/* Return suspended status */
	return ((mmio_read_32(UDPHS0_UDPHS_INTSTA(cdc->base)) &
		 UDPHS0_UDPHS_INTSTA_DET_SUSPD_INTSTA_M) ==
		UDPHS0_UDPHS_INTSTA_DET_SUSPD_INTSTA_M);
}

static void lan966x_usb_wait_txrdy(struct cdc *cdc)
{
	/* Wait until the TXRDY goes low (no TX packet ongoing) */
	VERBOSE("lan966x: %s\n", __func__);
	while ((mmio_read_32(UDPHS0_UDPHS_EPTSTA0(cdc->base)) &
		UDPHS0_UDPHS_EPTSTA0_TXRDY_EPTSTA0_M) != 0)
		;
	VERBOSE("lan966x: %s: done\n", __func__);
}

static bool lan966x_usb_wait_rxrdy(struct cdc *cdc)
{
	VERBOSE("lan966x: %s - start waiting\n", __func__);
	/* Wait for RXRDY indication */
	while ((mmio_read_32(UDPHS0_UDPHS_EPTSTA0(cdc->base)) &
		UDPHS0_UDPHS_EPTSTA0_RXRDY_TXKL_EPTSTA0_M) !=
	       UDPHS0_UDPHS_EPTSTA0_RXRDY_TXKL_EPTSTA0_M)
		;
	/* Clear RXRDY indication */
	mmio_write_32(UDPHS0_UDPHS_EPTCLRSTA0(cdc->base),
		      UDPHS0_UDPHS_EPTCLRSTA0_RXRDY_TXKL_EPTCLRSTA0(1));
	VERBOSE("lan966x: %s - done waiting\n", __func__);
	return false;
}

static void lan966x_usb_send_ctrldata(struct cdc *cdc, const uint8_t *data,
				      uint32_t length)
{
	uintptr_t ept_fifos = cdc->ept_fifos;
	uint32_t index = 0;
	uint32_t cpt = 0;
	uint8_t *fifo;

	VERBOSE("lan966x: %s: length: %u, wait\n", __func__, length);
	lan966x_usb_wait_txrdy(cdc);
	if (length) {
		while (length) {
			cpt = MIN(length, (uint32_t)MAXPACKETCTRL);
			length -= cpt;
			fifo = (uint8_t *)ept_fifos;

			/* Copy data to endpoint RAM */
			while (cpt--) {
				fifo[index] = data[index];
				VERBOSE("lan966x: %s: fifo[%u]: 0x%02x\n",
					__func__, index, fifo[index]);
				index++;
			}
			if (lan966x_usb_tx_wait(cdc))
				break;
		}
	} else {
		VERBOSE("lan966x: %s: empty packet\n", __func__);
		lan966x_usb_wait_txrdy(cdc);
		lan966x_usb_tx_wait(cdc);
	}
	VERBOSE("lan966x: %s: length: %u, done\n", __func__, length);
}

static void lan966x_usb_send_ctrlzlp(struct cdc *cdc)
{
	VERBOSE("lan966x: %s\n", __func__);
	while ((mmio_read_32(UDPHS0_UDPHS_EPTSTA0(cdc->base)) &
		UDPHS0_UDPHS_EPTSTA0_TXRDY_EPTSTA0_M) != 0)
		;

	mmio_write_32(UDPHS0_UDPHS_EPTSETSTA0(cdc->base),
		      UDPHS0_UDPHS_EPTSETSTA0_TXRDY_EPTSETSTA0(1));

	while (((mmio_read_32(UDPHS0_UDPHS_EPTSTA0(cdc->base)) &
		 UDPHS0_UDPHS_EPTSTA0_TXRDY_EPTSTA0_M) != 0) &&
	       ((mmio_read_32(UDPHS0_UDPHS_INTSTA(cdc->base)) &
		 UDPHS0_UDPHS_INTSTA_DET_SUSPD_INTSTA_M) !=
		UDPHS0_UDPHS_INTSTA_DET_SUSPD_INTSTA_M))
		;
}

static void lan966x_usb_send_ctrlstall(struct cdc *cdc)
{
	VERBOSE("lan966x: %s\n", __func__);
	mmio_write_32(UDPHS0_UDPHS_EPTSETSTA0(cdc->base),
		      UDPHS0_UDPHS_EPTSETSTA0_FRCESTALL_EPTSETSTA0(1));
}

static uint8_t lan966x_usb_eptpktsize(uint16_t packet_size)
{
	switch (packet_size) {
	case 8:
		return UDPHS_EPTCFG_EPT_SIZE_8;
	case 16:
		return UDPHS_EPTCFG_EPT_SIZE_16;
	case 32:
		return UDPHS_EPTCFG_EPT_SIZE_32;
	case 64:
		return UDPHS_EPTCFG_EPT_SIZE_64;
	case 128:
		return UDPHS_EPTCFG_EPT_SIZE_128;
	case 256:
		return UDPHS_EPTCFG_EPT_SIZE_256;
	case 512:
		return UDPHS_EPTCFG_EPT_SIZE_512;
	case 1024:
		return UDPHS_EPTCFG_EPT_SIZE_1024;
	default:
		return 0;
	}
}

static void lan966x_cdc_enumerate(struct cdc *cdc)
{
	uintptr_t ept_fifos = cdc->ept_fifos;
	union usb_request *setup_data = cdc->setup;
	uint16_t max_pktsize_out;
	uint16_t max_pktsize_in;
	uint16_t max_pktsize_inter;
	uint16_t ept_size;

	/* Check if RX Setup has been received */
	if ((mmio_read_32(UDPHS0_UDPHS_EPTSTA0(cdc->base)) &
	     UDPHS0_UDPHS_EPTSTA0_RX_SETUP_EPTSTA0_M) !=
	    UDPHS0_UDPHS_EPTSTA0_RX_SETUP_EPTSTA0_M)
		return;

	setup_data->data32[0] = mmio_read_32(ept_fifos);
	setup_data->data32[1] = mmio_read_32(ept_fifos);

	/* Clear RX Setup indication */
	mmio_write_32(UDPHS0_UDPHS_EPTCLRSTA0(cdc->base),
		      UDPHS0_UDPHS_EPTCLRSTA0_RX_SETUP_EPTCLRSTA0(1));

	/* Handle supported standard device request Cf Table 9-3 in USB
	 * specification Rev 1.1 */
	switch (setup_data->request.b_request) {
	case USB_GEN_REQ_GETDESCRIPTOR:
		VERBOSE("lan966x: %s: USB_GEN_REQ_GETDESCRIPTOR\n", __func__);
		if (setup_data->request.w_value == (USB_GEN_DESC_DEVICE << 8)) {
			lan966x_usb_send_ctrldata(
				cdc, device_descriptor,
				MIN((uint16_t)sizeof(device_descriptor),
				    setup_data->request.w_length));
		} else if (setup_data->request.w_value ==
			   (USB_GEN_DESC_CONFIGURATION << 8)) {
			if (mmio_read_32(UDPHS0_UDPHS_INTSTA(cdc->base)) &
			    UDPHS0_UDPHS_INTSTA_SPEED_M) {
				usb_device_config[1] =
					USB_GEN_DESC_CONFIGURATION;
				lan966x_usb_send_ctrldata(
					cdc, usb_device_config,
					MIN((uint16_t)sizeof(usb_device_config),
					    setup_data->request.w_length));
			} else {
				usb_other_speed_config[1] =
					USB_GEN_DESC_CONFIGURATION;
				lan966x_usb_send_ctrldata(
					cdc, usb_other_speed_config,
					MIN((uint16_t)sizeof(
						    usb_other_speed_config),
					    setup_data->request.w_length));
			}
		} else {
			lan966x_usb_send_ctrlstall(cdc);
		}
		break;

	case USB_GEN_REQ_SETADDRESS:
		VERBOSE("lan966x: %s: USB_GEN_REQ_SETADDRESS\n", __func__);
		lan966x_usb_send_ctrlzlp(cdc);
		mmio_clrsetbits_32(UDPHS0_UDPHS_CTRL(cdc->base),
				   UDPHS0_UDPHS_CTRL_DEV_ADDR_M |
					   UDPHS0_UDPHS_CTRL_FADDR_EN_M,
				   UDPHS0_UDPHS_CTRL_DEV_ADDR(
					   setup_data->request.w_value & 0x7F) |
					   UDPHS0_UDPHS_CTRL_FADDR_EN(1));
		break;

	case USB_GEN_REQ_SETCONFIGURATION:
		VERBOSE("lan966x: %s: USB_GEN_REQ_SETCONFIGURATION\n", __func__);
		cdc->current_configuration =
			(uint8_t)setup_data->request
				.w_value; /* The lower byte of the wValue field
					     specifies the desired
					     configuration. */
		lan966x_usb_send_ctrlzlp(cdc);

		if (mmio_read_32(UDPHS0_UDPHS_INTSTA(cdc->base)) &
		    UDPHS0_UDPHS_INTSTA_SPEED_M) {
			/* High Speed */
			max_pktsize_out = MAXPACKETSIZEOUT;
			max_pktsize_in = MAXPACKETSIZEIN;
			max_pktsize_inter = MAXPACKETSIZEINTER;
		} else {
			/* Full Speed */
			max_pktsize_out = OSCMAXPACKETSIZEOUT;
			max_pktsize_in = OSCMAXPACKETSIZEIN;
			max_pktsize_inter = OSCMAXPACKETSIZEINTER;
		}

		ept_size = lan966x_usb_eptpktsize(max_pktsize_out);
		mmio_write_32(
			UDPHS0_UDPHS_EPTCFG1(cdc->base),
			UDPHS0_UDPHS_EPTCFG1_EPT_SIZE_EPTCFG1(ept_size) |
				UDPHS0_UDPHS_EPTCFG1_EPT_TYPE_EPTCFG1(2) |
				UDPHS0_UDPHS_EPTCFG1_BK_NUMBER_EPTCFG1(2));
		while ((mmio_read_32(UDPHS0_UDPHS_EPTCFG1(cdc->base)) &
			UDPHS0_UDPHS_EPTCFG1_EPT_MAPD_EPTCFG1_M) !=
		       UDPHS0_UDPHS_EPTCFG1_EPT_MAPD_EPTCFG1_M)
			;
		mmio_write_32(
			UDPHS0_UDPHS_EPTCTLENB1(cdc->base),
			UDPHS0_UDPHS_EPTCTLENB1_RXRDY_TXKL_EPTCTLENB1(1) |
				UDPHS0_UDPHS_EPTCTLENB1_EPT_ENABL_EPTCTLENB1(
					1));

		ept_size = lan966x_usb_eptpktsize(max_pktsize_in);
		mmio_write_32(
			UDPHS0_UDPHS_EPTCFG2(cdc->base),
			UDPHS0_UDPHS_EPTCFG2_EPT_SIZE_EPTCFG2(ept_size) |
				UDPHS0_UDPHS_EPTCFG2_EPT_DIR_EPTCFG2(1) |
				UDPHS0_UDPHS_EPTCFG2_EPT_TYPE_EPTCFG2(2) |
				UDPHS0_UDPHS_EPTCFG2_BK_NUMBER_EPTCFG2(2));
		while ((mmio_read_32(UDPHS0_UDPHS_EPTCFG2(cdc->base)) &
			UDPHS0_UDPHS_EPTCFG2_EPT_MAPD_EPTCFG2_M) !=
		       UDPHS0_UDPHS_EPTCFG2_EPT_MAPD_EPTCFG2_M)
			;
		mmio_write_32(
			UDPHS0_UDPHS_EPTCTLENB2(cdc->base),
			UDPHS0_UDPHS_EPTCTLENB2_SHRT_PCKT_EPTCTLENB2_M |
				UDPHS0_UDPHS_EPTCTLENB2_EPT_ENABL_EPTCTLENB2_M);

		ept_size = lan966x_usb_eptpktsize(max_pktsize_inter);
		mmio_write_32(
			UDPHS0_UDPHS_EPTCFG3(cdc->base),
			UDPHS0_UDPHS_EPTCFG3_EPT_SIZE_EPTCFG3(ept_size) |
				UDPHS0_UDPHS_EPTCFG3_EPT_DIR_EPTCFG3(1) |
				UDPHS0_UDPHS_EPTCFG3_EPT_TYPE_EPTCFG3(3) |
				UDPHS0_UDPHS_EPTCFG3_BK_NUMBER_EPTCFG3(1));
		while ((mmio_read_32(UDPHS0_UDPHS_EPTCFG3(cdc->base)) &
			UDPHS0_UDPHS_EPTCFG3_EPT_MAPD_EPTCFG3_M) !=
		       UDPHS0_UDPHS_EPTCFG3_EPT_MAPD_EPTCFG3_M)
			;
		mmio_write_32(UDPHS0_UDPHS_EPTCTLENB3(cdc->base),
			      UDPHS0_UDPHS_EPTCTLENB3_EPT_ENABL_EPTCTLENB3(1));
		break;

	case USB_GEN_REQ_GETCONFIGURATION:
		VERBOSE("lan966x: %s: USB_GEN_REQ_GETCONFIGURATION\n", __func__);
		lan966x_usb_send_ctrldata(
			cdc, (uint8_t *)&(cdc->current_configuration),
			sizeof(cdc->current_configuration));
		break;

	/* handle CDC class requests */
	case CDC_GEN_REQ_SETLINECODING:
		VERBOSE("lan966x: %s: CDC_GEN_REQ_SETLINECODING\n", __func__);
		/* Waiting for Status stage */
		lan966x_usb_wait_rxrdy(cdc);
		cdc->set_line = 1;
		lan966x_usb_send_ctrlzlp(cdc);
		break;

	case CDC_GEN_REQ_GETLINECODING:
		VERBOSE("lan966x: %s: CDC_GEN_REQ_GETLINECODING\n", __func__);
		lan966x_usb_send_ctrldata(cdc, (uint8_t *)&line,
					  MIN((uint16_t)sizeof(line),
					      setup_data->request.w_length));
		break;

	case CDC_GEN_REQ_SETCONTROLLINESTATE:
		VERBOSE("lan966x: %s: CDC_GEN_REQ_SETCONTROLLINESTATE\n",
		     __func__);
		cdc->current_connection = (uint8_t)setup_data->request.w_value;
		cdc->set_line = 0;
		lan966x_usb_send_ctrlzlp(cdc);
		break;

	/* case USB_GEN_REQ_SETINTERFACE:  MUST BE STALL for us */
	default:
		VERBOSE("lan966x: %s: default handling\n", __func__);
		lan966x_usb_send_ctrlstall(cdc);
		break;
	}
}

static void lan966x_usb_config_ctrlept(struct cdc *cdc)
{
	uint16_t ept_size = lan966x_usb_eptpktsize(MAXPACKETCTRL);

	VERBOSE("lan966x: %s, control endpoint: size and bankcount\n", __func__);
	mmio_clrsetbits_32(
		UDPHS0_UDPHS_EPTCFG0(cdc->base),
		UDPHS0_UDPHS_EPTCFG0_EPT_SIZE_EPTCFG0_M |
		UDPHS0_UDPHS_EPTCFG0_EPT_TYPE_EPTCFG0_M |
		UDPHS0_UDPHS_EPTCFG0_BK_NUMBER_EPTCFG0_M,
		UDPHS0_UDPHS_EPTCFG0_EPT_SIZE_EPTCFG0(ept_size) |
		UDPHS0_UDPHS_EPTCFG0_EPT_TYPE_EPTCFG0(0) |
		UDPHS0_UDPHS_EPTCFG0_BK_NUMBER_EPTCFG0(1));

	VERBOSE("lan966x: %s, waiting for mapping\n", __func__);
	while ((mmio_read_32(UDPHS0_UDPHS_EPTCFG0(cdc->base)) &
		UDPHS0_UDPHS_EPTCFG0_EPT_MAPD_EPTCFG0_M) !=
	       UDPHS0_UDPHS_EPTCFG0_EPT_MAPD_EPTCFG0_M)
		;

	mmio_clrsetbits_32(
		UDPHS0_UDPHS_EPTCTLENB0(cdc->base),
		UDPHS0_UDPHS_EPTCTLENB0_RX_SETUP_EPTCTLENB0_M |
		UDPHS0_UDPHS_EPTCTLENB0_EPT_ENABL_EPTCTLENB0_M,
		UDPHS0_UDPHS_EPTCTLENB0_RX_SETUP_EPTCTLENB0(1) |
		UDPHS0_UDPHS_EPTCTLENB0_EPT_ENABL_EPTCTLENB0(1));

}

static uint8_t lan966x_usb_is_configured(struct cdc *cdc)
{
	uint32_t isr = mmio_read_32(UDPHS0_UDPHS_INTSTA(cdc->base));

	/* Resume */
	VERBOSE("lan966x: %s\n", __func__);
	if ((isr & UDPHS0_UDPHS_INTSTA_WAKE_UP_INTSTA_M) ||
	    (isr & UDPHS0_UDPHS_INTSTA_ENDOFRSM_INTSTA_M))
		mmio_clrsetbits_32(
			UDPHS0_UDPHS_CLRINT(cdc->base),
			UDPHS0_UDPHS_CLRINT_WAKE_UP_CLRINT_M |
				UDPHS0_UDPHS_CLRINT_ENDOFRSM_CLRINT_M,
			UDPHS0_UDPHS_CLRINT_WAKE_UP_CLRINT(1) |
				UDPHS0_UDPHS_CLRINT_ENDOFRSM_CLRINT(1));

	if (isr & UDPHS0_UDPHS_INTSTA_INT_SOF_INTSTA_M) {
		mmio_clrsetbits_32(UDPHS0_UDPHS_CLRINT(cdc->base),
				   UDPHS0_UDPHS_CLRINT_INT_SOF_CLRINT_M,
				   UDPHS0_UDPHS_CLRINT_INT_SOF_CLRINT(1));
	} else {
		if (isr & UDPHS0_UDPHS_INTSTA_MICRO_SOF_INTSTA_M) {
			mmio_clrsetbits_32(
				UDPHS0_UDPHS_CLRINT(cdc->base),
				UDPHS0_UDPHS_CLRINT_MICRO_SOF_CLRINT_M,
				UDPHS0_UDPHS_CLRINT_MICRO_SOF_CLRINT(1));
		}
	}

	/* Suspend */
	if (isr & UDPHS0_UDPHS_INTSTA_DET_SUSPD_INTSTA_M) {
		cdc->current_configuration = 0;
		cdc->set_line = 0;
		mmio_clrsetbits_32(UDPHS0_UDPHS_CLRINT(cdc->base),
				   UDPHS0_UDPHS_CLRINT_DET_SUSPD_CLRINT_M,
				   UDPHS0_UDPHS_CLRINT_DET_SUSPD_CLRINT(1));
	} else {
		/* End of reset: ready to configure */
		if (isr & UDPHS0_UDPHS_INTSTA_ENDRESET_INTSTA_M) {
			cdc->current_configuration = 0;
			cdc->set_line = 0;
			lan966x_usb_config_ctrlept(cdc);
			/* Clear the end-of-reset interrupt */
			mmio_clrsetbits_32(
				UDPHS0_UDPHS_CLRINT(cdc->base),
				UDPHS0_UDPHS_CLRINT_ENDRESET_CLRINT_M,
				UDPHS0_UDPHS_CLRINT_ENDRESET_CLRINT(1));
		} else {
			uint32_t eptint =
				UDPHS0_UDPHS_INTSTA_EPT_INTSTA_X(isr);

			if (eptint) {
				VERBOSE("lan966x: %s, eptint: 0x%02x\n",
					__func__, eptint);
				lan966x_cdc_enumerate(cdc);
			}
		}
	}
	return cdc->current_configuration && cdc->set_line;
}

static uint32_t lan966x_usb_read(struct cdc *cdc)
{
	uint32_t recv = 0;
	uint8_t *fifo;
	uint32_t size;
	uint8_t *infifo;

	if (!lan966x_usb_is_configured(cdc))
		return 0;

	infifo = (uint8_t *)cdc->ept_fifos + ((64 * 1024) * EP_OUT);
	/* If RXRDY is indicated on endpoint 1 */
	if ((mmio_read_32(UDPHS0_UDPHS_EPTSTA1(cdc->base)) &
	     UDPHS0_UDPHS_EPTSTA1_RXRDY_TXKL_EPTSTA1_M)) {
		size = (mmio_read_32(UDPHS0_UDPHS_EPTSTA1(cdc->base)) &
			UDPHS0_UDPHS_EPTSTA1_BYTE_COUNT_EPTSTA1_M) >>
		       20;
		fifo = infifo;

		/* Copy data from the endpoint buffer to our rx_buffer */
		while (size--) {
			cdc->rx_buffer[cdc->rx_buffer_end] = fifo[recv];
			recv++;

			cdc->rx_buffer_end =
				(cdc->rx_buffer_end + 1) % USB_RX_BUFFER_SIZE;
			if (cdc->rx_buffer_end == cdc->rx_buffer_begin)
				break;
		}

		/* Clear the RXRDY indication on endpoint 1: ready for more */
		mmio_write_32(UDPHS0_UDPHS_EPTCLRSTA1(cdc->base),
			      UDPHS0_UDPHS_EPTCLRSTA1_RXRDY_TXKL_EPTCLRSTA1(1));
	}
	return 0;
}

static uint32_t lan966x_usb_write(struct cdc *cdc, const uint8_t *data,
				  uint32_t length)
{
	uint32_t cpt = 0;
	uint32_t packet_size;
	uint8_t *outfifo;
	uint8_t *fifo;

	if (!lan966x_usb_is_configured(cdc))
		return 0;

	outfifo = (uint8_t *)cdc->ept_fifos + ((64 * 1024) * EP_IN);
	if (length) {
		VERBOSE("lan966x: %s, send: %u\n", __func__, length);
		/* Get the packet size via the speed indication */
		packet_size = mmio_read_32(UDPHS0_UDPHS_INTSTA(cdc->base)) &
					      UDPHS0_UDPHS_INTSTA_SPEED_M ?
					    MAXPACKETSIZEIN :
					    OSCMAXPACKETSIZEIN;

		/* Wait for the TXRDY indication on endpoint 2 */
		while ((mmio_read_32(UDPHS0_UDPHS_EPTSTA2(cdc->base)) &
			UDPHS0_UDPHS_EPTSTA2_TXRDY_EPTSTA2_M))
			;

		while (length) {
			cpt = MIN(length, packet_size);
			length -= cpt;
			fifo = outfifo;

			/* Copy data to the endpoint buffer */
			while (cpt) {
				*(fifo++) = *(data++);
				--cpt;
			}

			/* Set the TXRDY indication for endpooint 2 */
			mmio_write_32(
				UDPHS0_UDPHS_EPTSETSTA2(cdc->base),
				UDPHS0_UDPHS_EPTSETSTA2_TXRDY_EPTSETSTA2(1));

			/* Wait for TXRDY indication to go low or a suspend */
			while (((mmio_read_32(UDPHS0_UDPHS_EPTSTA2(cdc->base)) &
				 UDPHS0_UDPHS_EPTSTA2_TXRDY_EPTSTA2_M)) &&
			       ((mmio_read_32(UDPHS0_UDPHS_INTSTA(cdc->base)) &
				 UDPHS0_UDPHS_INTSTA_DET_SUSPD_INTSTA_M) !=
				UDPHS0_UDPHS_INTSTA_DET_SUSPD_INTSTA_M))
				;

			/* Bail out if it was a suspend */
			if ((mmio_read_32(UDPHS0_UDPHS_INTSTA(cdc->base)) &
			     UDPHS0_UDPHS_INTSTA_DET_SUSPD_INTSTA_M) ==
			    UDPHS0_UDPHS_INTSTA_DET_SUSPD_INTSTA_M)
				break;
		}
	}
	return length;
}


static int lan966x_usb_putc(int ch, struct console *con)
{
	uint8_t buffer[1] = { ch };

	lan966x_usb_write(&setup_cdc, buffer, 1);
	return 0;
}

static int lan966x_usb_getc(struct console *con)
{
	struct cdc *cdc = &setup_cdc;
	int val = 0;

	lan966x_usb_read(cdc);
	if (cdc->rx_buffer_begin != cdc->rx_buffer_end) {
		val = cdc->rx_buffer[cdc->rx_buffer_begin];
		if (++cdc->rx_buffer_begin >= USB_RX_BUFFER_SIZE) {
			cdc->rx_buffer_begin = 0;
		}
	}
	return val;
}

static void lan966x_usb_flush(struct console *con)
{
	struct cdc *cdc = &setup_cdc;
	cdc->rx_buffer_begin = 0;
	cdc->rx_buffer_end = 0;
}

static console_t lan966x_usb_console = {
	.putc = lan966x_usb_putc,
	.getc = lan966x_usb_getc,
	.flush = lan966x_usb_flush
};

void lan966x_usb_register_console(void)
{
	console_register(&lan966x_usb_console);
	console_set_scope(&lan966x_usb_console, CONSOLE_FLAG_BOOT |
			  CONSOLE_FLAG_CRASH | CONSOLE_FLAG_TRANSLATE_CRLF);
}

static void lan966x_usb_device_init(struct cdc *cdc)
{
	VERBOSE("lan966x: %s\n", __func__);
	/* enable clock */
	mmio_clrsetbits_32(CPU_CLK_GATING(cdc->cpubase),
			   CPU_CLK_GATING_UDPHS_CLK_GATING_M,
			   CPU_CLK_GATING_UDPHS_CLK_GATING_M);
	/* Enable Device mode */
	mmio_clrsetbits_32(
		UDPHS0_UDPHS_CTRL(cdc->base),
		UDPHS0_UDPHS_CTRL_PULLD_DIS_M | UDPHS0_UDPHS_CTRL_EN_UDPHS_M |
			UDPHS0_UDPHS_CTRL_DETACH_M,
		UDPHS0_UDPHS_CTRL_PULLD_DIS(1) | UDPHS0_UDPHS_CTRL_EN_UDPHS(1) |
			UDPHS0_UDPHS_CTRL_DETACH(0));
	/* Reset of ULPI bridge */
	mmio_write_32(CPU_ULPI_RST(cdc->cpubase), CPU_ULPI_RST_ULPI_RST(0));
	/* Wait for UTMI clock to be stable */
	mdelay(1);
	/* Disable endpoint interrupts 0-15 */
	mmio_clrsetbits_32(UDPHS0_UDPHS_IEN(cdc->base),
			   UDPHS0_UDPHS_IEN_EPT_M, UDPHS0_UDPHS_IEN_EPT(0));
	VERBOSE("lan966x: %s - Done\n", __func__);
}

static void lan966x_apply_trim(uintptr_t cpubase, const struct usb_trim *trim)
{
	if (trim->valid) {
		mmio_write_32(CPU_USB_BIAS_MAG(cpubase),
			      CPU_USB_BIAS_MAG_TRIM(trim->bias));
		mmio_write_32(CPU_USB_RBIAS_MAG(cpubase),
			      CPU_USB_RBIAS_MAG_TRIM(trim->rbias));
	}
}

void lan966x_usb_init(const struct usb_trim *trim)
{
	struct cdc *cdc = &setup_cdc;
	union usb_request *req = &setup_payload;
	uint64_t timeout = timeout_init_us(TIMEOUT_US_1S);

	VERBOSE("lan966x: %s\n", __func__);
	memset(cdc, 0x0, sizeof(struct cdc));
	memset(req, 0x0, sizeof(union usb_request));
	cdc->base = LAN966X_UDPHS0_BASE;
	cdc->ept_fifos = LAN966X_USB_BASE;
	cdc->cpubase = LAN966X_CPU_BASE;
	cdc->setup = req;
	cdc->current_configuration = 0;
	cdc->current_connection = 0;
	cdc->set_line = 0;
	lan966x_apply_trim(cdc->cpubase, trim);
	if (lan966x_usb_epts_up(&setup_cdc)) {
		VERBOSE("lan966x: %s: epts are up\n", __func__);
		setup_cdc.current_configuration = 1;
		setup_cdc.current_connection = 1;
		setup_cdc.set_line = 1;
	} else {
		VERBOSE("lan966x: %s: setup device\n", __func__);
		lan966x_usb_device_init(&setup_cdc);
	}

	while (!setup_cdc.current_connection || !setup_cdc.set_line) {
		lan966x_usb_is_configured(&setup_cdc);
		if (timeout_elapsed(timeout)) {
			VERBOSE("lan966x: %s, timeout\n", __func__);
			goto byebye;
		}
	}
byebye:
	VERBOSE("lan966x: %s done!\n", __func__);
}
