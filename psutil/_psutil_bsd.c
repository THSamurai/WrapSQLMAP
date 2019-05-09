/*
 * Copyright (c) 2009, Giampaolo Rodola', Landry Breuil (OpenBSD).
 * All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Platform-specific module methods for FreeBSD and OpenBSD.

 * OpenBSD references:
 * - OpenBSD source code: http://anoncvs.spacehopper.org/openbsd-src/
 *
 * OpenBSD / NetBSD: missing APIs compared to FreeBSD implementation:
 * - psutil.net_connections()
 * - psutil.Process.get/set_cpu_affinity()  (not supported natively)
 * - psutil.Process.memory_maps()
 */

#if defined(__NetBSD__)
#define _KMEMUSER
#endif

#include <Python.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <paths.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <net/route.h>
#include <sys/socketvar.h>    // for struct xsocket
#include <sys/un.h>
#include <sys/unpcb.h>
// for xinpcb struct
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/tcp.h>
#include <netinet/tcp_timer.h>
#include <netinet/ip_var.h>
#include <netinet/tcp_var.h>   // for struct xtcpcb
#include <netinet/tcp_fsm.h>   // for TCP connection states
#include <arpa/inet.h>         // for inet_ntop()

#include <sys/mount.h>

#include <net/if.h>       // net io counters
#include <net/if_dl.h>
#include <net/route.h>

#include <netinet/in.h>   // process open files/connections
#include <sys/un.h>

#include "_psutil_bsd.h"
#include "_psutil_common.h"

#ifdef __FreeBSD__
    #include "arch/bsd/freebsd.h"
    #include "arch/bsd/freebsd_socks.h"
#elif __OpenBSD__
    #include "arch/bsd/openbsd.h"
#elif __NetBSD__
    #include "arch/bsd/netbsd.h"
    #include "arch/bsd/netbsd_socks.h"
#endif

#ifdef __FreeBSD__
    #include <net/if_media.h>
    #include <devstat.h>  // get io counters
    #include <libutil.h>  // process open files, shared libs (kinfo_getvmmap)
    #if __FreeBSD_version < 900000
        #include <utmp.h>  // system users
    #else
        #include <utmpx.h>
    #endif
#endif

#ifdef __OpenBSD__
    #include <utmp.h>
    #include <sys/vnode.h>  // for VREG
    #define _KERNEL  // for DTYPE_VNODE
    #include <sys/file.h>
    #undef _KERNEL
    #include <sys/sched.h>  // for CPUSTATES & CP_*
#endif

#if defined(__NetBSD__)
    #include <utmpx.h>
    #include <sys/vnode.h>  // for VREG
    #include <sys/sched.h>  // for CPUSTATES & CP_*
    #include <machine/vmparam.h>  // for PAGE_SHIFT
#define _KERNEL
    #include <uvm/uvm_extern.h>
#undef _KERNEL
#endif


// convert a timeval struct to a double
#define PSUTIL_TV2DOUBLE(t) ((t).tv_sec + (t).tv_usec / 1000000.0)

#ifdef __FreeBSD__
    // convert a bintime struct to milliseconds
    #define PSUTIL_BT2MSEC(bt) (bt.sec * 1000 + (((uint64_t) 1000000000 * \
                           (uint32_t) (bt.frac >> 32) ) >> 32 ) / 1000000)
#endif

#if defined(__OpenBSD__) || defined (__NetBSD__)
    #define PSUTIL_KPT2DOUBLE(t) (t ## _sec + t ## _usec / 1000000.0)
#endif


/*
 * Return a Python list of all the PIDs running on the system.
 */
static PyObject *
psutil_pids(PyObject *self, PyObject *args) {
    kinfo_proc *proclist = NULL;
    kinfo_proc *orig_address = NULL;
    size_t num_processes;
    size_t idx;
    PyObject *py_retlist = PyList_New(0);
    PyObject *py_pid = NULL;

    if (py_retlist == NULL)
        return NULL;
    if (psutil_get_proc_list(&proclist, &num_processes) != 0) {
        PyErr_SetString(PyExc_RuntimeError,
                        "failed to retrieve process list.");
        goto error;
    }

    if (num_processes > 0) {
        orig_address = proclist; // save so we can free it after we're done
        for (idx = 0; idx < num_processes; idx++) {
#ifdef __FreeBSD__
            py_pid = Py_BuildValue("i", proclist->ki_pid);
#elif defined(__OpenBSD__) || defined(__NetBSD__)
            py_pid = Py_BuildValue("i", proclist->p_pid);
#endif
            if (!py_pid)
                goto error;
            if (PyList_Append(py_retlist, py_pid))
                goto error;
            Py_DECREF(py_pid);
            proclist++;
        }
        free(orig_address);
    }

    return py_retlist;

error:
    Py_XDECREF(py_pid);
    Py_DECREF(py_retlist);
    if (orig_address != NULL)
        free(orig_address);
    return NULL;
}


/*
 * Return a Python float indicating the system boot time expressed in
 * seconds since the epoch.
 */
