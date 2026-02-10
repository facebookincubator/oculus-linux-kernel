# HzOS Ext

HzOS Ext is a Linux kernel subsystem that allows HzOS to implement custom kernel
policies in an easy and extensible way. Why is this necessary when the Linux kernel
has 'capabilities' or other upstream hooks such as LSM? There are a few reasons:

1. Capabilities are "UAPI", which means that they are 100% always ABI backwards
   compatible indefinitely, and there is therefore a large swath of user space
   code that codes based off of these capabilities. For example, Android init
   hard codes all of the capabilities as part of its logic for parsing a
   service's capabilities from its init.rc file.

2. We extend the kernel in various ways that are not present upstream (for
   example, with the SHARED_RUNQ feature), and we can't always rely on upstream
   hooks for implementing policies. For example, the security_task_setscheduler()
   LSM hook only takes in the task rather than also the policy being updated.

3. Sometimes this lets us apply the principles of MAC (Mandatory Access
   Control) with a finer tooth comb than what SELinux allows. For example, we
   currently rely on CAP_SYS_ADMIN to toggle whether EAS is enabled. We could
   instead add a new Meta hzos_ext flag that the perf service could specify,
   thus allowing it to no longer require CAP_SYS_ADMIN (which it currently
   requires purely for that reason).

## Isn't this just a shadow system outside of SELinux?

Any security extensions are, maybe. But we have no other choice unless we want
to make significant investments in upstreaming changes to SELinux or the rest
of the kernel (and sometimes hzos_ext can be a precursor to that). The SELinux
ecosystem is extensive (there is the Object Manager, Security Server, etc), and
it has baked in assumptions about the objects that can be managed, along with a
reliance on traditional Linux capabilities such as CAP_SYS_ADMIN. Any changes we
make to it to implement finer-grained policies like this would also never make
it past upstream review.

There are of course some alternatives for implementing LSM-like functionality,
such as using BPF LSM (unfortunately not available on 4.19). Regardless of
which LSM we choose, however, the reality is that _any_ LSM that is not SELinux
is going to be something that sits to the "side" relative to the overall
Android security strategy, which entirely relies on SELinux [0] to implement
its Mandatory Access Control (MAC) policies. As described in [1], Android
leverages SELinux by using a large set of policy files. Some of those policy
files are core and public, and define domains, etc that can be used by the rest
of the system. Others are core and private, and others are public and specified
per-device, etc. The point is that the entire ecosystem is built around
SELinux, and a well-defined SELinux policy file architecture.

[0]: https://source.android.com/docs/security/features/selinux
[1]: https://source.android.com/docs/security/features/selinux/implement

So why do we think this is justified? The hypothesis is formed from the
following set of claims:

1. Changing SELinux to understand Meta-specific security policy is both
   prohibitively expensive in terms of engineering investment, and also
   very unlikely to be upstreamed.

2. We will need to update the kernel indefinitely and on an ongoing basis.
   By having a system that lets us make changes to the kernel by leveraging
   GKI hooks, we will have a substantially lower maintenance burden in the long
   run.

# HzOS Ext flags

HzOS Ext defines a set of flags that may be written to a process via procfs.

## Enabling and disabling flags

Flags can be both enabled, or disabled, by echoing the flag to the specified
thread's procfs file. For example, we could enable RT for the vrdevicemanager
service as follows:

```
eureka:/ # cd /proc/1017/task/1017
eureka:/proc/1017/task/1017 # ls
attr    clear_refs  cpuset   exe     io        maps        mountinfo  ns         oom_score_adj  root       sessionid     stack  status         wchan
auxv    cmdline     cwd      fd      limits    mem         mounts     oom_adj    pagemap        sched      smaps         stat   syscall
cgroup  comm        environ  fdinfo  loginuid  hzos_ext_flags  net        oom_score  personality    schedstat  smaps_rollup  statm  time_in_state
eureka:/proc/1017/task/1017 # cat comm
vrdevicemanager
eureka:/proc/1017/task/1017 # cat hzos_ext_flags

eureka:/proc/1017/task/1017 # echo ALLOW_RT > hzos_ext_flags
```

Of course, once flags have been written to the file, you can query them via
`cat` as usual:

```
eureka:/proc/1017/task/1017 # cat hzos_ext_flags
ALLOW_RT
```

We can also disable flags by prefixing the flag with NO_ before we write it to
the `hzos_ext_flags` procfs file:

```
eureka:/proc/1017/task/1017 # cat hzos_ext_flags
ALLOW_RT
eureka:/proc/1017/task/1017 #
eureka:/proc/1017/task/1017 # echo NO_ALLOW_RT > hzos_ext_flags
eureka:/proc/1017/task/1017 # cat hzos_ext_flags
NO_ALLOW_RT
```

## Supported flags

Let's discuss each of the flags we currently support with HzOS Ext.

### ALLOW_RT

Traditionally in Linux, the CAP_SYS_NICE capability has been used to configure
whether a process is allowed to run with RT. Unfortunately, CAP_SYS_NICE also
comes with some additional capabilities:

- Allowing the process to set the scheduler policy for other processes
- Allowing the process to take a negative niceness value

Of course, none of these capabilities need to overlap. While updating a
thread's niceness to be negative does affect fairness, that is very different
from granting it the ability to be RT, which can completely bork the system.

What we want is the ability to control, separately:

1. Which threads may use a negative niceness
2. Which threads are allowed to be set as RT
3. Which threads are allowed to change other threads' scheduling policies

The ALLOW_RT flag allows us to be a bit more fine grained by preventing a
thread from becoming RT *even if it has CAP_SYS_NICE*, with the assumption
being that they should only be able to update their niceness. This enables us
to have a config-driven approach to RT, and be able to better track and reason
about when RT is being used, and why. Note that longer term we plan to add a
flag for allowing a thread to be able to update other threads' scheduling
policies as well. This will likely only be set for the Orchestrator Service.

Note that if a task is RT when this capability is removed, we do not demote
them from RT. While they technically no longer have permission to use RT, we
want to err on the side of simplicity to avoid conflicting with e.g. temporary
priority inheritance.

## Adding a new flag

Adding a new flag is simple. Notice that there is a `flags_table.h` header file
in the hzos_ext directory which contains the following, and *no ifdef guard*:

```
HZOS_EXT_FLAG(ALLOW_RT)
```

If you want to add a new flag, simply add a new entry to that list, such as:

```
HZOS_EXT_FLAG(ALLOW_RT)
HZOS_EXT_FLAG(ALLOW_SET_EAS)
```

The HzOS Ext machinery will handle generating all of the code for using that flag
file for you. In the example above, after adding your flag to the list you'd be
able to set the flag from user space by doing:

```
eureka:/proc/1017/task/1017 # echo ALLOW_SET_EAS > hzos_ext_flags
```

Similarly, you'd be able to access the flag from HzOS Ext using the
`hzos_ext_task_has_flag()` function as follows:

```
static bool can_set_eas(const struc task_struct *p)
{
    return hzos_ext_task_has_flag(p, HZOS_EXT_FLAG_ALLOW_RT);
}
```

# Contributing

If you want to add a new feature or submit a bugfix to HzOS Ext, please add
someone from the vros_optimization_oncall to the review.
