#include <linux/firmware.h>
#include <sound/core.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#include <sys/uio.h>

MALLOC_DECLARE(M_ALSA);

int
request_firmware(const struct firmware **fw_p, const char *name, struct device *device)
{
	struct firmware *fw;
	struct nameidata nd;
	struct vattr va;
	int error;
	char path[MAXPATHLEN];

	snprintf(path, sizeof(path), "/boot/firmware/%s", name);

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, path);
	error = vn_open(&nd, &((int){FREAD}), 0, NULL);
	if (error) {
		/* Try current directory as fallback */
		snprintf(path, sizeof(path), "./%s", name);
		NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, path);
		error = vn_open(&nd, &((int){FREAD}), 0, NULL);
		if (error)
			return -error;
	}

	error = VOP_GETATTR(nd.ni_vp, &va, curthread->td_ucred);
	if (error) {
		VOP_UNLOCK(nd.ni_vp);
		vn_close(nd.ni_vp, FREAD, curthread->td_ucred, curthread);
		return -error;
	}

	fw = malloc(sizeof(*fw) + va.va_size, M_ALSA, M_WAITOK | M_ZERO);
	fw->size = va.va_size;
	fw->data = (uint8_t *)(fw + 1);

	error = vn_rdwr(UIO_READ, nd.ni_vp, fw->data, fw->size, 0,
			UIO_SYSSPACE, IO_UNIT, curthread->td_ucred, NOCRED,
			NULL, curthread);
	
	VOP_UNLOCK(nd.ni_vp);
	vn_close(nd.ni_vp, FREAD, curthread->td_ucred, curthread);

	if (error) {
		free(fw, M_ALSA);
		return -error;
	}

	*fw_p = fw;
	return 0;
}

void
release_firmware(const struct firmware *fw)
{
	if (fw)
		free(__DECONST(void *, fw), M_ALSA);
}
