/**
 * @file message.h
 * Message processing functions and structures
 */

#ifndef RSPAMD_MESSAGE_H
#define RSPAMD_MESSAGE_H

#include "config.h"
#include "email_addr.h"
#include "addr.h"
#include "cryptobox.h"
#include "mime_headers.h"
#include "content_type.h"

struct rspamd_task;
struct controller_session;
struct html_content;
struct rspamd_image;
struct rspamd_archive;

enum rspamd_mime_part_flags {
	RSPAMD_MIME_PART_TEXT = (1 << 0),
	RSPAMD_MIME_PART_ATTACHEMENT = (1 << 1),
	RSPAMD_MIME_PART_IMAGE = (1 << 2),
	RSPAMD_MIME_PART_ARCHIVE = (1 << 3),
	RSPAMD_MIME_PART_BAD_CTE = (1 << 4)
};

enum rspamd_cte {
	RSPAMD_CTE_UNKNOWN = 0,
	RSPAMD_CTE_7BIT = 1,
	RSPAMD_CTE_8BIT = 2,
	RSPAMD_CTE_QP = 3,
	RSPAMD_CTE_B64 = 4,
};

struct rspamd_mime_text_part;

struct rspamd_mime_multipart {
	GPtrArray *children;
};

struct rspamd_mime_part {
	struct rspamd_content_type *ct;
	struct rspamd_content_disposition *cd;
	rspamd_ftok_t raw_data;
	rspamd_ftok_t parsed_data;
	struct rspamd_mime_part *parent_part;
	GHashTable *raw_headers;
	gchar *raw_headers_str;
	gsize raw_headers_len;
	enum rspamd_cte cte;

	union {
		struct rspamd_mime_multipart mp;
		struct rspamd_mime_text_part *txt;
		struct rspamd_image *img;
		struct rspamd_archive *arch;
	} specific;

	enum rspamd_mime_part_flags flags;
	guchar digest[rspamd_cryptobox_HASHBYTES];
};

#define RSPAMD_MIME_TEXT_PART_FLAG_UTF (1 << 0)
#define RSPAMD_MIME_TEXT_PART_FLAG_BALANCED (1 << 1)
#define RSPAMD_MIME_TEXT_PART_FLAG_EMPTY (1 << 2)
#define RSPAMD_MIME_TEXT_PART_FLAG_HTML (1 << 3)

#define IS_PART_EMPTY(part) ((part)->flags & RSPAMD_MIME_TEXT_PART_FLAG_EMPTY)
#define IS_PART_UTF(part) ((part)->flags & RSPAMD_MIME_TEXT_PART_FLAG_UTF)
#define IS_PART_RAW(part) (!((part)->flags & RSPAMD_MIME_TEXT_PART_FLAG_UTF))
#define IS_PART_HTML(part) ((part)->flags & RSPAMD_MIME_TEXT_PART_FLAG_HTML)

struct rspamd_mime_text_part {
	guint flags;
	GUnicodeScript script;
	const gchar *lang_code;
	const gchar *language;
	const gchar *real_charset;
	rspamd_ftok_t raw;
	rspamd_ftok_t parsed;
	GByteArray *content;
	GByteArray *stripped_content;
	GPtrArray *newlines;	/**< positions of newlines in text					*/
	struct html_content *html;
	GList *exceptions;	/**< list of offsets of urls						*/
	struct rspamd_mime_part *mime_part;
	GArray *normalized_words;
	GArray *normalized_hashes;
	guint nlines;
};

enum rspamd_received_type {
	RSPAMD_RECEIVED_SMTP = 0,
	RSPAMD_RECEIVED_ESMTP,
	RSPAMD_RECEIVED_ESMTPA,
	RSPAMD_RECEIVED_ESMTPS,
	RSPAMD_RECEIVED_ESMTPSA,
	RSPAMD_RECEIVED_LMTP,
	RSPAMD_RECEIVED_IMAP,
	RSPAMD_RECEIVED_UNKNOWN
};

struct received_header {
	gchar *from_hostname;
	gchar *from_ip;
	gchar *real_hostname;
	gchar *real_ip;
	gchar *by_hostname;
	gchar *for_mbox;
	rspamd_inet_addr_t *addr;
	time_t timestamp;
	enum rspamd_received_type type;
};

/**
 * Parse and pre-process mime message
 * @param task worker_task object
 * @return
 */
gboolean rspamd_message_parse (struct rspamd_task *task);

/**
 * Get an array of header's values with specified header's name using raw headers
 * @param task worker task structure
 * @param field header's name
 * @param strong if this flag is TRUE header's name is case sensitive, otherwise it is not
 * @return An array of header's values or NULL. It is NOT permitted to free array or values.
 */
GPtrArray *rspamd_message_get_header_array (struct rspamd_task *task,
		const gchar *field,
		gboolean strong);
/**
 * Get an array of mime parts header's values with specified header's name using raw headers
 * @param task worker task structure
 * @param field header's name
 * @param strong if this flag is TRUE header's name is case sensitive, otherwise it is not
 * @return An array of header's values or NULL. It is NOT permitted to free array or values.
 */
GPtrArray *rspamd_message_get_mime_header_array (struct rspamd_task *task,
		const gchar *field,
		gboolean strong);

/**
 * Get an array of header's values with specified header's name using raw headers
 * @param htb hash table indexed by header name (caseless) with ptr arrays as elements
 * @param field header's name
 * @param strong if this flag is TRUE header's name is case sensitive, otherwise it is not
 * @return An array of header's values or NULL. It is NOT permitted to free array or values.
 */
GPtrArray *rspamd_message_get_header_from_hash (GHashTable *htb,
		rspamd_mempool_t *pool,
		const gchar *field,
		gboolean strong);

#endif
