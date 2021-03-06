#ifndef RSPAMD_UTIL_H
#define RSPAMD_UTIL_H

#include "config.h"
#include "mem_pool.h"
#include "printf.h"
#include "fstring.h"
#include "addr.h"
#include "str_util.h"

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#include <event.h>
#include <time.h>

struct rspamd_config;
struct rspamd_main;
struct workq;

/**
 * Create generic socket
 * @param af address family
 * @param type socket type
 * @param protocol socket protocol
 * @param async set non-blocking on a socket
 * @return socket FD or -1 in case of error
 */
gint rspamd_socket_create (gint af, gint type, gint protocol, gboolean async);
/*
 * Create socket and bind or connect it to specified address and port
 */
gint rspamd_socket_tcp (struct addrinfo *, gboolean is_server, gboolean async);
/*
 * Create socket and bind or connect it to specified address and port
 */
gint rspamd_socket_udp (struct addrinfo *, gboolean is_server, gboolean async);

/*
 * Create and bind or connect unix socket
 */
gint rspamd_socket_unix (const gchar *,
	struct sockaddr_un *,
	gint type,
	gboolean is_server,
	gboolean async);

/**
 * Make a universal socket
 * @param credits host, ip or path to unix socket
 * @param port port (used for network sockets)
 * @param type type of socket (SO_STREAM or SO_DGRAM)
 * @param async make this socket asynced
 * @param is_server make this socket as server socket
 * @param try_resolve try name resolution for a socket (BLOCKING)
 */
gint rspamd_socket (const gchar *credits, guint16 port, gint type,
	gboolean async, gboolean is_server, gboolean try_resolve);

/**
 * Make a universal sockets
 * @param credits host, ip or path to unix socket (several items may be separated by ',')
 * @param port port (used for network sockets)
 * @param type type of socket (SO_STREAM or SO_DGRAM)
 * @param async make this socket asynced
 * @param is_server make this socket as server socket
 * @param try_resolve try name resolution for a socket (BLOCKING)
 */
GList * rspamd_sockets_list (const gchar *credits,
	guint16 port,
	gint type,
	gboolean async,
	gboolean is_server,
	gboolean try_resolve);
/*
 * Create socketpair
 */
gboolean rspamd_socketpair (gint pair[2]);

/*
 * Write pid to file
 */
gint rspamd_write_pid (struct rspamd_main *);

/*
 * Make specified socket non-blocking
 */
gint rspamd_socket_nonblocking (gint);
/*
 * Make specified socket blocking
 */
gint rspamd_socket_blocking (gint);

/*
 * Poll a sync socket for specified events
 */
gint rspamd_socket_poll (gint fd, gint timeout, short events);

/*
 * Init signals
 */
#ifdef HAVE_SA_SIGINFO
void rspamd_signals_init (struct sigaction *sa, void (*sig_handler)(gint,
	siginfo_t *,
	void *));
#else
void rspamd_signals_init (struct sigaction *sa, void (*sig_handler)(gint));
#endif

/*
 * Send specified signal to each worker
 */
void rspamd_pass_signal (GHashTable *, gint );

#ifndef HAVE_SETPROCTITLE
/*
 * Process title utility functions
 */
gint init_title (gint argc, gchar *argv[], gchar *envp[]);
gint setproctitle (const gchar *fmt, ...);
#endif

#ifndef HAVE_PIDFILE
/*
 * Pidfile functions from FreeBSD libutil code
 */
typedef struct rspamd_pidfh_s {
	gint pf_fd;
#ifdef HAVE_PATH_MAX
	gchar pf_path[PATH_MAX + 1];
#elif defined(HAVE_MAXPATHLEN)
	gchar pf_path[MAXPATHLEN + 1];
#else
	gchar pf_path[1024 + 1];
#endif
	dev_t pf_dev;
	ino_t pf_ino;
} rspamd_pidfh_t;
rspamd_pidfh_t * rspamd_pidfile_open (const gchar *path,
	mode_t mode,
	pid_t *pidptr);
