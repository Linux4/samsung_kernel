#include <kunit/test.h>

#define wake_lock_init(args...)		((void *)0)

struct scsc_mx *test_alloc_scscmx(struct kunit *test, struct scsc_mif_abs *mif)
{
	struct scsc_mx *mx;

	mx = kunit_kzalloc(test, sizeof(*mx), GFP_KERNEL);

	mx->mif_abs = mif;

	return mx;
}

void set_srvman(struct scsc_mx *scscmx, struct srvman srvman)
{
	memcpy(&scscmx->srvman, &srvman, sizeof(struct srvman));
}

void set_mxman(struct scsc_mx *scscmx, struct mxman *mxman)
{
	memcpy(&scscmx->mxman, mxman, sizeof(struct mxman));
}

void set_mxlog(struct scsc_mx *scscmx, struct mxlog *mxlog)
{
	memcpy(&scscmx->mxlog, mxlog, sizeof(struct mxlog));
}