static PyObject *
psutil_boot_time(PyObject *self, PyObject *args) {
    // fetch sysctl "kern.boottime"
    static int request[2] = { CTL_KERN, KERN_BOOTTIME };
    struct timeval boottime;
    size_t len = sizeof(boottime);

    if (sysctl(request, 2, &boottime, &len, NULL, 0) == -1) {
        PyErr_SetFromErrno(PyExc_OSError);
        return NULL;
    }
    return Py_BuildValue("d", (double)boottime.tv_sec);
}


/*
 * Return process name from kinfo_proc as a Python string.
 */
static PyObject *
psutil_proc_name(PyObject *self, PyObject *args) {
    long pid;
    kinfo_proc kp;
    if (! PyArg_ParseTuple(args, "l", &pid))
        return NULL;
    if (psutil_kinfo_proc(pid, &kp) == -1)
        return NULL;
#ifdef __FreeBSD__
    return Py_BuildValue("s", kp.ki_comm);
#elif defined(__OpenBSD__) || defined(__NetBSD__)
    return Py_BuildValue("s", kp.p_comm);
#endif
}


/*
 * Return process cmdline as a Python list of cmdline arguments.
 */
static PyObject *
psutil_proc_cmdline(PyObject *self, PyObject *args) {
    long pid;
    PyObject *py_retlist = NULL;

    if (! PyArg_ParseTuple(args, "l", &pid))
        return NULL;

    py_retlist = psutil_get_cmdline(pid);
    // psutil_get_cmdline() returns NULL only if psutil_cmd_args
    // failed with ESRCH (no process with that PID)
    if (NULL == py_retlist)
        return PyErr_SetFromErrno(PyExc_OSError);
    return Py_BuildValue("N", py_retlist);
}


/*
 * Return process parent pid from kinfo_proc as a Python integer.
 */
static PyObject *
psutil_proc_ppid(PyObject *self, PyObject *args) {
    long pid;
    kinfo_proc kp;
    if (! PyArg_ParseTuple(args, "l", &pid))
        return NULL;
    if (psutil_kinfo_proc(pid, &kp) == -1)
        return NULL;
#ifdef __FreeBSD__
    return Py_BuildValue("l", (long)kp.ki_ppid);
#elif defined(__OpenBSD__) || defined(__NetBSD__)
    return Py_BuildValue("l", (long)kp.p_ppid);
#endif
}


/*
 * Return process status as a Python integer.
 */
static PyObject *
psutil_proc_status(PyObject *self, PyObject *args) {
    long pid;
    kinfo_proc kp;
    if (! PyArg_ParseTuple(args, "l", &pid))
        return NULL;
    if (psutil_kinfo_proc(pid, &kp) == -1)
        return NULL;
#ifdef __FreeBSD__
    return Py_BuildValue("i", (int)kp.ki_stat);
#elif defined(__OpenBSD__) || defined(__NetBSD__)
    return Py_BuildValue("i", (int)kp.p_stat);
#endif
}


/*
 * Return process real, effective and saved user ids from kinfo_proc
 * as a Python tuple.
 */
static PyObject *
psutil_proc_uids(PyObject *self, PyObject *args) {
    long pid;
    kinfo_proc kp;
    if (! PyArg_ParseTuple(args, "l", &pid))
        return NULL;
    if (psutil_kinfo_proc(pid, &kp) == -1)
        return NULL;
    return Py_BuildValue("lll",
#ifdef __FreeBSD__
                         (long)kp.ki_ruid,
                         (long)kp.ki_uid,
                         (long)kp.ki_svuid);
#elif defined(__OpenBSD__) || defined(__NetBSD__)
                         (long)kp.p_ruid,
                         (long)kp.p_uid,
                         (long)kp.p_svuid);
#endif
}


/*
 * Return process real, effective and saved group ids from kinfo_proc
 * as a Python tuple.
 */
static PyObject *
psutil_proc_gids(PyObject *self, PyObject *args) {
    long pid;
    kinfo_proc kp;
    if (! PyArg_ParseTuple(args, "l", &pid))
        return NULL;
    if (psutil_kinfo_proc(pid, &kp) == -1)
        return NULL;
    return Py_BuildValue("lll",
#ifdef __FreeBSD__
                         (long)kp.ki_rgid,
                         (long)kp.ki_groups[0],
                         (long)kp.ki_svuid);
#elif defined(__OpenBSD__) || defined(__NetBSD__)
                         (long)kp.p_rgid,
                         (long)kp.p_groups[0],
                         (long)kp.p_svuid);
#endif
}


/*
 * Return process real, effective and saved group ids from kinfo_proc
 * as a Python tuple.
 */
static PyObject *
psutil_proc_tty_nr(PyObject *self, PyObject *args) {
    long pid;
    kinfo_proc kp;
    if (! PyArg_ParseTuple(args, "l", &pid))
        return NULL;
    if (psutil_kinfo_proc(pid, &kp) == -1)
        return NULL;
#ifdef __FreeBSD__
    return Py_BuildValue("i", kp.ki_tdev);
#elif defined(__OpenBSD__) || defined(__NetBSD__)
    return Py_BuildValue("i", kp.p_tdev);
#endif
}


