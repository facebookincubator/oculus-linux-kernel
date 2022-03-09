#ifndef __BACKPORT_NET_GENETLINK_H
#define __BACKPORT_NET_GENETLINK_H
#include_next <net/genetlink.h>
#include <linux/version.h>

static inline void __bp_genl_info_userhdr_set(struct genl_info *info,
					      void *userhdr)
{
	info->userhdr = userhdr;
}

static inline void *__bp_genl_info_userhdr(struct genl_info *info)
{
	return info->userhdr;
}

#if LINUX_VERSION_IS_LESS(4,12,0)
#define GENL_SET_ERR_MSG(info, msg) do { } while (0)

static inline int genl_err_attr(struct genl_info *info, int err,
				struct nlattr *attr)
{
#if LINUX_VERSION_IS_GEQ(4,12,0)
	info->extack->bad_attr = attr;
#endif

	return err;
}
#endif

/* this is for patches we apply */
static inline struct netlink_ext_ack *genl_info_extack(struct genl_info *info)
{
#if LINUX_VERSION_IS_GEQ(4,12,0)
	return info->extack;
#else
	return info->userhdr;
#endif
}

/* this gets put in place of info->userhdr, since we use that above */
static inline void *genl_info_userhdr(struct genl_info *info)
{
	return (u8 *)info->genlhdr + GENL_HDRLEN;
}

/* this is for patches we apply */
#if LINUX_VERSION_IS_LESS(3,7,0)
#define genl_info_snd_portid(__genl_info) (__genl_info->snd_pid)
#else
#define genl_info_snd_portid(__genl_info) (__genl_info->snd_portid)
#endif

#ifndef GENLMSG_DEFAULT_SIZE
#define GENLMSG_DEFAULT_SIZE (NLMSG_DEFAULT_SIZE - GENL_HDRLEN)
#endif

#if LINUX_VERSION_IS_LESS(3,1,0)
#define genl_dump_check_consistent(cb, user_hdr)
#endif

#if LINUX_VERSION_IS_LESS(4,15,0)
#ifndef genl_dump_check_consistent
static inline
void backport_genl_dump_check_consistent(struct netlink_callback *cb,
					 void *user_hdr)
{
	struct genl_family dummy_family = {
		.hdrsize = 0,
	};

	genl_dump_check_consistent(cb, user_hdr, &dummy_family);
}
#define genl_dump_check_consistent LINUX_BACKPORT(genl_dump_check_consistent)
#endif
#endif /* LINUX_VERSION_IS_LESS(4,15,0) */

#if LINUX_VERSION_IS_LESS(3,13,0) && RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,0)
static inline int __real_genl_register_family(struct genl_family *family)
{
	return genl_register_family(family);
}

/* Needed for the mcgrps pointer */
struct backport_genl_family {
	struct genl_family family;

	unsigned int id, hdrsize, version, maxattr;
	char name[GENL_NAMSIZ];
	bool netnsok;
	bool parallel_ops;

	struct nlattr **attrbuf;

	int (*pre_doit)(struct genl_ops *ops, struct sk_buff *skb,
			struct genl_info *info);

	void (*post_doit)(struct genl_ops *ops, struct sk_buff *skb,
			  struct genl_info *info);

	struct genl_multicast_group *mcgrps;
	struct genl_ops *ops;
	unsigned int n_mcgrps, n_ops;

	struct module *module;
};
#define genl_family LINUX_BACKPORT(genl_family)

int __backport_genl_register_family(struct genl_family *family);

#define genl_register_family LINUX_BACKPORT(genl_register_family)
static inline int
genl_register_family(struct genl_family *family)
{
	family->module = THIS_MODULE;
	return __backport_genl_register_family(family);
}

#define _genl_register_family_with_ops_grps \
	_backport_genl_register_family_with_ops_grps
static inline int
_genl_register_family_with_ops_grps(struct genl_family *family,
				    struct genl_ops *ops, size_t n_ops,
				    struct genl_multicast_group *mcgrps,
				    size_t n_mcgrps)
{
	family->ops = ops;
	family->n_ops = n_ops;
	family->mcgrps = mcgrps;
	family->n_mcgrps = n_mcgrps;
	return genl_register_family(family);
}

#define genl_register_family_with_ops(family, ops)			\
	_genl_register_family_with_ops_grps((family),			\
					    (ops), ARRAY_SIZE(ops),	\
					    NULL, 0)
#define genl_register_family_with_ops_groups(family, ops, grps)		\
	_genl_register_family_with_ops_grps((family),			\
					    (ops), ARRAY_SIZE(ops),	\
					    (grps), ARRAY_SIZE(grps))

