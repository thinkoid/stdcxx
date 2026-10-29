# The locale sources are glibc 2.2's: a comparison with the current localedata

*Apache stdcxx revival notes, October 2026.*

The locale definitions and charmaps under `etc/nls` came with the
library as copies of GNU libc 2.2's, and nothing has touched their
data since 2007. This note compares them with the C library's current
sources and records what the comparison found: where the data has
moved, what the shared tables say about their own age, and whether
the library's `localedef` can read the current sources at all. It is
a survey of source values, not a defect list; its counts are
differences, and each becomes a defect only when a facet is shown to
decode the old value. The queue entry "The locale sources under
`etc/nls` are glibc 2.2's" in `TODO` carries the ordered refresh the
survey suggests.

## 0. tl;dr

- `etc/nls/src` and `etc/nls/charmaps` are glibc 2.2's, as the
  original `README` says. The last change to the sources is 312ef830
  (2007), a license header. The compared glibc tree is 60a2ae4098
  (2026-09-29), whose `version.h` says 2.44.9000.
- Coverage: 179 files against 372, 163 language and territory
  definitions against 351. 139 names match outright, 20 more once
  the `.euro` suffix maps to `@euro`. Four names are legacy and ours
  only; 192 are glibc's only.
- Among the 139 shared definitions, currencies, numeric punctuation,
  date formats, month and weekday names, the Japanese eras and the
  Romanian collation tailoring all differ in part. The Euro modifiers
  exist on our side; the unmodified names still select the national
  currencies of 2001.
- The shared tables are older than the definitions: `i18n` covers
  Unicode 3.0, the UTF-8 charmap's widths follow Unicode 3.2, against
  Unicode 14.0.0 in glibc's generated `i18n_ctype`.
- `localedef` builds the old `en_US` and rejects the current one:
  Error 312, symbolic name `<S02D0>` not found, in the collation
  template. Its keyword table lacks the newer keywords.
- A refresh starts with the compiler, then the shared tables with
  their charmaps, then the definitions with a mapping for the legacy
  names, then regenerated databases compared with the glibc source
  baseline.

## 1. Scope and provenance

The comparison is of `etc/nls/src` at 8f02f6bd against
`localedata/locales` of a glibc checkout at 60a2ae4098 (2026-09-29;
`version.h` says 2.44.9000). Both are snapshots; the note makes no
claim about the latest upstream tip, and the glibc tree is not a
guarantee that every datum in it is current.

The original `README` (section on the locale utilities) identifies
the supplied definitions and charmaps as copies of those distributed
with GNU libc 2.2. The git history of `etc/nls/src` ends at 312ef830
(2007-09-28), whose sampled `ro_RO` change adds the license header;
22bf3eaf (2006-08-26) repairs two euro-variant copy targets. No later
change touches the data.

The compatibility probe of chapter 6 used the `localedef` of a 15D
build at the same commit, with the uncommitted constructor change
that became e2b9a550; the change clears output images and does not
touch parsing.

## 2. Coverage

| | stdcxx | glibc |
|---|---|---|
| All files in the source directory | 179 | 372 |
| Language and territory definitions | 163 | 351 |
| Shared helper files | 15 | 19 |
| C and POSIX definitions | 1 | 2 |

Language and territory definitions include modifiers and glibc's bare
`eo`, `syr` and `tok`. Helper files are `i18n`, the collation
templates and the transliteration sources. The counts are files, not
distinct languages and not built locale and encoding combinations.

139 ordinary definitions match by filename. Twenty of ours end in
`.euro` and match glibc's `@euro` names once the suffix is converted.
The four remaining names are ours only: `iw_IL`, `no_NO`, `sr_YU` and
`sr_YU.cyrillic`. They are legacy language and territory codes, and
their data is not automatically that of a modern alias; `iw_IL`
overlaps the Hebrew that `he_IL` already carries. After the suffix
conversion 192 names are glibc's only; chapter 8 lists them. Among
them: `bn_BD`, `bo_CN`, `chr_US`, `gu_IN`, `kk_KZ`, `km_KH`, `kn_IN`,
`lo_LA`, `ml_IN`, `my_MM`, `ne_NP`, `pa_IN`, `si_LK`, `sw_KE`,
`ur_IN`, `yo_NG`, `zu_ZA`.

## 3. Monetary and numeric data

Over the 139 shared definitions, after resolving the category `copy`
directives and decoding `<Uxxxx>` strings:

