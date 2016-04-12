/*-
 * Copyright 2016 Vsevolod Stakhov
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "config.h"
#include "multipattern.h"

#ifdef WITH_HYPERSCAN
#include "hs.h"
#else
#include "acism.h"
#endif

struct rspamd_multipattern {
#ifdef WITH_HYPERSCAN
	hs_database_t *db;
	hs_scratch_t *scratch;
	GArray *hs_pats;
	GArray *hs_ids;
	GArray *hs_flags;
#else
	ac_trie_t *t;
	GArray *pats;
#endif
	gboolean compiled;
	guint cnt;
	enum rspamd_multipattern_flags flags;
};

static GQuark
rspamd_multipattern_quark (void)
{
	return g_quark_from_static_string ("multipattern");
}

/*
 * Escapes special characters from specific pattern
 */
static gchar *
rspamd_multipattern_pattern_filter (const gchar *pattern,
		enum rspamd_multipattern_flags flags)
{
	/*
	 * TODO: implement patterns filtering
	 */
	return strdup (pattern);
}

struct rspamd_multipattern *
rspamd_multipattern_create (enum rspamd_multipattern_flags flags)
{
	struct rspamd_multipattern *mp;

	mp = g_slice_alloc0 (sizeof (*mp));
	mp->flags = flags;

#ifdef WITH_HYPERSCAN
	mp->hs_pats = g_array_new (FALSE, TRUE, sizeof (gchar *));
	mp->hs_flags = g_array_new (FALSE, TRUE, sizeof (gint));
	mp->hs_ids = g_array_new (FALSE, TRUE, sizeof (gint));
#else
	mp->pats = g_array_new (FALSE, TRUE, sizeof (ac_trie_pat_t));
#endif

	return mp;
}

struct rspamd_multipattern *
rspamd_multipattern_create_sized (guint npatterns,
		enum rspamd_multipattern_flags flags)
{
	struct rspamd_multipattern *mp;

	g_assert (npatterns > 0);

	mp = g_slice_alloc0 (sizeof (*mp));
	mp->flags = flags;

#ifdef WITH_HYPERSCAN
	mp->hs_pats = g_array_sized_new (FALSE, TRUE, sizeof (gchar *), npatterns);
	mp->hs_flags = g_array_sized_new (FALSE, TRUE, sizeof (gint), npatterns);
	mp->hs_ids = g_array_sized_new (FALSE, TRUE, sizeof (gint), npatterns);
#else
	mp->pats = g_array_sized_new (FALSE, TRUE, sizeof (ac_trie_pat_t), npatterns);
#endif

	return mp;
}

void
rspamd_multipattern_add_pattern (struct rspamd_multipattern *mp,
		const gchar *pattern)
{
	g_assert (pattern != NULL);
	g_assert (mp != NULL);
	g_assert (!mp->compiled);

#ifdef WITH_HYPERSCAN
	gchar *np;
	gint fl = 0;

	if (mp->flags & RSPAMD_MULTIPATTERN_ICASE) {
		fl |= HS_FLAG_CASELESS;
	}
	if (mp->flags & RSPAMD_MULTIPATTERN_UTF8) {
		fl |= HS_FLAG_UTF8;
	}

	g_array_append_val (mp->hs_flags, fl);
	np = rspamd_multipattern_pattern_filter (pattern, mp->flags);
	g_array_append_val (mp->hs_pats, np);
	fl = mp->cnt;
	g_array_append_val (mp->hs_ids, fl);
#else
	ac_trie_pat_t pat;

	pat.ptr = rspamd_multipattern_pattern_filter (pattern, mp->flags);
	pat.len = strlen (pat.ptr);

	g_array_append_val (mp->pats, pat);
#endif

	mp->cnt ++;
}

struct rspamd_multipattern *
rspamd_multipattern_create_full (const gchar **patterns,
		guint npatterns, enum rspamd_multipattern_flags flags)
{
	struct rspamd_multipattern *mp;
	guint i;

	g_assert (npatterns > 0);
	g_assert (patterns != NULL);

	mp = rspamd_multipattern_create_sized (npatterns, flags);

	for (i = 0; i < npatterns; i++) {
		rspamd_multipattern_add_pattern (mp, patterns[i]);
	}

	return mp;
}

