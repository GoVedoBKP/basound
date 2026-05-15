#include <sys/param.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/systm.h>

static int
basound_modevent(module_t mod, int type, void *data)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		printf("basound: ALSA glue layer loaded\n");
		break;
	case MOD_UNLOAD:
		printf("basound: ALSA glue layer unloaded\n");
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}

	return (error);
}

static moduledata_t basound_mod = {
	"basound",
	basound_modevent,
	NULL
};

DECLARE_MODULE(basound, basound_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
MODULE_VERSION(basound, 1);