| field | definitions that differ |
|---|---|
| `int_curr_symbol` | 46 |
| `currency_symbol` | 75 |
| numeric `thousands_sep` | 54 |
| numeric `decimal_point` | 8 |
| numeric `grouping` | 45 |

Examples, ours to glibc's:

- `ro_RO`: ROL to RON; no numeric separator and no grouping, to a dot
  and groups of three.
- `fr_FR`: FRF and F to EUR and the euro sign; an empty numeric
  separator to U+202F.
- `de_DE`: DEM and DM to EUR and the euro sign.
- `tr_TR`: TRL and TL to TRY and the lira sign; the numeric decimal
  point a dot, to a comma.
- `ru_RU`: RUR to RUB. `hr_HR`: HRK to EUR. `bg_BG`: BGL to EUR.

The Euro modifiers already exist on our side, so a program that asks
for `de_DE.ISO-8859-15@euro` gets the euro. The unmodified names
still select the national currencies. That limits the finding and
does not remove it. The sites: `etc/nls/src/ro_RO:154` has ROL, the
glibc `ro_RO:161` has RON; `etc/nls/src/tr_TR:3473` has the dot, the
glibc `tr_TR:2948` the comma.

## 4. Dates, eras and collation

Over the same 139 definitions, `d_fmt` differs in 36, the full month
names in 45, the full weekday names in 24. These are source values;
two date directives can have the same effect, so the count is not 36
proven defects.

`ro_RO` is the worked example. Its `d_fmt` moves from `%Y-%m-%d` to
`%d.%m.%Y`. Capitalized names with the wrong letters, `DuminicĂ` and
`SîmbĂtĂ`, become `duminică` and `sâmbătă`, with the comma-below `ț`.
Its collation tailoring orders a, â, ă on our side (`ro_RO:87`,
circumflex before breve) and a, ă, â in glibc's (`ro_RO:85`). The
survey compared the tailoring rules and ran no collation oracle.

`ja_JP` (`etc/nls/src/ja_JP:14962`) leaves the Heisei era open-ended
and has no Reiwa entry; glibc's (`ja_JP:14967`) closes Heisei at
2019-04-30 and adds Reiwa.

## 5. Unicode and the shared templates

`etc/nls/src/i18n:58` says it covers Unicode 3.0. glibc's
`i18n_ctype:16` says it was generated for Unicode 14.0.0. The labels
are reported as written, not validated against a Unicode release.

The UTF-8 charmap (`etc/nls/charmaps/UTF-8:28309`) says its width
data follows Unicode 3.2. It has no entry for U+1C90 (Georgian
capital AN), U+1E900 (Adlam capital ALIF) or U+1F600 (grinning face);
glibc's charmap has all three. Absence from the charmap is a table
gap; it does not say which runtime conversion paths reject the
characters.

glibc's `i18n_ctype` carries the U+10D0 to U+1C90 uppercase mapping.
Its shared collation template says it was generated from
`unidata-9.0.0.txt` and is about 3.3 MB; our `iso14651_t1` is about
48 KB. Size is not a correctness measure.

## 6. The compiler and the runtime's reach

`localedef` handles the six C++ locale categories
(`util/def.cpp:480` dispatches them). The GNU categories outside the
standard facets, paper, address, measurement and the rest, are not a
conformance gap and their data is not counted here.

The keyword table in `util/scanner.cpp` (from line 231) lacks
`alt_mon`, `ab_alt_mon`, `class`, `map` and glibc's conditionals.
glibc's `ru_RU:121` and its peers use the stand-alone month names.

Two probes of `en_US`, each with `-c`, an absolute `-f` UTF-8 charmap
and an absolute `-i` source path:

- Our sources and charmap: exit 0, warnings, six category databases.
- glibc's sources and charmap: exit 4, Error 312, symbolic name
  `<S02D0>` not found at `iso14651_t1_common:63100`. A second run
  into an empty directory, to rule out reuse of the shared `i18n`
  ctype database, exits 4 with the same error. The symbol is declared
  as a `collating-symbol` at line 8234 of the same file; why the
  compiler does not see it is the investigation's first question.

What the stale sources reach at run time is bounded. The facets can
use the library's own databases or the C library's locales, the
choice varying by facet and by the options a program builds with
(`doc/notes/ref-locale.md`, the backend chapter). Stale sources do not
imply that every `std::locale` a program constructs on a current host
decodes stale data. The converse holds too: refreshing the C library's
installed locales refreshes nothing of ours.

## 7. Method