gint rspamd_pidfile_write (rspamd_pidfh_t *pfh);
gint rspamd_pidfile_close (rspamd_pidfh_t *pfh);
gint rspamd_pidfile_remove (rspamd_pidfh_t *pfh);
#else
typedef struct pidfh rspamd_pidfh_t;
#define rspamd_pidfile_open pidfile_open
#define rspamd_pidfile_write pidfile_write
#define rspamd_pidfile_close pidfile_close
#define rspamd_pidfile_remove pidfile_remove
#endif

/*
 * Replace %r with rcpt value and %f with from value, new string is allocated in pool
 */
gchar * resolve_stat_filename (rspamd_mempool_t *pool,
	gchar *pattern,
	gchar *rcpt,
	gchar *from);

const gchar *
rspamd_log_check_time (gdouble start, gdouble end, gint resolution);

/*
 * File locking functions
 */
gboolean rspamd_file_lock (gint fd, gboolean async);
gboolean rspamd_file_unlock (gint fd, gboolean async);

/*
 * Google perf-tools initialization function
 */
void gperf_profiler_init (struct rspamd_config *cfg, const gchar *descr);
void gperf_profiler_stop (void);

/*
 * Workarounds for older versions of glib
 */
#if ((GLIB_MAJOR_VERSION == 2) && (GLIB_MINOR_VERSION < 22))
void g_ptr_array_unref (GPtrArray *array);
gboolean g_int64_equal (gconstpointer v1, gconstpointer v2);
guint g_int64_hash (gconstpointer v);
#endif
#if ((GLIB_MAJOR_VERSION == 2) && (GLIB_MINOR_VERSION < 14))
void g_queue_clear (GQueue *queue);
#endif
#if ((GLIB_MAJOR_VERSION == 2) && (GLIB_MINOR_VERSION < 32))
void g_queue_free_full (GQueue *queue, GDestroyNotify free_func);
#endif
#if ((GLIB_MAJOR_VERSION == 2) && (GLIB_MINOR_VERSION < 40))
void g_ptr_array_insert (GPtrArray *array, gint index_, gpointer data);
#endif

/*
 * Convert milliseconds to timeval fields
 */
#define msec_to_tv(msec, tv) do { (tv)->tv_sec = (msec) / 1000; (tv)->tv_usec = \
									  ((msec) - (tv)->tv_sec * 1000) * 1000; \
} while (0)
#define double_to_tv(dbl, tv) do { (tv)->tv_sec = (int)(dbl); (tv)->tv_usec = \
									   ((dbl) - (int)(dbl)) * 1000 * 1000; \
} while (0)
#define double_to_ts(dbl, ts) do { (ts)->tv_sec = (int)(dbl); (ts)->tv_nsec = \
                                       ((dbl) - (int)(dbl)) * 1e9; \
} while (0)
#define tv_to_msec(tv) ((tv)->tv_sec * 1000LLU + (tv)->tv_usec / 1000LLU)
#define tv_to_double(tv) ((double)(tv)->tv_sec + (tv)->tv_usec / 1.0e6)
#define ts_to_usec(ts) ((ts)->tv_sec * 1000000LLU +							\
	(ts)->tv_nsec / 1000LLU)

/**
 * Try to allocate a file on filesystem (using fallocate or posix_fallocate)
 * @param fd descriptor
 * @param offset offset of file
 * @param len length to allocate
 * @return -1 in case of failure
 */
gint rspamd_fallocate (gint fd, off_t offset, off_t len);

/**
 * Utils for working with threads to be compatible with all glib versions
 */
typedef struct rspamd_mutex_s {
#if ((GLIB_MAJOR_VERSION == 2) && (GLIB_MINOR_VERSION > 30))
	GMutex mtx;
#else
	GStaticMutex mtx;
#endif
} rspamd_mutex_t;

