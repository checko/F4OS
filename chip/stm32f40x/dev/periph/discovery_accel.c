#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <mm/mm.h>
#include <kernel/semaphore.h>
#include <kernel/sched.h>
#include <kernel/fault.h>
#include <dev/resource.h>
#include <dev/registers.h>
#include <dev/hw/gpio.h>

#include <dev/hw/spi.h>
#include <dev/periph/discovery_accel.h>

/* When this bit is set, the register address on the accelerometer
 * will automatically increment for multi-byte reads/writes */
#define ADDR_INC (1 << 6)

typedef struct discovery_accel {
        struct spi_port *spi_port;
        struct spi_dev spi_dev;
        uint8_t read_ctr;
        uint8_t data[3];
} discovery_accel;

static char discovery_accel_read(void *env, int *error) __attribute__((section(".kernel")));
static int discovery_accel_write(char d, void *env) __attribute__((section(".kernel")));
static int discovery_accel_close(resource *env) __attribute__((section(".kernel")));
static void discovery_accel_cs_high(void) __attribute__((section(".kernel")));
static void discovery_accel_cs_low(void) __attribute__((section(".kernel")));

static void discovery_accel_cs_high(void) {
    *GPIO_ODR(GPIOE) |= GPIO_ODR_PIN(3);
}

static void discovery_accel_cs_low(void) {
    *GPIO_ODR(GPIOE) &= ~(GPIO_ODR_PIN(3));
}

rd_t open_discovery_accel(void) {
    /* --- Set up CS pin and set high ---*/
    *RCC_AHB1ENR |= RCC_AHB1ENR_GPIOEEN;

    /* PE3 */
    gpio_moder(GPIOE, 3, GPIO_MODER_OUT);
    gpio_otyper(GPIOE, 3, GPIO_OTYPER_PP);
    gpio_pupdr(GPIOE, 3, GPIO_PUPDR_NONE);
    gpio_ospeedr(GPIOE, 3, GPIO_OSPEEDR_50M);
    
    /* ------ Properly idle CS */
    discovery_accel_cs_high();

    discovery_accel *accel = kmalloc(sizeof(discovery_accel));
    if (!accel) {
        printk("OOPS: Could not allocate space for discovery accel resource.\r\n");
        return -1;
    }

    resource *new_r = create_new_resource();
    if(!new_r) {
        printk("OOPS: Could not allocate space for discovery accel resource.\r\n");
        kfree(accel);
        return -1;
    }

    accel->spi_port = &spi1;
    if (!accel->spi_port->ready) {
        accel->spi_port->init();
    }

    accel->spi_dev.cs_high = &discovery_accel_cs_high;
    accel->spi_dev.cs_low = &discovery_accel_cs_low;

    /* Active mode, XYZ enable */
    uint8_t data = 0x47;
    if (spi_write(accel->spi_port, &accel->spi_dev, 0x20, &data, 1) != 1) {
        /* Unable to activate :( */
        kfree(accel);
        kfree(new_r);
        return -1;
    }

    accel->read_ctr = 0;

    new_r->env = accel;
    new_r->writer = &discovery_accel_write;
    new_r->reader = &discovery_accel_read;
    new_r->closer = &discovery_accel_close;
    new_r->sem = &spi1_semaphore;

    return add_resource(curr_task->task, new_r);
}

char discovery_accel_read(void *env, int *error) {
    if (error != NULL) {
        *error = 0;
    }

    if (env == NULL) {
        if (error != NULL) {
            *error = -1;
        }
        return 0;
    }

    discovery_accel *accel = (discovery_accel *)env;

    if (accel->read_ctr == 0) {
        uint8_t data[5];
        if (spi_read(accel->spi_port, &accel->spi_dev, 0x29 | ADDR_INC, data, 5) != 5) {
            if (error != NULL) {
                *error = -1;
            }
            return 0;
        }

        accel->data[0] = data[0];
        accel->data[1] = data[2];
        accel->data[2] = data[4];
    }

    char ret = (char) accel->data[accel->read_ctr++];

    if(accel->read_ctr > 2) {
        accel->read_ctr = 0;
    }

    return ret;
}

int discovery_accel_write(char d, void *env) {
    /* No real meaning to this yet */
    return -1;
}

int discovery_accel_close(resource *res) {
    kfree(res->env);
    return 0;
}

/* Helper function - read all data and convert to G */
int read_discovery_accel(rd_t rd, struct accelerometer *accel) {
    char data[3];

    if (!accel) {
        return -1;
    }

    if (read(rd, data, 3) != 3) {
        return -1;
    }

    accel->x = ((int8_t) data[0]) * DISCOVERY_ACCEL_SENSITIVITY;
    accel->y = ((int8_t) data[1]) * DISCOVERY_ACCEL_SENSITIVITY;
    accel->z = ((int8_t) data[2]) * DISCOVERY_ACCEL_SENSITIVITY;

    return 0;
}
