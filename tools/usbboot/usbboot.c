/*
 * Copyright (C) 2010 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/stat.h>

#include <assert.h>
#include <inttypes.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <libelf.h>
#include <libusb.h>

/* XXX - make this work! */
#if 0
#include <armv7/include/dev/omap/omap44xx_boot_dev.h>
#endif

// #include "usb.h"
//#include <usb-linux.h>
#include <omap4/boot.h>
#include <protocol.h>

#define min(a,b) (((a) < (b)) ? (a): (b))

void
fail(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(EXIT_FAILURE);
}

static void
fail_usb(const char *str, int e) {
    fprintf(stderr, "%s: %s\n", str, libusb_strerror(e));
    exit(EXIT_FAILURE);
}

static void
fail_errno(const char *fmt, ...) {
    char s[1024];

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s, 1024, fmt, ap);
    va_end(ap);

    perror(s);
    exit(EXIT_FAILURE);
}

static void
fail_elf(const char *s) {
    fprintf(stderr, "%s: %s\n", s, elf_errmsg(elf_errno()));
    exit(EXIT_FAILURE);
}

struct usb_load_chunk {
    uint32_t address;
    void *data;
    unsigned size;
};

extern unsigned char aboot_data[];
extern unsigned aboot_size;

static void
usb_write(struct libusb_device_handle *usbdev, void *data, int len) {
    while(len > 0) {
        int transferred;
        int r= libusb_bulk_transfer(usbdev, OMAP44xx_bulk_out,
                                    data, min(len, CHUNK_SIZE),
                                    &transferred, 0);
        if(r < 0) fail_usb("libusb_bulk_transfer", r);

        assert(transferred <= len);
        len-=  transferred;
        data+= transferred;
    }
}

static void
usb_read(struct libusb_device_handle *usbdev, void *data, int len) {
    while(len > 0) {
        int transferred;
        int r= libusb_bulk_transfer(usbdev, OMAP44xx_bulk_in,
                                    data, min(len, CHUNK_SIZE),
                                    &transferred, 0);
        if(r < 0) fail_usb("libusb_bulk_transfer", r);

        assert(transferred <= len);
        len-=  transferred;
        data+= transferred;
    }
}

void
send_word(libusb_device_handle *usb, uint32_t msg) {
    usb_write(usb, &msg, sizeof(msg));
}

int
usb_boot(libusb_device_handle *usb, void *image_data,
         size_t image_size, uint32_t load_address) {
    uint32_t msg;

    fprintf(stderr,"Reading ASIC ID\n");
    send_word(usb, OMAP44xx_bootmsg_getid);

    struct omap44xx_id id;
    usb_read(usb, &id, sizeof(id));

    assert(id.items == 5);
    assert(id.id.subblock_id = 0x01);
    assert(id.checksum.subblock_id = 0x15);

    printf("Chip reports itself to be an OMAP%02x%02x\n",
           id.id.device[0], id.id.device[1]);

    if(id.id.ch == OMAP44xx_ch_enabled)
        printf("Configuration header (CH) loading enabled.\n");
    else if(id.id.ch == OMAP44xx_ch_disabled)
        printf("Configuration header (CH) loading disabled.\n");
    else
        printf("Unrecognised or corrupted CH setting: %02x\n", id.id.ch);

    printf("ROM revision %02x\n", id.id.rom_revision);
    printf("ROM CRC: %02x%02x%02x%02x\n",
           id.checksum.rom_crc[0], id.checksum.rom_crc[1],
           id.checksum.rom_crc[2], id.checksum.rom_crc[3]);

    printf("Sending second stage bootloader... \n");
    send_word(usb, OMAP44xx_bootmsg_periphboot);
    usleep(1);
    send_word(usb, aboot_size);
    usleep(1);
    usb_write(usb, aboot_data, aboot_size);

    // sleep to make stuff work
    sleep(1);

    msg = 0;
    printf("Waiting for second stage response...\n");
    usb_read(usb, &msg, sizeof(msg));

    printf("Response is \"%x\"\n", msg);
    if (msg != ABOOT_IS_READY) fail("Unexpected second stage response\n");

    usleep(500);

    printf("Sending size = %zu, ", image_size);
    send_word(usb, image_size);

    printf("Sending address = 0x%08X, ", load_address);
    send_word(usb, load_address);

    struct timespec start, end;
    printf("Sending image... ");
    fflush(stdout);
    if(clock_gettime(CLOCK_REALTIME, &start)) fail_errno("clock_gettime");
    usb_write(usb, image_data, image_size);
    if(clock_gettime(CLOCK_REALTIME, &end)) fail_errno("clock_gettime");
    printf("done.\n");

    double tstart= start.tv_sec + start.tv_nsec * 1e-9;
    double tend=   end.tv_sec   + end.tv_nsec   * 1e-9;
    double elapsed= tend - tstart;

    printf("Transferred %zuB in %.2fs at %.2fMB/s\n",
           image_size, elapsed, (image_size / elapsed) / 1024 / 1024);

    sleep(1);

    printf("Starting chunk at 0x%"PRIx32"\n", load_address);
    send_word(usb, ABOOT_NO_MORE_DATA);
    
    return 0;
}