typedef struct rspamd_rwlock_s {
#if ((GLIB_MAJOR_VERSION == 2) && (GLIB_MINOR_VERSION > 30))
	GRWLock rwlock;
#else
	GStaticRWLock rwlock;
#endif
} rspamd_rwlock_t;


/**
 * Create new mutex
 * @return mutex or NULL
 */
rspamd_mutex_t * rspamd_mutex_new (void);

/**
 * Lock mutex
 * @param mtx
 */
void rspamd_mutex_lock (rspamd_mutex_t *mtx);

/**
 * Unlock mutex
 * @param mtx
 */
void rspamd_mutex_unlock (rspamd_mutex_t *mtx);

/**
 * Clear rspamd mutex
 * @param mtx
 */
void rspamd_mutex_free (rspamd_mutex_t *mtx);

/**
 * Create new rwloc
 * @return
 */
rspamd_rwlock_t * rspamd_rwlock_new (void);

/**
 * Lock rwlock for writing
 * @param mtx
 */
void rspamd_rwlock_writer_lock (rspamd_rwlock_t *mtx);

/**
 * Lock rwlock for reading
 * @param mtx
 */
void rspamd_rwlock_reader_lock (rspamd_rwlock_t *mtx);

/**
 * Unlock rwlock from writing
 * @param mtx
 */
void rspamd_rwlock_writer_unlock (rspamd_rwlock_t *mtx);

/**
 * Unlock rwlock from reading
 * @param mtx
 */
void rspamd_rwlock_reader_unlock (rspamd_rwlock_t *mtx);

/**
 * Free rwlock
 * @param mtx
 */
void rspamd_rwlock_free (rspamd_rwlock_t *mtx);

static inline void
rspamd_cond_wait (GCond *cond, rspamd_mutex_t *mtx)
{
#if ((GLIB_MAJOR_VERSION == 2) && (GLIB_MINOR_VERSION > 30))
	g_cond_wait (cond, &mtx->mtx);
#else
	g_cond_wait (cond, g_static_mutex_get_mutex (&mtx->mtx));
#endif
}

/**
 * Create new named thread
 * @param name name pattern
 * @param func function to start
 * @param data data to pass to function
 * @param err error pointer
 * @return new thread object that can be joined
 */
GThread * rspamd_create_thread (const gchar *name,
	GThreadFunc func,
	gpointer data,
	GError **err);

/**
 * Deep copy of one hash table to another
 * @param src source hash
 * @param dst destination hash
 * @param key_copy_func function called to copy or modify keys (or NULL)
 * @param value_copy_func function called to copy or modify values (or NULL)
 * @param ud user data for copy functions
 */
void rspamd_hash_table_copy (GHashTable *src, GHashTable *dst,
	gpointer (*key_copy_func)(gconstpointer data, gpointer ud),
	gpointer (*value_copy_func)(gconstpointer data, gpointer ud),
	gpointer ud);


/**
 * Read passphrase from tty
 * @param buf buffer to fill with a password
 * @param size size of the buffer
 * @param rwflag unused flag
 * @param key unused key
 * @return size of password read
 */
gint rspamd_read_passphrase (gchar *buf, gint size, gint rwflag, gpointer key);

/**
 * Portably return the current clock ticks as seconds
 * @return
 */
gdouble rspamd_get_ticks (void);

/**
 * Portably return the current virtual clock ticks as seconds
 * @return
 */
gdouble rspamd_get_virtual_ticks (void);


/**
 * Return the real timestamp as unixtime
 */
gdouble rspamd_get_calendar_ticks (void);

/**
 * Special utility to help array freeing in rspamd_mempool
 * @param p
 */
void rspamd_ptr_array_free_hard (gpointer p);

/**
 * Special utility to help array freeing in rspamd_mempool
 * @param p
 */
void rspamd_array_free_hard (gpointer p);
/**
 * Special utility to help GString freeing in rspamd_mempool
 * @param p
 */
void rspamd_gstring_free_hard (gpointer p);

/**
 * Special utility to help GString freeing (without freeing the memory segment) in rspamd_mempool
 * @param p
 */
