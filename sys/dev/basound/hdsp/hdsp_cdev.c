// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * hdsp_cdev.c — /dev/hdspN character device for the RME HDSP driver
 *
 * Exposes a simple ioctl interface that a FreeBSD-native hdspmixer tool
 * can use to inspect card configuration and read/write the 2048-entry
 * hardware matrix mixer.
 *
 * Interface summary (see hdsp_ioctl.h for struct definitions):
 *
 *   HDSP_IOCTL_GET_CONFIG   – card type, firmware rev, channel counts,
 *                             current sample rate
 *   HDSP_IOCTL_GET_MIXER    – snapshot of all 2048 gain values
 *   HDSP_IOCTL_SET_ENTRY    – write a single (addr, gain) pair
 *   HDSP_IOCTL_SET_MIXER    – atomically replace the full 2048-entry matrix
 *
 * Locking
 * -------
 * All ioctl handlers hold hdsp->lock for the duration of the hardware
 * access.  hdsp_write_gain() does a short busy-wait (DELAY) per entry
 * while waiting for the FIFO; in practice the FIFO is always ready and
 * the wait completes on the first poll.  For SET_MIXER (2048 writes)
 * this is still O(ms) and acceptable for an infrequent setup operation.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mutex.h>

#include "hdsp.h"
#include "hdsp_ioctl.h"

/* ------------------------------------------------------------------ */
/* cdevsw callbacks                                                     */
/* ------------------------------------------------------------------ */

static d_open_t  hdsp_cdev_open;
static d_close_t hdsp_cdev_close;
static d_ioctl_t hdsp_cdev_ioctl;

static struct cdevsw hdsp_cdevsw = {
	.d_version = D_VERSION,
	.d_open    = hdsp_cdev_open,
	.d_close   = hdsp_cdev_close,
	.d_ioctl   = hdsp_cdev_ioctl,
	.d_name    = "hdsp",
};

static int
hdsp_cdev_open(struct cdev *dev __unused, int flags __unused,
    int fmt __unused, struct thread *td __unused)
{
	return 0;
}

static int
hdsp_cdev_close(struct cdev *dev __unused, int flags __unused,
    int fmt __unused, struct thread *td __unused)
{
	return 0;
}

static int
hdsp_cdev_ioctl(struct cdev *dev, u_long cmd, caddr_t data,
    int flags __unused, struct thread *td __unused)
{
	struct hdsp *hdsp = dev->si_drv1;
	int i, err;

	if (hdsp == NULL)
		return (ENXIO);

	switch (cmd) {

	/* ---- read card configuration ---------------------------------- */
	case HDSP_IOCTL_GET_CONFIG: {
		struct hdsp_config_info *info = (struct hdsp_config_info *)data;

		mtx_lock(&hdsp->lock);
		info->io_type            = (uint8_t)hdsp->io_type;
		info->firmware_rev       = (uint8_t)hdsp->firmware_rev;
		info->max_channels       = hdsp->max_channels;
		info->ss_in_channels     = hdsp->ss_in_channels;
		info->ss_out_channels    = hdsp->ss_out_channels;
		info->system_sample_rate = (uint32_t)hdsp->system_sample_rate;
		mtx_unlock(&hdsp->lock);
		return (0);
	}

	/* ---- read full mixer matrix snapshot -------------------------- */
	case HDSP_IOCTL_GET_MIXER: {
		struct hdsp_mixer_ioctl *mx = (struct hdsp_mixer_ioctl *)data;

		mtx_lock(&hdsp->lock);
		memcpy(mx->matrix, hdsp->mixer_matrix,
		    HDSP_MATRIX_MIXER_SIZE * sizeof(uint16_t));
		mtx_unlock(&hdsp->lock);
		return (0);
	}

	/* ---- write a single matrix entry ------------------------------ */
	case HDSP_IOCTL_SET_ENTRY: {
		struct hdsp_mixer_entry *e = (struct hdsp_mixer_entry *)data;

		if (e->addr >= HDSP_MATRIX_MIXER_SIZE)
			return (EINVAL);

		mtx_lock(&hdsp->lock);
		err = hdsp_write_gain(hdsp, e->addr, e->gain);
		mtx_unlock(&hdsp->lock);

		return (err ? EIO : 0);
	}

	/* ---- replace the full matrix ---------------------------------- */
	case HDSP_IOCTL_SET_MIXER: {
		struct hdsp_mixer_ioctl *mx = (struct hdsp_mixer_ioctl *)data;

		mtx_lock(&hdsp->lock);
		for (i = 0; i < HDSP_MATRIX_MIXER_SIZE; ++i) {
			if (hdsp_write_gain(hdsp, i, mx->matrix[i]) < 0) {
				mtx_unlock(&hdsp->lock);
				dev_err(hdsp->card->dev,
				    "SET_MIXER: FIFO timeout at entry %d\n", i);
				return (EIO);
			}
		}
		mtx_unlock(&hdsp->lock);
		return (0);
	}

	default:
		return (ENOTTY);
	}
}

/* ------------------------------------------------------------------ */
/* Lifecycle — called from hdsp_bsd.c attach / detach                 */
/* ------------------------------------------------------------------ */

int
hdsp_cdev_create(struct hdsp *hdsp, int unit)
{
	hdsp->cdev = make_dev(&hdsp_cdevsw, unit,
	    UID_ROOT, GID_WHEEL, 0660, "hdsp%d", unit);
	if (hdsp->cdev == NULL)
		return (ENOMEM);

	hdsp->cdev->si_drv1 = hdsp;

	dev_info(hdsp->card->dev, "mixer device: /dev/hdsp%d\n", unit);
	return (0);
}

void
hdsp_cdev_destroy(struct hdsp *hdsp)
{
	if (hdsp->cdev != NULL) {
		destroy_dev(hdsp->cdev);
		hdsp->cdev = NULL;
	}
}