/*
 * Return the number of context switches performed by process as a tuple.
 */
static PyObject *
psutil_proc_num_ctx_switches(PyObject *self, PyObject *args) {
    long pid;
    kinfo_proc kp;
    if (! PyArg_ParseTuple(args, "l", &pid))
        return NULL;
    if (psutil_kinfo_proc(pid, &kp) == -1)
        return NULL;
    return Py_BuildValue("(ll)",
#ifdef __FreeBSD__
                         kp.ki_rusage.ru_nvcsw,
                         kp.ki_rusage.ru_nivcsw);
#elif defined(__OpenBSD__) || defined(__NetBSD__)
                         kp.p_uru_nvcsw,
                         kp.p_uru_nivcsw);
#endif
}


/*
 * Return a Python tuple (user_time, kernel_time)
 */
static PyObject *
psutil_proc_cpu_times(PyObject *self, PyObject *args) {
    long pid;
    double user_t, sys_t;
    kinfo_proc kp;
    if (! PyArg_ParseTuple(args, "l", &pid))
        return NULL;
    if (psutil_kinfo_proc(pid, &kp) == -1)
        return NULL;
    // convert from microseconds to seconds
#ifdef __FreeBSD__
    user_t = PSUTIL_TV2DOUBLE(kp.ki_rusage.ru_utime);
    sys_t = PSUTIL_TV2DOUBLE(kp.ki_rusage.ru_stime);
#elif defined(__OpenBSD__) || defined(__NetBSD__)
    user_t = PSUTIL_KPT2DOUBLE(kp.p_uutime);
    sys_t = PSUTIL_KPT2DOUBLE(kp.p_ustime);
#endif
    return Py_BuildValue("(dd)", user_t, sys_t);
}


/*
 * Return the number of logical CPUs in the system.
 * XXX this could be shared with OSX
 */
static PyObject *
psutil_cpu_count_logical(PyObject *self, PyObject *args) {
    int mib[2];
    int ncpu;
    size_t len;

    mib[0] = CTL_HW;
    mib[1] = HW_NCPU;
    len = sizeof(ncpu);

    if (sysctl(mib, 2, &ncpu, &len, NULL, 0) == -1)
        Py_RETURN_NONE;  // mimic os.cpu_count()
    else
        return Py_BuildValue("i", ncpu);
}


/*
 * Return a Python float indicating the process create time expressed in
 * seconds since the epoch.
 */
static PyObject *
psutil_proc_create_time(PyObject *self, PyObject *args) {
    long pid;
    kinfo_proc kp;
    if (! PyArg_ParseTuple(args, "l", &pid))
        return NULL;
    if (psutil_kinfo_proc(pid, &kp) == -1)
        return NULL;
#ifdef __FreeBSD__
    return Py_BuildValue("d", PSUTIL_TV2DOUBLE(kp.ki_start));
#elif defined(__OpenBSD__) || defined(__NetBSD__)
    return Py_BuildValue("d", PSUTIL_KPT2DOUBLE(kp.p_ustart));
#endif
}


/*
 * Return a Python float indicating the process create time expressed in
 * seconds since the epoch.
 */
static PyObject *
psutil_proc_io_counters(PyObject *self, PyObject *args) {
    long pid;
    kinfo_proc kp;
    if (! PyArg_ParseTuple(args, "l", &pid))
        return NULL;
    if (psutil_kinfo_proc(pid, &kp) == -1)
        return NULL;
    // there's apparently no way to determine bytes count, hence return -1.
    return Py_BuildValue("(llll)",
#ifdef __FreeBSD__
                         kp.ki_rusage.ru_inblock,
                         kp.ki_rusage.ru_oublock,
#elif defined(__OpenBSD__) || defined(__NetBSD__)
                         kp.p_uru_inblock,
                         kp.p_uru_oublock,
#endif
                         -1,
                         -1);
}


#if defined(__OpenBSD__) || defined(__NetBSD__)
    #define ptoa(x) ((paddr_t)(x) << PAGE_SHIFT)
#endif

/*
 * Return extended memory info for a process as a Python tuple.
 */