void rspamd_gstring_free_soft (gpointer p);

struct rspamd_external_libs_ctx;
/**
 * Initialize rspamd libraries
 */
struct rspamd_external_libs_ctx* rspamd_init_libs (void);

/**
 * Configure libraries
 */
void rspamd_config_libs (struct rspamd_external_libs_ctx *ctx,
		struct rspamd_config *cfg);

/**
 * Reset and initialize decompressor
 * @param ctx
 */
gboolean rspamd_libs_reset_decompression (struct rspamd_external_libs_ctx *ctx);
/**
 * Reset and initialize compressor
 * @param ctx
 */
gboolean rspamd_libs_reset_compression (struct rspamd_external_libs_ctx *ctx);

/**
 * Destroy external libraries context
 */
void rspamd_deinit_libs (struct rspamd_external_libs_ctx *ctx);

/**
 * Returns some statically initialized random hash seed
 * @return hash seed
 */
guint64 rspamd_hash_seed (void);

/**
 * Returns random hex string of the specified length
 * @param buf
 * @param len
 */
void rspamd_random_hex (guchar *buf, guint64 len);

/**
 * Returns
 * @param pattern pattern to create (should end with some number of X symbols), modified by this function
 * @return
 */
gint rspamd_shmem_mkstemp (gchar *pattern);

/**
 * Return jittered time value
 */
gdouble rspamd_time_jitter (gdouble in, gdouble jitter);

/**
 * Return random double in range [0..1)
 * @return
 */
gdouble rspamd_random_double (void);

/**
 * Return random double in range [0..1) using xoroshiro128+ algorithm (not crypto secure)
 * @return
 */
gdouble rspamd_random_double_fast (void);

guint64 rspamd_random_uint64_fast (void);

/**
 * Seed fast rng
 */
void rspamd_random_seed_fast (void);

/**
 * Constant time version of memcmp
 */
gboolean rspamd_constant_memcmp (const guchar *a, const guchar *b, gsize len);

/* Special case for ancient libevent */
#if !defined(LIBEVENT_VERSION_NUMBER) || LIBEVENT_VERSION_NUMBER < 0x02000000UL
struct event_base * event_get_base (struct event *ev);
#endif
/* CentOS libevent */
#ifndef evsignal_set
#define evsignal_set(ev, x, cb, arg)    \
    event_set((ev), (x), EV_SIGNAL|EV_PERSIST, (cb), (arg))
#endif

/**
 * Open file without following symlinks or special stuff
 * @param fname filename
 * @param oflags open flags
 * @param mode mode to open
 * @return fd or -1 in case of error
 */
int rspamd_file_xopen (const char *fname, int oflags, guint mode);

/**
 * Map file without following symlinks or special stuff
 * @param fname filename
 * @param mode mode to open
 * @param size target size (must NOT be NULL)
 * @return pointer to memory (should be freed using munmap) or NULL in case of error
 */
gpointer rspamd_file_xmap (const char *fname, guint mode,
		gsize *size);

/**
 * Map named shared memory segment
 * @param fname filename
 * @param mode mode to open
 * @param size target size (must NOT be NULL)
 * @return pointer to memory (should be freed using munmap) or NULL in case of error
 */
gpointer rspamd_shmem_xmap (const char *fname, guint mode,
		gsize *size);

/**
 * Normalize probabilities using polynomial function
 * @param x probability (bias .. 1)
 * @return
 */
gdouble rspamd_normalize_probability (gdouble x, gdouble bias);

/**
 * Converts struct tm to time_t
 * @param tm
 * @param tz timezone in format (hours * 100) + minutes
 * @return
 */
guint64 rspamd_tm_to_time (const struct tm *tm, glong tz);

#define PTR_ARRAY_FOREACH(ar, i, cur) if ((ar) != NULL && (ar)->len > 0) for ((i) = 0; (i) < (ar)->len && (((cur) = g_ptr_array_index((ar), (i))) || 1); ++(i))
#endif