void *
load_file(const char *file, size_t *sz, uint32_t *load_address) {
    int fd= open(file, O_RDONLY);
    if(fd < 0) fail_errno("open");

    struct stat stat;
    if(fstat(fd, &stat)) fail_errno("fstat");
    size_t elfsize= stat.st_size;

    void *elfdata= malloc(elfsize);
    if(!elfdata) fail_errno("malloc");

    /* Read the raw file data. */
    {
        size_t to_read= elfsize;
        do {
            size_t bytes_read= read(fd, elfdata, to_read);
            if(bytes_read < 0) fail_errno("read");
            assert(bytes_read <= to_read);
            to_read -= bytes_read;
        } while(to_read > 0);
    }

    Elf *elf= elf_memory(elfdata, elfsize);
    if(!elf) fail_elf("elf_begin");

    const char *elf_ident= elf_getident(elf, NULL);
    if(!elf_ident) fail_elf("elf_getident");

    if(elf_ident[EI_CLASS] != ELFCLASS32 ||
       elf_ident[EI_DATA] != ELFDATA2LSB) {
        fail("Not a 32-bit little-endian image.\n");
    }

    Elf32_Ehdr *ehdr= elf32_getehdr(elf);
    if(!ehdr) fail_elf("elf32_getehdr");

    if(ehdr->e_type != ET_EXEC) fail("Not an executable.\n");
    if(ehdr->e_machine != EM_ARM) fail("Not an ARM binary.\n");

    if(ehdr->e_phnum == 0) fail("No loadable segment.\n");
    if(ehdr->e_phnum  > 1) fail("More than one loadable segment.\n");

    Elf32_Phdr *phdr= elf32_getphdr(elf);
    if(!phdr) fail_elf("elf32_getphdr");

    printf("Loadable segment at offset %08x, size %u\n",
           phdr->p_offset, phdr->p_filesz);
    printf("Load address %08x, loaded size %u\n",
           phdr->p_vaddr, phdr->p_memsz);

    void *image_base= elfdata + phdr->p_offset;
    *sz= phdr->p_filesz;
    *load_address= phdr->p_vaddr;

    if(elf_end(elf)) fail_elf("elf_end");

    return image_base;
}

int main(int argc, char **argv)
{
    struct libusb_context *usb;
    struct libusb_device_handle *usbdev;
    void *image_data;
    size_t image_size;
    uint32_t load_address;
    int r;

    if(elf_version(EV_CURRENT) == EV_NONE)
        fail("ELF library version out of date");;

    if (argc < 2)fail("usage: %s <image>\n", argv[0]);
    image_data= load_file(argv[1], &image_size, &load_address);

    r = libusb_init(&usb);
    if(r) fail_usb("libusb_init", r);
    libusb_set_debug(usb, LIBUSB_LOG_LEVEL_WARNING);

    int once= 1;
    for (;;) {
        usbdev= libusb_open_device_with_vid_pid(usb,
                    OMAP44xx_vid, OMAP44xx_pid);
        if(usbdev) {
            r= libusb_reset_device(usbdev);
            if(r) fail_usb("libusb_reset_device", r);

            r= libusb_set_auto_detach_kernel_driver(usbdev, 1);
            if(r) fail_usb("libusb_detach_kernel_driver", r);

            r = libusb_set_configuration(usbdev, 1);
            if(r) fail_usb("libusb_set_configuration", r);

            r = libusb_claim_interface(usbdev, 0);
            if(r) fail_usb("libusb_claim_interface", r);

            struct libusb_device *dev= libusb_get_device(usbdev);
            int speed= libusb_get_device_speed(dev);
            if(speed < 0) fail_usb("libusb_get_device_speed", speed);
            printf("Connected at ");
            switch(speed) {
                case LIBUSB_SPEED_LOW:
                    printf("1.5Mb/s\n");
                    break;
                case LIBUSB_SPEED_FULL:
                    printf("12Mb/s.\n");
                    break;
                case LIBUSB_SPEED_HIGH:
                    printf("480Mb/s.\n");
                    break;
                case LIBUSB_SPEED_SUPER:
                    printf("5000Mb/s.\n");
                    break;
                default:
                    printf("unknown speed.\n");
                    break;
            }
        }

        if (r == 0 && usbdev) {
            r = usb_boot(usbdev, image_data, image_size, load_address);
            libusb_release_interface(usbdev, 0);
            libusb_close(usbdev);
            break;
        }

        if (once) {
            once = 0;
            fprintf(stderr,"Waiting for OMAP44xx device...\n");
        }

        usleep(250);
    }

    libusb_exit(usb);

    return r;
}