static PyObject *
psutil_proc_memory_info(PyObject *self, PyObject *args) {
    long pid;
    kinfo_proc kp;
    if (! PyArg_ParseTuple(args, "l", &pid))
        return NULL;
    if (psutil_kinfo_proc(pid, &kp) == -1)
        return NULL;

    return Py_BuildValue(
        "(lllll)",
#ifdef __FreeBSD__
        (long) ptoa(kp.ki_rssize),  // rss
        (long) kp.ki_size,  // vms
        (long) ptoa(kp.ki_tsize),  // text
        (long) ptoa(kp.ki_dsize),  // data
        (long) ptoa(kp.ki_ssize)  // stack
#else
        (long) ptoa(kp.p_vm_rssize),    // rss
    #ifdef __OpenBSD__
        // VMS, this is how ps determines it on OpenBSD:
        // http://anoncvs.spacehopper.org/openbsd-src/tree/bin/ps/print.c#n461
        (long) ptoa(kp.p_vm_dsize + kp.p_vm_ssize + kp.p_vm_tsize),  // vms
    #elif __NetBSD__
        // VMS, this is how top determines it on NetBSD:
        // ftp://ftp.iij.ad.jp/pub/NetBSD/NetBSD-release-6/src/external/bsd/
        //     top/dist/machine/m_netbsd.c
        (long) ptoa(kp.p_vm_msize),  // vms
    #endif
        (long) ptoa(kp.p_vm_tsize),  // text
        (long) ptoa(kp.p_vm_dsize),  // data
        (long) ptoa(kp.p_vm_ssize)  // stack
#endif
    );
}


/*
 * Return a Python tuple representing user, kernel and idle CPU times
 */
static PyObject *
psutil_cpu_times(PyObject *self, PyObject *args) {
#if defined(__NetBSD__)
    u_int64_t cpu_time[CPUSTATES];
#else
    long cpu_time[CPUSTATES];
#endif
    size_t size = sizeof(cpu_time);
    int ret;

#if defined(__FreeBSD__) || defined(__NetBSD__)
    ret = sysctlbyname("kern.cp_time", &cpu_time, &size, NULL, 0);
#elif __OpenBSD__
    int mib[] = {CTL_KERN, KERN_CPTIME};
    ret = sysctl(mib, 2, &cpu_time, &size, NULL, 0);
#endif
    if (ret == -1) {
        PyErr_SetFromErrno(PyExc_OSError);
        return NULL;
    }

    return Py_BuildValue("(ddddd)",
                         (double)cpu_time[CP_USER] / CLOCKS_PER_SEC,
                         (double)cpu_time[CP_NICE] / CLOCKS_PER_SEC,
                         (double)cpu_time[CP_SYS] / CLOCKS_PER_SEC,
                         (double)cpu_time[CP_IDLE] / CLOCKS_PER_SEC,
                         (double)cpu_time[CP_INTR] / CLOCKS_PER_SEC
                        );
}


 /*
 * Return files opened by process as a list of (path, fd) tuples.
 * TODO: this is broken as it may report empty paths. 'procstat'
 * utility has the same problem see:
 * https://github.com/giampaolo/psutil/issues/595
 */