gboolean
rspamd_multipattern_compile (struct rspamd_multipattern *mp, GError **err)
{
	g_assert (mp != NULL);
	g_assert (!mp->compiled);

#ifdef WITH_HYPERSCAN
	hs_platform_info_t plt;
	hs_compile_error_t *hs_errors;

	g_assert (hs_populate_platform (&plt) == HS_SUCCESS);

	if (hs_compile_multi ((const char *const *)mp->hs_pats->data,
			(const unsigned int *)mp->hs_flags->data,
			(const unsigned int *)mp->hs_ids->data,
			mp->cnt,
			HS_MODE_BLOCK,
			&plt,
			&mp->db,
			&hs_errors) != HS_SUCCESS) {

		g_set_error (err, rspamd_multipattern_quark (), EINVAL,
				"cannot create tree of regexp when processing '%s': %s",
				g_array_index (mp->hs_pats, char *, hs_errors->expression),
				hs_errors->message);
		hs_free_compile_error (hs_errors);

		return FALSE;
	}

	g_assert (hs_alloc_scratch (mp->db, &mp->scratch) == HS_SUCCESS);
#else
	mp->t = acism_create (mp->pats->data, mp->cnt);
#endif
	mp->compiled = TRUE;

	return TRUE;
}

struct rspamd_multipattern_cbdata {
	struct rspamd_multipattern *mp;
	const gchar *in;
	gsize len;
	rspamd_multipattern_cb_t cb;
	gpointer ud;
	guint nfound;
	gint ret;
};

#ifdef WITH_HYPERSCAN
static gint
rspamd_multipattern_hs_cb (unsigned int id,
		unsigned long long from,
		unsigned long long to,
		unsigned int flags,
		void *ud)
{
	struct rspamd_multipattern_cbdata *cbd = ud;
	gint ret;

	ret = cbd->cb (cbd->mp, id, to, cbd->in, cbd->len, cbd->ud);

	cbd->nfound ++;
	cbd->ret = ret;

	return ret;
}
#else
static gint
rspamd_multipattern_acism_cb (int strnum, int textpos, void *context)
{
	struct rspamd_multipattern_cbdata *cbd = context;
	gint ret;

	ret = cbd->cb (cbd->mp, strnum, textpos, cbd->in, cbd->len, cbd->ud);

	cbd->nfound ++;
	cbd->ret = ret;

	return ret;
}
#endif

gint
rspamd_multipattern_lookup (struct rspamd_multipattern *mp,
		const gchar *in, gsize len, rspamd_multipattern_cb_t cb,
		gpointer ud, guint *pnfound)
{
	struct rspamd_multipattern_cbdata cbd;
	gint ret = 0;

	g_assert (mp != NULL);
	g_assert (mp->compiled);

	cbd.mp = mp;
	cbd.in = in;
	cbd.len = len;
	cbd.cb = cb;
	cbd.ud = ud;
	cbd.nfound = 0;
	cbd.ret = 0;

#ifdef WITH_HYPERSCAN
	ret = hs_scan (mp->db, in, len, 0, mp->scratch,
			rspamd_multipattern_hs_cb, &cbd);

	if (ret == HS_SUCCESS) {
		ret = 0;
	}
	else if (ret == HS_SCAN_TERMINATED) {
		ret = cbd.ret;
	}
#else
	gint state = 0;

	ret = acism_lookup (mp->t, in, len, rspamd_multipattern_acism_cb, &cbd,
			&state, mp->flags & RSPAMD_MULTIPATTERN_ICASE);
#endif

	if (pnfound) {
		*pnfound = cbd.nfound;
	}

	return ret;
}


void
rspamd_multipattern_destroy (struct rspamd_multipattern *mp)
{
	guint i;

	if (mp) {
#ifdef WITH_HYPERSCAN
		gchar *p;

		if (mp->compiled) {
			hs_free_scratch (mp->scratch);
			hs_free_database (mp->db);
		}

		for (i = 0; i < mp->cnt; i ++) {
			p = g_array_index (mp->hs_pats, gchar *, i);
			g_free (p);
		}

		g_array_free (mp->hs_pats, TRUE);
		g_array_free (mp->hs_ids, TRUE);
		g_array_free (mp->hs_flags, TRUE);
#else
		ac_trie_pat_t pat;

		if (mp->compiled) {
			acism_destroy (mp->t);
		}

		for (i = 0; i < mp->cnt; i ++) {
			pat = g_array_index (mp->pats, ac_trie_pat_t, i);
			g_free ((gchar *)pat.ptr);
		}

		g_array_free (mp->pats, TRUE);
#endif
		g_slice_free1 (sizeof (*mp), mp);
	}
}