The comparison strips comments, joins continued lines, decodes the
`<Uxxxx>` symbols and escapes, and resolves category `copy`
directives recursively. Groupings `3;3` and `3` count as the same
repeating grouping. It evaluates no other `localedef` construct,
compares no compiled database, and proves no equivalence of date
format strings or regular expressions. The scalar census covers
fields present on both sides.

The refresh the survey suggests, in order: characterize what in the
current sources the compiler rejects and extend it until glibc's
`en_US` builds; refresh the shared ctype, transliteration and
collation templates together with their charmaps, since new leaf
definitions over old shared tables leave the tables stale; refresh
the definitions with an explicit mapping for the legacy names;
regenerate the databases and compare the decoded facet values with
the glibc source baseline, the currency, numeric and date repairs
staged separately.

## 8. The 192 glibc-only definitions

After the `.euro` suffix conversion:

aa_DJ, aa_ER, aa_ET, ab_GE, agr_PE, ak_GH, an_ES, anp_IN, ar_SS,
as_IN, ast_ES, ayc_PE, az_IR, be_BY@latin, bem_ZM, ber_DZ, ber_MA,
bhb_IN, bho_IN, bho_NP, bi_VU, bn_BD, bo_CN, bo_IN, br_FR@euro,
brh_PK, brx_IN, byn_ER, ca_AD, ca_ES@valencia, ca_FR, ca_IT, ce_RU,
chr_US, ckb_IQ, cmn_TW, crh_RU, crh_UA, csb_PL, cv_RU, de_IT, de_LI,
doi_IN, dsb_DE, dv_MV, dz_BT, el_CY, en_AG, en_IL, en_NG, en_SC,
en_SE, en_ZM, eo, es_CU, ff_SN, fil_PH, fur_IT, fy_DE, fy_NL, gbm_IN,
gez_ER, gez_ER@abegede, gez_ET, gez_ET@abegede, gu_IN, ha_NG, hak_TW,
hif_FJ, hne_IN, hrx_BR, hsb_DE, ht_HT, ia_FR, ig_NG, ik_CA, iu_CA,
kab_DZ, kk_KZ, km_KH, kn_IN, kok_IN, ks_IN, ks_IN@devanagari, ku_TR,
kv_RU, ky_KG, lb_LU, lg_UG, li_BE, li_NL, lij_IT, ln_CD, lo_LA,
ltg_LV, lzh_TW, mag_IN, mai_IN, mai_NP, mdf_RU, mfe_MU, mg_MG, mhr_RU,
miq_NI, mjw_IN, ml_IN, mn_MN, mni_IN, mnw_MM, my_MM, nan_TW,
nan_TW@latin, nb_NO, nds_DE, nds_NL, ne_NP, nhn_MX, niu_NU, niu_NZ,
nl_AW, nr_ZA, nso_ZA, om_ET, om_KE, or_IN, os_RU, pa_IN, pa_PK,
pap_AW, pap_CW, ps_AF, quz_PE, raj_IN, rif_MA, rw_RW, sa_IN, sah_RU,
sat_IN, sc_IT, scn_IT, sd_IN, sd_IN@devanagari, sgs_LT, shn_MM,
shs_CA, si_LK, sid_ET, sm_WS, so_DJ, so_ET, so_KE, so_SO, sq_MK,
sr_ME, sr_RS, sr_RS@latin, ss_ZA, ssy_ER, st_ZA, su_ID, sw_KE, sw_TZ,
syr, szl_PL, ta_LK, tcy_IN, the_NP, tig_ER, tk_TM, tn_ZA, to_TO, tok,
tpi_PG, tr_CY, ts_ZA, tt_RU@iqtelif, ug_CN, unm_US, ur_IN,
uz_UZ@cyrillic, ve_ZA, wa_BE, wa_BE@euro, wae_CH, wal_ET, wo_SN,
xh_ZA, yo_NG, yue_HK, yuw_PG, zgh_MA, zu_ZA.

## 9. Outcome

cb1f2768 replaced the sources. `etc/nls/src` and `etc/nls/charmaps`
hold the `localedata` of the installed package, glibc
2.44+r50+g1848099f063e-1, which is glibc commit 1848099f063e, byte
for byte. That is a different snapshot from the 60a2ae4098 this
survey compared with. The 139 shared definitions carry glibc's
values now; the counts of chapters 3 and 4 describe the tree before
the import. The four legacy names are gone, 52a86884.
`doc/notes/plan-locale-sources.md` records the steps and their
checks. `bin/oracle/scalar.cpp` compares the decoded numeric,
monetary and time values with the C library's; the plan's chapter 5
has its first run.