#if (defined(__FreeBSD_version) && __FreeBSD_version >= 800000) || __OpenBSD__ || defined(__NetBSD__)
static PyObject *
psutil_proc_open_files(PyObject *self, PyObject *args) {
    long pid;
    int i, cnt;
    struct kinfo_file *freep = NULL;
    struct kinfo_file *kif;
    kinfo_proc kipp;
    PyObject *py_retlist = PyList_New(0);
    PyObject *py_tuple = NULL;

    if (py_retlist == NULL)
        return NULL;
    if (! PyArg_ParseTuple(args, "l", &pid))
        goto error;
    if (psutil_kinfo_proc(pid, &kipp) == -1)
        goto error;

    freep = kinfo_getfile(pid, &cnt);
    if (freep == NULL) {
        psutil_raise_ad_or_nsp(pid);
        goto error;
    }

    for (i = 0; i < cnt; i++) {
        kif = &freep[i];
#ifdef __FreeBSD__
        if ((kif->kf_type == KF_TYPE_VNODE) &&
                (kif->kf_vnode_type == KF_VTYPE_VREG))
        {
            py_tuple = Py_BuildValue("(si)", kif->kf_path, kif->kf_fd);
#elif defined(__OpenBSD__)
        if ((kif->f_type == DTYPE_VNODE) &&
                (kif->v_type == VREG))
        {
            py_tuple = Py_BuildValue("(si)", "", kif->fd_fd);
#elif defined(__NetBSD__)
        if ((kif->ki_ftype == DTYPE_VNODE) &&
                (kif->ki_vtype == VREG))
        {
            py_tuple = Py_BuildValue("(si)", "", kif->ki_fd);
#endif
            if (py_tuple == NULL)
                goto error;
            if (PyList_Append(py_retlist, py_tuple))
                goto error;
            Py_DECREF(py_tuple);
        }
    }
    free(freep);
    return py_retlist;

error:
    Py_XDECREF(py_tuple);
    Py_DECREF(py_retlist);
    if (freep != NULL)
        free(freep);
    return NULL;
}
#endif


/*
 * Return a list of tuples including device, mount point and fs type
 * for all partitions mounted on the system.
 */
static PyObject *
psutil_disk_partitions(PyObject *self, PyObject *args) {
    int num;
    int i;
    long len;
    uint64_t flags;
    char opts[200];
#if defined(__NetBSD__)
    struct statvfs *fs = NULL;
#else
    struct statfs *fs = NULL;
#endif
    PyObject *py_retlist = PyList_New(0);
    PyObject *py_tuple = NULL;

    if (py_retlist == NULL)
        return NULL;

    // get the number of mount points
    Py_BEGIN_ALLOW_THREADS
#if defined(__NetBSD__)
    num = getvfsstat(NULL, 0, MNT_NOWAIT);
#else
    num = getfsstat(NULL, 0, MNT_NOWAIT);
#endif
    Py_END_ALLOW_THREADS
    if (num == -1) {
        PyErr_SetFromErrno(PyExc_OSError);
        goto error;
    }

    len = sizeof(*fs) * num;
    fs = malloc(len);
    if (fs == NULL) {
        PyErr_NoMemory();
        goto error;
    }

    Py_BEGIN_ALLOW_THREADS
#if defined(__NetBSD__)
    num = getvfsstat(fs, len, MNT_NOWAIT);
#else
    num = getfsstat(fs, len, MNT_NOWAIT);
#endif
    Py_END_ALLOW_THREADS
    if (num == -1) {
        PyErr_SetFromErrno(PyExc_OSError);
        goto error;
    }

    for (i = 0; i < num; i++) {
        py_tuple = NULL;
        opts[0] = 0;
#if defined(__NetBSD__)
        flags = fs[i].f_flag;
#else
        flags = fs[i].f_flags;
#endif

        // see sys/mount.h
        if (flags & MNT_RDONLY)
            strlcat(opts, "ro", sizeof(opts));
        else
            strlcat(opts, "rw", sizeof(opts));
        if (flags & MNT_SYNCHRONOUS)
            strlcat(opts, ",sync", sizeof(opts));
        if (flags & MNT_NOEXEC)
            strlcat(opts, ",noexec", sizeof(opts));
        if (flags & MNT_NOSUID)
            strlcat(opts, ",nosuid", sizeof(opts));
        if (flags & MNT_ASYNC)
            strlcat(opts, ",async", sizeof(opts));
        if (flags & MNT_NOATIME)
            strlcat(opts, ",noatime", sizeof(opts));
        if (flags & MNT_SOFTDEP)
            strlcat(opts, ",softdep", sizeof(opts));
#ifdef __FreeBSD__
        if (flags & MNT_UNION)
            strlcat(opts, ",union", sizeof(opts));
        if (flags & MNT_SUIDDIR)
            strlcat(opts, ",suiddir", sizeof(opts));
        if (flags & MNT_SOFTDEP)
            strlcat(opts, ",softdep", sizeof(opts));
        if (flags & MNT_NOSYMFOLLOW)
            strlcat(opts, ",nosymfollow", sizeof(opts));
        if (flags & MNT_GJOURNAL)
            strlcat(opts, ",gjournal", sizeof(opts));
        if (flags & MNT_MULTILABEL)
            strlcat(opts, ",multilabel", sizeof(opts));
        if (flags & MNT_ACLS)
            strlcat(opts, ",acls", sizeof(opts));
        if (flags & MNT_NOCLUSTERR)
            strlcat(opts, ",noclusterr", sizeof(opts));
        if (flags & MNT_NOCLUSTERW)
            strlcat(opts, ",noclusterw", sizeof(opts));
        if (flags & MNT_NFS4ACLS)
            strlcat(opts, ",nfs4acls", sizeof(opts));
#elif __NetBSD__
        if (flags & MNT_NODEV)
            strlcat(opts, ",nodev", sizeof(opts));
        if (flags & MNT_UNION)
            strlcat(opts, ",union", sizeof(opts));
        if (flags & MNT_NOCOREDUMP)
            strlcat(opts, ",nocoredump", sizeof(opts));
        if (flags & MNT_RELATIME)
            strlcat(opts, ",relatime", sizeof(opts));
        if (flags & MNT_IGNORE)
            strlcat(opts, ",ignore", sizeof(opts));
        if (flags & MNT_DISCARD)
            strlcat(opts, ",discard", sizeof(opts));
        if (flags & MNT_EXTATTR)
            strlcat(opts, ",extattr", sizeof(opts));
        if (flags & MNT_LOG)
            strlcat(opts, ",log", sizeof(opts));
        if (flags & MNT_SYMPERM)
            strlcat(opts, ",symperm", sizeof(opts));
        if (flags & MNT_NODEVMTIME)
            strlcat(opts, ",nodevmtime", sizeof(opts));
#endif
        py_tuple = Py_BuildValue("(ssss)",
                                 fs[i].f_mntfromname,  // device
                                 fs[i].f_mntonname,    // mount point
                                 fs[i].f_fstypename,   // fs type
                                 opts);                // options
        if (!py_tuple)
            goto error;
        if (PyList_Append(py_retlist, py_tuple))
            goto error;
        Py_DECREF(py_tuple);
    }

    free(fs);
    return py_retlist;

error:
    Py_XDECREF(py_tuple);
    Py_DECREF(py_retlist);
    if (fs != NULL)
        free(fs);
    return NULL;
}


/*
 * Return a Python list of named tuples with overall network I/O information
 */
static PyObject *
psutil_net_io_counters(PyObject *self, PyObject *args) {
    char *buf = NULL, *lim, *next;
    struct if_msghdr *ifm;
    int mib[6];
    size_t len;
    PyObject *py_retdict = PyDict_New();
    PyObject *py_ifc_info = NULL;
    if (py_retdict == NULL)
        return NULL;

    mib[0] = CTL_NET;          // networking subsystem
    mib[1] = PF_ROUTE;         // type of information
    mib[2] = 0;                // protocol (IPPROTO_xxx)
    mib[3] = 0;                // address family
    mib[4] = NET_RT_IFLIST;   // operation
    mib[5] = 0;

    if (sysctl(mib, 6, NULL, &len, NULL, 0) < 0) {
        PyErr_SetFromErrno(PyExc_OSError);
        goto error;
    }

    buf = malloc(len);
    if (buf == NULL) {
        PyErr_NoMemory();
        goto error;
    }

    if (sysctl(mib, 6, buf, &len, NULL, 0) < 0) {
        PyErr_SetFromErrno(PyExc_OSError);
        goto error;
    }

    lim = buf + len;

    for (next = buf; next < lim; ) {
        py_ifc_info = NULL;
        ifm = (struct if_msghdr *)next;
        next += ifm->ifm_msglen;

        if (ifm->ifm_type == RTM_IFINFO) {
            struct if_msghdr *if2m = (struct if_msghdr *)ifm;
            struct sockaddr_dl *sdl = (struct sockaddr_dl *)(if2m + 1);
            char ifc_name[32];

            strncpy(ifc_name, sdl->sdl_data, sdl->sdl_nlen);
            ifc_name[sdl->sdl_nlen] = 0;
            // XXX: ignore usbus interfaces:
            // http://lists.freebsd.org/pipermail/freebsd-current/
            //     2011-October/028752.html
            // 'ifconfig -a' doesn't show them, nor do we.
            if (strncmp(ifc_name, "usbus", 5) == 0)
                continue;

            py_ifc_info = Py_BuildValue("(kkkkkkki)",
                                        if2m->ifm_data.ifi_obytes,
                                        if2m->ifm_data.ifi_ibytes,
                                        if2m->ifm_data.ifi_opackets,
                                        if2m->ifm_data.ifi_ipackets,
                                        if2m->ifm_data.ifi_ierrors,
                                        if2m->ifm_data.ifi_oerrors,
                                        if2m->ifm_data.ifi_iqdrops,
                                        0);  // dropout not supported
            if (!py_ifc_info)
                goto error;
            if (PyDict_SetItemString(py_retdict, ifc_name, py_ifc_info))
                goto error;
            Py_DECREF(py_ifc_info);
        }
        else {
            continue;
        }
    }

    free(buf);
    return py_retdict;

error:
    Py_XDECREF(py_ifc_info);
    Py_DECREF(py_retdict);
    if (buf != NULL)
        free(buf);
    return NULL;
}


/*
 * Return currently connected users as a list of tuples.
 */
static PyObject *
psutil_users(PyObject *self, PyObject *args) {
    PyObject *py_retlist = PyList_New(0);
    PyObject *py_tuple = NULL;

    if (py_retlist == NULL)
        return NULL;

#if (defined(__FreeBSD_version) && (__FreeBSD_version < 900000)) || __OpenBSD__
    struct utmp ut;
    FILE *fp;

    fp = fopen(_PATH_UTMP, "r");
    if (fp == NULL) {
        PyErr_SetFromErrno(PyExc_OSError);
        goto error;
    }

    while (fread(&ut, sizeof(ut), 1, fp) == 1) {
        if (*ut.ut_name == '\0')
            continue;
        py_tuple = Py_BuildValue(
            "(sssf)",
            ut.ut_name,         // username
            ut.ut_line,         // tty
            ut.ut_host,         // hostname
           (float)ut.ut_time);  // start time
        if (!py_tuple) {
            fclose(fp);
            goto error;
        }
        if (PyList_Append(py_retlist, py_tuple)) {
            fclose(fp);
            goto error;
        }
        Py_DECREF(py_tuple);
    }

    fclose(fp);
#else
    struct utmpx *utx;

    setutxent();
    while ((utx = getutxent()) != NULL) {
        if (utx->ut_type != USER_PROCESS)
            continue;
        py_tuple = Py_BuildValue(
            "(sssf)",
            utx->ut_user,  // username
            utx->ut_line,  // tty
            utx->ut_host,  // hostname
            (float)utx->ut_tv.tv_sec  // start time
        );

        if (!py_tuple) {
            endutxent();
            goto error;
        }
        if (PyList_Append(py_retlist, py_tuple)) {
            endutxent();
            goto error;
        }
        Py_DECREF(py_tuple);
    }

    endutxent();
#endif
    return py_retlist;

error:
    Py_XDECREF(py_tuple);
    Py_DECREF(py_retlist);
    return NULL;
}


/*
 * define the psutil C module methods and initialize the module.
 */
static PyMethodDef
PsutilMethods[] = {
    // --- per-process functions

    {"proc_name", psutil_proc_name, METH_VARARGS,
     "Return process name"},
    {"proc_connections", psutil_proc_connections, METH_VARARGS,
     "Return connections opened by process"},
    {"proc_cmdline", psutil_proc_cmdline, METH_VARARGS,
     "Return process cmdline as a list of cmdline arguments"},
    {"proc_ppid", psutil_proc_ppid, METH_VARARGS,
     "Return process ppid as an integer"},
    {"proc_uids", psutil_proc_uids, METH_VARARGS,
     "Return process real effective and saved user ids as a Python tuple"},
    {"proc_gids", psutil_proc_gids, METH_VARARGS,
     "Return process real effective and saved group ids as a Python tuple"},
    {"proc_cpu_times", psutil_proc_cpu_times, METH_VARARGS,
     "Return tuple of user/kern time for the given PID"},
    {"proc_create_time", psutil_proc_create_time, METH_VARARGS,
     "Return a float indicating the process create time expressed in "
     "seconds since the epoch"},
    {"proc_memory_info", psutil_proc_memory_info, METH_VARARGS,
     "Return extended memory info for a process as a Python tuple."},
    {"proc_num_ctx_switches", psutil_proc_num_ctx_switches, METH_VARARGS,
     "Return the number of context switches performed by process"},
    {"proc_threads", psutil_proc_threads, METH_VARARGS,
     "Return process threads"},
    {"proc_status", psutil_proc_status, METH_VARARGS,
     "Return process status as an integer"},
    {"proc_io_counters", psutil_proc_io_counters, METH_VARARGS,
     "Return process IO counters"},
    {"proc_tty_nr", psutil_proc_tty_nr, METH_VARARGS,
     "Return process tty (terminal) number"},
#if defined(__FreeBSD__) || defined(__OpenBSD__)
    {"proc_cwd", psutil_proc_cwd, METH_VARARGS,
     "Return process current working directory."},
#endif
#if defined(__FreeBSD_version) && __FreeBSD_version >= 800000 || __OpenBSD__ || defined(__NetBSD__)
    {"proc_num_fds", psutil_proc_num_fds, METH_VARARGS,
     "Return the number of file descriptors opened by this process"},
#endif
#if defined(__FreeBSD_version) && __FreeBSD_version >= 800000 || __OpenBSD__ || defined(__NetBSD__)
    {"proc_open_files", psutil_proc_open_files, METH_VARARGS,
     "Return files opened by process as a list of (path, fd) tuples"},
#endif

#if defined(__FreeBSD__) || defined(__NetBSD__)
    {"proc_exe", psutil_proc_exe, METH_VARARGS,
     "Return process pathname executable"},
    {"proc_num_threads", psutil_proc_num_threads, METH_VARARGS,
     "Return number of threads used by process"},
#if defined(__FreeBSD__)
    {"proc_memory_maps", psutil_proc_memory_maps, METH_VARARGS,
     "Return a list of tuples for every process's memory map"},
    {"proc_cpu_affinity_get", psutil_proc_cpu_affinity_get, METH_VARARGS,
     "Return process CPU affinity."},
    {"proc_cpu_affinity_set", psutil_proc_cpu_affinity_set, METH_VARARGS,
     "Set process CPU affinity."},
    {"cpu_count_phys", psutil_cpu_count_phys, METH_VARARGS,
     "Return an XML string to determine the number physical CPUs."},
#endif
#endif

    // --- system-related functions

    {"pids", psutil_pids, METH_VARARGS,
     "Returns a list of PIDs currently running on the system"},
    {"cpu_count_logical", psutil_cpu_count_logical, METH_VARARGS,
     "Return number of logical CPUs on the system"},
    {"virtual_mem", psutil_virtual_mem, METH_VARARGS,
     "Return system virtual memory usage statistics"},
    {"swap_mem", psutil_swap_mem, METH_VARARGS,
     "Return swap mem stats"},
    {"cpu_times", psutil_cpu_times, METH_VARARGS,
     "Return system cpu times as a tuple (user, system, nice, idle, irc)"},
    {"per_cpu_times", psutil_per_cpu_times, METH_VARARGS,
     "Return system per-cpu times as a list of tuples"},
    {"boot_time", psutil_boot_time, METH_VARARGS,
     "Return the system boot time expressed in seconds since the epoch."},
    {"disk_partitions", psutil_disk_partitions, METH_VARARGS,
     "Return a list of tuples including device, mount point and "
     "fs type for all partitions mounted on the system."},
    {"net_io_counters", psutil_net_io_counters, METH_VARARGS,
     "Return dict of tuples of networks I/O information."},
    {"disk_io_counters", psutil_disk_io_counters, METH_VARARGS,
     "Return a Python dict of tuples for disk I/O information"},
    {"users", psutil_users, METH_VARARGS,
     "Return currently connected users as a list of tuples"},
#if defined(__FreeBSD__) || defined(__NetBSD__)
    {"net_connections", psutil_net_connections, METH_VARARGS,
     "Return system-wide open connections."},
#endif
    {NULL, NULL, 0, NULL}
};

struct module_state {
    PyObject *error;
};

#if PY_MAJOR_VERSION >= 3
#define GETSTATE(m) ((struct module_state*)PyModule_GetState(m))
#else
#define GETSTATE(m) (&_state)
#endif

#if PY_MAJOR_VERSION >= 3

static int
psutil_bsd_traverse(PyObject *m, visitproc visit, void *arg) {
    Py_VISIT(GETSTATE(m)->error);
    return 0;
}

static int
psutil_bsd_clear(PyObject *m) {
    Py_CLEAR(GETSTATE(m)->error);
    return 0;
}

static struct PyModuleDef
        moduledef = {
    PyModuleDef_HEAD_INIT,
    "psutil_bsd",
    NULL,
    sizeof(struct module_state),
    PsutilMethods,
    NULL,
    psutil_bsd_traverse,
    psutil_bsd_clear,
    NULL
};

#define INITERROR return NULL

PyMODINIT_FUNC PyInit__psutil_bsd(void)

#else
#define INITERROR return

void init_psutil_bsd(void)
#endif
{
#if PY_MAJOR_VERSION >= 3
    PyObject *module = PyModule_Create(&moduledef);
#else
    PyObject *module = Py_InitModule("_psutil_bsd", PsutilMethods);
#endif
    PyModule_AddIntConstant(module, "version", PSUTIL_VERSION);
    // process status constants

#ifdef __FreeBSD__
    PyModule_AddIntConstant(module, "SIDL", SIDL);
    PyModule_AddIntConstant(module, "SRUN", SRUN);
    PyModule_AddIntConstant(module, "SSLEEP", SSLEEP);
    PyModule_AddIntConstant(module, "SSTOP", SSTOP);
    PyModule_AddIntConstant(module, "SZOMB", SZOMB);
    PyModule_AddIntConstant(module, "SWAIT", SWAIT);
    PyModule_AddIntConstant(module, "SLOCK", SLOCK);
#elif  __OpenBSD__
    PyModule_AddIntConstant(module, "SIDL", SIDL);
    PyModule_AddIntConstant(module, "SRUN", SRUN);
    PyModule_AddIntConstant(module, "SSLEEP", SSLEEP);
    PyModule_AddIntConstant(module, "SSTOP", SSTOP);
    PyModule_AddIntConstant(module, "SZOMB", SZOMB);  // unused
    PyModule_AddIntConstant(module, "SDEAD", SDEAD);
    PyModule_AddIntConstant(module, "SONPROC", SONPROC);
#elif  defined(__NetBSD__)
    PyModule_AddIntConstant(module, "SIDL", LSIDL);
    PyModule_AddIntConstant(module, "SRUN", LSRUN);
    PyModule_AddIntConstant(module, "SSLEEP", LSSLEEP);
    PyModule_AddIntConstant(module, "SSTOP", LSSTOP);
    PyModule_AddIntConstant(module, "SZOMB", LSZOMB);
    PyModule_AddIntConstant(module, "SDEAD", LSDEAD);
    PyModule_AddIntConstant(module, "SONPROC", LSONPROC);
    // unique to NetBSD
    PyModule_AddIntConstant(module, "SSUSPENDED", LSSUSPENDED);
#endif

    // connection status constants
    PyModule_AddIntConstant(module, "TCPS_CLOSED", TCPS_CLOSED);
    PyModule_AddIntConstant(module, "TCPS_CLOSING", TCPS_CLOSING);
    PyModule_AddIntConstant(module, "TCPS_CLOSE_WAIT", TCPS_CLOSE_WAIT);
    PyModule_AddIntConstant(module, "TCPS_LISTEN", TCPS_LISTEN);
    PyModule_AddIntConstant(module, "TCPS_ESTABLISHED", TCPS_ESTABLISHED);
    PyModule_AddIntConstant(module, "TCPS_SYN_SENT", TCPS_SYN_SENT);
    PyModule_AddIntConstant(module, "TCPS_SYN_RECEIVED", TCPS_SYN_RECEIVED);
    PyModule_AddIntConstant(module, "TCPS_FIN_WAIT_1", TCPS_FIN_WAIT_1);
    PyModule_AddIntConstant(module, "TCPS_FIN_WAIT_2", TCPS_FIN_WAIT_2);
    PyModule_AddIntConstant(module, "TCPS_LAST_ACK", TCPS_LAST_ACK);
    PyModule_AddIntConstant(module, "TCPS_TIME_WAIT", TCPS_TIME_WAIT);
    // PSUTIL_CONN_NONE
    PyModule_AddIntConstant(module, "PSUTIL_CONN_NONE", 128);

    if (module == NULL)
        INITERROR;
#if PY_MAJOR_VERSION >= 3
    return module;
#endif
}