#define genl_unregister_family backport_genl_unregister_family
int genl_unregister_family(struct genl_family *family);

#if LINUX_VERSION_IS_LESS(3,3,0)
extern void genl_notify(struct sk_buff *skb, struct net *net, u32 pid,
			u32 group, struct nlmsghdr *nlh, gfp_t flags);
#endif
#define genl_notify(_fam, _skb, _info, _group, _flags)			\
	genl_notify(_skb, genl_info_net(_info),				\
		    genl_info_snd_portid(_info),			\
		    (_fam)->mcgrps[_group].id, _info->nlhdr, _flags)
#define genlmsg_put(_skb, _pid, _seq, _fam, _flags, _cmd)		\
	genlmsg_put(_skb, _pid, _seq, &(_fam)->family, _flags, _cmd)
#define genlmsg_nlhdr(_hdr, _fam)					\
	genlmsg_nlhdr(_hdr, &(_fam)->family)

#ifndef genlmsg_put_reply /* might already be there from _info override above */
#define genlmsg_put_reply(_skb, _info, _fam, _flags, _cmd)		\
	genlmsg_put_reply(_skb, _info, &(_fam)->family, _flags, _cmd)
#endif
#define genlmsg_multicast_netns LINUX_BACKPORT(genlmsg_multicast_netns)
static inline int genlmsg_multicast_netns(struct genl_family *family,
					  struct net *net, struct sk_buff *skb,
					  u32 portid, unsigned int group,
					  gfp_t flags)
{
	if (WARN_ON_ONCE(group >= family->n_mcgrps))
		return -EINVAL;
	group = family->mcgrps[group].id;
	return nlmsg_multicast(
		net->genl_sock,
		skb, portid, group, flags);
}
#define genlmsg_multicast LINUX_BACKPORT(genlmsg_multicast)
static inline int genlmsg_multicast(struct genl_family *family,
				    struct sk_buff *skb, u32 portid,
				    unsigned int group, gfp_t flags)
{
	if (WARN_ON_ONCE(group >= family->n_mcgrps))
		return -EINVAL;
	group = family->mcgrps[group].id;
	return nlmsg_multicast(
		init_net.genl_sock,
		skb, portid, group, flags);
}
static inline int
backport_genlmsg_multicast_allns(struct genl_family *family,
				 struct sk_buff *skb, u32 portid,
				 unsigned int group, gfp_t flags)
{
	if (WARN_ON_ONCE(group >= family->n_mcgrps))
		return -EINVAL;
	group = family->mcgrps[group].id;
	return genlmsg_multicast_allns(skb, portid, group, flags);
}
#define genlmsg_multicast_allns LINUX_BACKPORT(genlmsg_multicast_allns)

#define __genl_const
#else /* < 3.13 */
#define __genl_const const
#if LINUX_VERSION_IS_LESS(4,4,0)
#define genl_notify(_fam, _skb, _info, _group, _flags)			\
	genl_notify(_fam, _skb, genl_info_net(_info),			\
		    genl_info_snd_portid(_info),			\
		    _group, _info->nlhdr, _flags)
#endif /* < 4.4 */
#endif /* < 3.13 */

#if LINUX_VERSION_IS_LESS(4,10,0)
/**
 * genl_family_attrbuf - return family's attrbuf
 * @family: the family
 *
 * Return the family's attrbuf, while validating that it's
 * actually valid to access it.
 *
 * You cannot use this function with a family that has parallel_ops
 * and you can only use it within (pre/post) doit/dumpit callbacks.
 */
#define genl_family_attrbuf LINUX_BACKPORT(genl_family_attrbuf)
static inline struct nlattr **genl_family_attrbuf(struct genl_family *family)
{
	WARN_ON(family->parallel_ops);

	return family->attrbuf;
}

#define __genl_ro_after_init
#else
#define __genl_ro_after_init __ro_after_init
#endif

#if LINUX_VERSION_IS_LESS(4,12,0)
static inline int
__real_bp_extack_genl_register_family(struct genl_family *family)
{
	return genl_register_family(family);
}
static inline int
__real_bp_extack_genl_unregister_family(struct genl_family *family)
{
	return genl_unregister_family(family);
}
int bp_extack_genl_register_family(struct genl_family *family);
int bp_extack_genl_unregister_family(struct genl_family *family);
#undef genl_register_family
#define genl_register_family bp_extack_genl_register_family
#undef genl_unregister_family
#define genl_unregister_family bp_extack_genl_unregister_family
#endif

#endif /* __BACKPORT_NET_GENETLINK_H */
