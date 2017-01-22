/*-
 * Copyright (c) 2001 Atsushi Onoe
 * Copyright (c) 2002-2005 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: ieee80211.c 2617 2007-07-26 14:38:46Z mrenzmann $
 */
#ifndef EXPORT_SYMTAB
#define	EXPORT_SYMTAB
#endif

/*
 * IEEE 802.11 generic handler
 */
#ifndef AUTOCONF_INCLUDED
#include <linux/config.h>
#endif
#include <linux/version.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/rtnetlink.h>		/* XXX for rtnl_lock */

#include <asm/board/pm.h>

#include "net80211/if_media.h"

#include "net80211/ieee80211_var.h"
#include "net80211/_ieee80211.h"
#include "net80211/ieee80211_tpc.h"

#include <qtn/qtn_buffers.h>
#include <qtn/qtn_global.h>
#include <qdrv/qdrv_vap.h>

const char *ieee80211_phymode_name[] = {
	"auto",		/* IEEE80211_MODE_AUTO */
	"11a",		/* IEEE80211_MODE_11A */
	"11b",		/* IEEE80211_MODE_11B */
	"11g",		/* IEEE80211_MODE_11G */
	"FH",		/* IEEE80211_MODE_FH */
	"turboA",	/* IEEE80211_MODE_TURBO_A */
	"turboG",	/* IEEE80211_MODE_TURBO_G */
	"11na",		/* IEEE80211_MODE_11NA */
	"11ng",		/* IEEE80211_MODE_11NG */
	"11ng40",	/* IEEE80211_MODE_11NG_HT40PM */
	"11na40",	/* IEEE80211_MODE_11NA_HT40PM */
	"11ac20",	/* IEEE80211_MODE_11AC_VHT20PM */
	"11ac40",	/* IEEE80211_MODE_11AC_VHT40PM */
	"11ac80",	/* IEEE80211_MODE_11AC_VHT80PM */
	"11ac160",	/* IEEE80211_MODE_11AC_VHT160PM */
};
EXPORT_SYMBOL(ieee80211_phymode_name);

/* integer portion of HT rates */
const u_int16_t ht_rate_table_20MHz_400[] = {
							7,
							14,
							21,
							28,
							43,
							57,
							65,
							72,
							14,
							28,
							43,
							57,
							86,
							115,
							130,
							144
						};

const u_int16_t ht_rate_table_20MHz_800[] = {
							6,
							13,
							19,
							26,
							39,
							52,
							58,
							65,
							13,
							26,
							39,
							52,
							78,
							104,
							117,
							130
						};

const u_int16_t ht_rate_table_40MHz_400[] = {
							15,
							30,
							45,
							60,
							90,
							120,
							135,
							150,
							30,
							60,
							90,
							120,
							180,
							240,
							270,
							300
						};

const u_int16_t ht_rate_table_40MHz_800[] = {
							13,
							27,
							40,
							54,
							81,
							108,
							121,
							135,
							27,
							54,
							81,
							108,
							162,
							216,
							243,
							270
						};

EXPORT_SYMBOL(ht_rate_table_40MHz_800);
EXPORT_SYMBOL(ht_rate_table_40MHz_400);
EXPORT_SYMBOL(ht_rate_table_20MHz_800);
EXPORT_SYMBOL(ht_rate_table_20MHz_400);

/* Please update it when the definition of ieee80211_phymode changed */
static const u_int ieee80211_chanflags[] = {
	0,				/* IEEE80211_MODE_AUTO */
	IEEE80211_CHAN_A,		/* IEEE80211_MODE_11A */
	IEEE80211_CHAN_B,		/* IEEE80211_MODE_11B */
	IEEE80211_CHAN_PUREG,		/* IEEE80211_MODE_11G */
	IEEE80211_CHAN_FHSS,		/* IEEE80211_MODE_FH */
	IEEE80211_CHAN_108A,		/* IEEE80211_MODE_TURBO_A */
	IEEE80211_CHAN_108G,		/* IEEE80211_MODE_TURBO_G */
	IEEE80211_CHAN_11NA,		/* IEEE80211_MODE_11NA */
	IEEE80211_CHAN_11NG,		/* IEEE80211_MODE_11NG */
	IEEE80211_CHAN_11NG_HT40,	/* IEEE80211_MODE_11NG_HT40PM */
	IEEE80211_CHAN_11NA_HT40,	/* IEEE80211_MODE_11NA_HT40PM */
	IEEE80211_CHAN_11AC,
	IEEE80211_CHAN_11AC_VHT40,
	IEEE80211_CHAN_11AC_VHT80,
};

static void ieee80211com_media_status(void *, struct ifmediareq *);
static int ieee80211com_media_change(void *);
static struct net_device_stats *ieee80211_getstats(struct net_device *);
static int ieee80211_change_mtu(struct net_device *, int);
static void ieee80211_set_multicast_list(struct net_device *);

MALLOC_DEFINE(M_80211_VAP, "80211vap", "802.11 vap state");

/*
 * Country Code Table for code-to-string conversion.
 */
struct country_code_to_string {
	u_int16_t iso_code;
	const char *iso_name;
};

static const  struct country_code_to_string country_strings[] = {
    {CTRY_DEBUG,		"DB" },
    {CTRY_DEFAULT,		"NA" },
    {CTRY_AFGHANISTAN,		"AF" },
    {CTRY_ALBANIA,		"AL" },
    {CTRY_ALGERIA,		"DZ" },
    {CTRY_AMERICAN_SAMOA,	"AS" },
    {CTRY_ANDORRA,		"AD" },
    {CTRY_ANGOLA,		"AO" },
    {CTRY_ANGUILLA,		"AI" },
    {CTRY_ANTARTICA,		"AQ" },
    {CTRY_ANTIGUA,		"AG" },
    {CTRY_ARGENTINA,		"AR" },
    {CTRY_ARMENIA,		"AM" },
    {CTRY_ARUBA,		"AW" },
    {CTRY_AUSTRALIA,		"AU" },
    {CTRY_AUSTRIA,		"AT" },
    {CTRY_AZERBAIJAN,		"AZ" },
    {CTRY_BAHAMAS,		"BS" },
    {CTRY_BAHRAIN,		"BH" },
    {CTRY_BANGLADESH,		"BD" },
    {CTRY_BARBADOS,		"BB" },
    {CTRY_BELARUS,		"BY" },
    {CTRY_BELGIUM,		"BE" },
    {CTRY_BELIZE,		"BZ" },
    {CTRY_BENIN,		"BJ" },
    {CTRY_BERMUDA,		"BM" },
    {CTRY_BHUTAN,		"BT" },
    {CTRY_BOLIVIA,		"BO" },
    {CTRY_BOSNIA_AND_HERZEGOWINA,	"BA" },
    {CTRY_BOTSWANA,		"BW" },
    {CTRY_BOUVET_ISLAND,	"BV" },
    {CTRY_BRAZIL,		"BR" },
    {CTRY_BRITISH_INDIAN_OCEAN_TERRITORY,	"IO" },
    {CTRY_BRUNEI_DARUSSALAM,	"BN" },
    {CTRY_BULGARIA,		"BG" },
    {CTRY_BURKINA_FASO,		"BF" },
    {CTRY_BURUNDI,		"BI" },
    {CTRY_CAMBODIA,		"KH" },
    {CTRY_CAMEROON,		"CM" },
    {CTRY_CANADA,		"CA" },
    {CTRY_CAPE_VERDE,		"CV" },
    {CTRY_CAYMAN_ISLANDS,	"KY" },
    {CTRY_CENTRAL_AFRICAN_REPUBLIC,	"CF" },
    {CTRY_CHAD,			"TD" },
    {CTRY_CHILE,		"CL" },
    {CTRY_CHINA,		"CN" },
    {CTRY_CHRISTMAS_ISLAND,	"CX" },
    {CTRY_COCOS_ISLANDS,	"CC" },
    {CTRY_COLOMBIA,		"CO" },
    {CTRY_COMOROS,		"KM" },
    {CTRY_CONGO,		"CG" },
    {CTRY_COOK_ISLANDS,		"CK" },
    {CTRY_COSTA_RICA,		"CR" },
    {CTRY_COTE_DIVOIRE,		"CI" },
    {CTRY_CROATIA,		"HR" },
    {CTRY_CYPRUS,		"CY" },
    {CTRY_CZECH,		"CZ" },
    {CTRY_DENMARK,		"DK" },
    {CTRY_DJIBOUTI,		"DJ" },
    {CTRY_DOMINICA,		"DM" },
    {CTRY_DOMINICAN_REPUBLIC,	"DO" },
    {CTRY_ECUADOR,		"EC" },
    {CTRY_EGYPT,		"EG" },
    {CTRY_EL_SALVADOR,		"SV" },
    {CTRY_EQUATORIAL_GUINEA,	"GQ" },
    {CTRY_ERITREA,		"ER" },
    {CTRY_ESTONIA,		"EE" },
    {CTRY_ETHIOPIA,		"ET" },
    {CTRY_FALKLAND_ISLANDS,	"FK" },
    {CTRY_EUROPE,		"EU" },
    {CTRY_FIJI,			"FJ" },
    {CTRY_FINLAND,		"FI" },
    {CTRY_FRANCE,		"FR" },
    {CTRY_FRANCE2,		"F2" },
    {CTRY_FRENCH_GUIANA,	"GF" },
    {CTRY_FRENCH_POLYNESIA,	"PF" },
    {CTRY_FRENCH_SOUTHERN_TERRITORIES,	"TF" },
    {CTRY_GABON,		"GA" },
    {CTRY_GAMBIA,		"GM" },
    {CTRY_GEORGIA,		"GE" },
    {CTRY_GERMANY,		"DE" },
    {CTRY_GHANA,		"GH" },
    {CTRY_GIBRALTAR,		"GI" },
    {CTRY_GREECE,		"GR" },
    {CTRY_GREENLAND,		"GL" },
    {CTRY_GRENADA,		"GD" },
    {CTRY_GUADELOUPE,		"GP" },
    {CTRY_GUAM,			"GU" },
    {CTRY_GUATEMALA,		"GT" },
    {CTRY_GUINEA,		"GN" },
    {CTRY_GUINEA_BISSAU,	"GW" },
    {CTRY_GUYANA,		"GY" },
    {CTRY_HAITI,		"HT" },
    {CTRY_HONDURAS,		"HN" },
    {CTRY_HONG_KONG,		"HK" },
    {CTRY_HUNGARY,		"HU" },
    {CTRY_ICELAND,		"IS" },
    {CTRY_INDIA,		"IN" },
    {CTRY_INDONESIA,		"ID" },
    {CTRY_IRAN,			"IR" },
    {CTRY_IRELAND,		"IE" },
    {CTRY_ISRAEL,		"IL" },
    {CTRY_ITALY,		"IT" },
    {CTRY_JAPAN,		"JP" },
    {CTRY_JAPAN1,		"J1" },
    {CTRY_JAPAN2,		"J2" },    
    {CTRY_JAPAN3,		"J3" },
    {CTRY_JAPAN4,		"J4" },
    {CTRY_JAPAN5,		"J5" },    
    {CTRY_JAPAN7,		"JP" },
    {CTRY_JAPAN6,		"JP" },
    {CTRY_JAPAN8,		"JP" },
    {CTRY_JAPAN9,	      	"JP" },
    {CTRY_JAPAN10,	      	"JP" }, 
    {CTRY_JAPAN11,	      	"JP" },
    {CTRY_JAPAN12,	      	"JP" },
    {CTRY_JAPAN13,	      	"JP" },
    {CTRY_JAPAN14,	      	"JP" },
    {CTRY_JAPAN15,	      	"JP" },
    {CTRY_JAPAN16,	      	"JP" }, 
    {CTRY_JAPAN17,	      	"JP" },
    {CTRY_JAPAN18,	      	"JP" },
    {CTRY_JAPAN19,	      	"JP" },
    {CTRY_JAPAN20,	      	"JP" },
    {CTRY_JAPAN21,	      	"JP" }, 
    {CTRY_JAPAN22,	      	"JP" },
    {CTRY_JAPAN23,	      	"JP" },
    {CTRY_JAPAN24,	      	"JP" },
    {CTRY_JAPAN25,	      	"JP" }, 
    {CTRY_JAPAN26,	      	"JP" },
    {CTRY_JAPAN27,	      	"JP" },
    {CTRY_JAPAN28,	      	"JP" },
    {CTRY_JAPAN29,	      	"JP" },
    {CTRY_JAPAN30,      	"JP" },
    {CTRY_JAPAN31,      	"JP" },
    {CTRY_JAPAN32,      	"JP" },
    {CTRY_JAPAN33,      	"JP" },
    {CTRY_JAPAN34,      	"JP" },
    {CTRY_JAPAN35,      	"JP" },
    {CTRY_JAPAN36,      	"JP" },
    {CTRY_JAPAN37,      	"JP" },
    {CTRY_JAPAN38,      	"JP" },
    {CTRY_JAPAN39,      	"JP" },
    {CTRY_JAPAN40,      	"JP" },
    {CTRY_JAPAN41,      	"JP" },
    {CTRY_JAPAN42,      	"JP" },
    {CTRY_JAPAN43,      	"JP" },
    {CTRY_JAPAN44,      	"JP" },
    {CTRY_JAPAN45,      	"JP" },
    {CTRY_JAPAN46,      	"JP" },
    {CTRY_JAPAN47,      	"JP" },
    {CTRY_JAPAN48,      	"JP" },
    {CTRY_JORDAN,		"JO" },
    {CTRY_KAZAKHSTAN,		"KZ" },
    {CTRY_KOREA_NORTH,		"KP" },
    {CTRY_KOREA_ROC,		"KR" },
    {CTRY_KOREA_ROC2,		"K2" },
    {CTRY_KUWAIT,		"KW" },
    {CTRY_LATVIA,		"LV" },
    {CTRY_LEBANON,		"LB" },
    {CTRY_LIECHTENSTEIN,	"LI" },
    {CTRY_LITHUANIA,		"LT" },
    {CTRY_LUXEMBOURG,		"LU" },
    {CTRY_MACAU,		"MO" },
    {CTRY_MACEDONIA,		"MK" },
    {CTRY_MALAYSIA,		"MY" },
    {CTRY_MEXICO,		"MX" },
    {CTRY_MONACO,		"MC" },
    {CTRY_MOROCCO,		"MA" },
    {CTRY_NEPAL,		"NP" },
    {CTRY_NETHERLANDS,		"NL" },
    {CTRY_NEW_ZEALAND,		"NZ" },
    {CTRY_NORWAY,		"NO" },
    {CTRY_OMAN,			"OM" },
    {CTRY_PAKISTAN,		"PK" },
    {CTRY_PANAMA,		"PA" },
    {CTRY_PERU,			"PE" },
    {CTRY_PHILIPPINES,		"PH" },
    {CTRY_POLAND,		"PL" },
    {CTRY_PORTUGAL,		"PT" },
    {CTRY_PUERTO_RICO,		"PR" },
    {CTRY_QATAR,		"QA" },
    {CTRY_ROMANIA,		"RO" },
    {CTRY_RUSSIA,		"RU" },
    {CTRY_SAUDI_ARABIA,		"SA" },
    {CTRY_SINGAPORE,		"SG" },
    {CTRY_SLOVAKIA,		"SK" },
    {CTRY_SLOVENIA,		"SI" },
    {CTRY_SOUTH_AFRICA,		"ZA" },
    {CTRY_SPAIN,		"ES" },
    {CTRY_SRILANKA,		"LK" },
    {CTRY_SWEDEN,		"SE" },
    {CTRY_SWITZERLAND,		"CH" },
    {CTRY_SYRIA,		"SY" },
    {CTRY_TAIWAN,		"TW" },
    {CTRY_THAILAND,		"TH" },
    {CTRY_TRINIDAD_Y_TOBAGO,	"TT" },
    {CTRY_TUNISIA,		"TN" },
    {CTRY_TURKEY,		"TR" },
    {CTRY_UKRAINE,		"UA" },
    {CTRY_UAE,			"AE" },
    {CTRY_UNITED_KINGDOM,	"GB" },
    {CTRY_UNITED_STATES,	"US" },
    {CTRY_UNITED_STATES_FCC49,	"US" },
    {CTRY_URUGUAY,		"UY" },
    {CTRY_UZBEKISTAN,		"UZ" },
    {CTRY_VENEZUELA,		"VE" },
    {CTRY_VIET_NAM,		"VN" },
    {CTRY_YEMEN,		"YE" },
    {CTRY_ZIMBABWE,		"ZW" }
};

static const struct operating_class_table us_oper_class_table[] = {
	{1, 115, 20, {36,40,44,48}, 0},
	{2, 118, 20, {52,56,60,64}, IEEE80211_OC_BEHAV_DFS_50_100},
	{3, 124, 20, {149,153,157,161}, IEEE80211_OC_BEHAV_NOMADIC},
	{4, 121, 20, {100,104,108,112,116,120,124,128,132,136,140},
		IEEE80211_OC_BEHAV_DFS_50_100 | IEEE80211_OC_BEHAV_EIRP_TXPOWENV},
	{5, 125, 20, {149,153,157,161,165}, IEEE80211_OC_BEHAV_LICEN_EXEP},
	{12, 81, 25, {1,2,3,4,5,6,7,8,9,10,11}, IEEE80211_OC_BEHAV_LICEN_EXEP},
	{22, 116, 40, {36,44}, IEEE80211_OC_BEHAV_CHAN_LOWWER},
	{23, 119, 40, {52,60}, IEEE80211_OC_BEHAV_CHAN_LOWWER},
	{24, 122, 40, {100,108,116,124,132}, IEEE80211_OC_BEHAV_CHAN_LOWWER |
		IEEE80211_OC_BEHAV_DFS_50_100 | IEEE80211_OC_BEHAV_EIRP_TXPOWENV},
	{25, 126, 40, {149,157}, IEEE80211_OC_BEHAV_CHAN_LOWWER},
	{26, 126, 40, {149,157}, IEEE80211_OC_BEHAV_LICEN_EXEP | IEEE80211_OC_BEHAV_CHAN_LOWWER},
	{27, 117, 40, {40,48}, IEEE80211_OC_BEHAV_CHAN_UPPER},
	{28, 120, 40, {56,64}, IEEE80211_OC_BEHAV_CHAN_UPPER},
	{29, 123, 40, {104,112,120,128,136}, IEEE80211_OC_BEHAV_NOMADIC |
		IEEE80211_OC_BEHAV_CHAN_UPPER |	IEEE80211_OC_BEHAV_DFS_50_100 | IEEE80211_OC_BEHAV_EIRP_TXPOWENV},
	{30, 127, 40, {153,161}, IEEE80211_OC_BEHAV_NOMADIC | IEEE80211_OC_BEHAV_CHAN_UPPER},
	{31, 127, 40, {153,161}, IEEE80211_OC_BEHAV_LICEN_EXEP | IEEE80211_OC_BEHAV_CHAN_UPPER},
	{32, 83, 40, {1,2,3,4,5,6,7}, IEEE80211_OC_BEHAV_LICEN_EXEP | IEEE80211_OC_BEHAV_CHAN_LOWWER},
	{33, 84, 40, {5,6,7,8,9,10,11}, IEEE80211_OC_BEHAV_LICEN_EXEP | IEEE80211_OC_BEHAV_CHAN_UPPER},
	{128, 128, 80, {36,40,44,48,52,56,60,64,100,104,108,112,116,120,124,128,132,136,140,144,149,153,157,161},
		IEEE80211_OC_BEHAV_EIRP_TXPOWENV},
	{130, 130, 80, {36,40,44,48,52,56,60,64,100,104,108,112,116,120,124,128,132,136,140,144,149,153,157,161},
		IEEE80211_OC_BEHAV_80PLUS | IEEE80211_OC_BEHAV_EIRP_TXPOWENV},
};

static const struct operating_class_table eu_oper_class_table[] = {
	{1, 115, 20, {36,40,44,48},0},
	{2, 118, 20, {52,56,60,64}, IEEE80211_OC_BEHAV_NOMADIC},
	{3, 121, 20, {100,104,108,112,116,120,124,128,132,136,140}, 0},
	{4, 81, 25, {1,2,3,4,5,6,7,8,9,10,11,12,13}, 0},
	{5, 116, 40, {36,44}, IEEE80211_OC_BEHAV_CHAN_LOWWER},
	{6, 119, 40, {52,60}, IEEE80211_OC_BEHAV_CHAN_LOWWER},
	{7, 122, 40, {100,108,116,124,132}, IEEE80211_OC_BEHAV_CHAN_LOWWER},
	{8, 117, 40, {40,48}, IEEE80211_OC_BEHAV_CHAN_UPPER},
	{9, 120, 40, {56,64}, IEEE80211_OC_BEHAV_CHAN_UPPER},
	{10, 123, 40, {104,112,120,128,136}, IEEE80211_OC_BEHAV_CHAN_UPPER},
	{11, 83, 40, {1,2,3,4,5,6,7,8,9}, IEEE80211_OC_BEHAV_LICEN_EXEP | IEEE80211_OC_BEHAV_CHAN_LOWWER},
	{12, 84, 40, {5,6,7,8,9,10,11,12,13}, IEEE80211_OC_BEHAV_LICEN_EXEP | IEEE80211_OC_BEHAV_CHAN_UPPER},
	{128, 128, 80, {36,40,44,48,52,56,60,64,100,104,108,112,116,120,124,128},
		IEEE80211_OC_BEHAV_EIRP_TXPOWENV},
	{130, 130, 80, {36,40,44,48,52,56,60,64,100,104,108,112,116,120,124,128},
		IEEE80211_OC_BEHAV_80PLUS | IEEE80211_OC_BEHAV_EIRP_TXPOWENV},
};

static const struct operating_class_table jp_oper_class_table[] = {
	{1, 115, 20, {36,40,44,48}, 0},
	{30, 81, 25, {1,2,3,4,5,6,7,8,9,10,11,12,13}, IEEE80211_OC_BEHAV_LICEN_EXEP},
	{31, 82, 25, {14}, IEEE80211_OC_BEHAV_LICEN_EXEP},
	{32, 118, 20, {52,56,60,64}, 0},
	{33, 118, 20, {52,56,60,64}, 0},
	{34, 121, 20, {100,104,108,112,116,120,124,128,132,136,140}, IEEE80211_OC_BEHAV_DFS_50_100},
	{35, 121, 20, {100,104,108,112,116,120,124,128,132,136,140}, IEEE80211_OC_BEHAV_DFS_50_100},
	{36, 116, 40, {36,44}, IEEE80211_OC_BEHAV_CHAN_LOWWER},
	{37, 119, 40, {52,60}, IEEE80211_OC_BEHAV_CHAN_LOWWER},
	{38, 119, 40, {52,60}, IEEE80211_OC_BEHAV_CHAN_LOWWER},
	{39, 122, 40, {100,108,116,124,132}, IEEE80211_OC_BEHAV_CHAN_LOWWER|IEEE80211_OC_BEHAV_DFS_50_100},
	{40, 122, 40, {100,108,116,124,132}, IEEE80211_OC_BEHAV_CHAN_LOWWER|IEEE80211_OC_BEHAV_DFS_50_100},
	{41, 117, 40, {40,48}, IEEE80211_OC_BEHAV_CHAN_UPPER},
	{42, 120, 40, {56,64}, IEEE80211_OC_BEHAV_CHAN_UPPER},
	{43, 120, 40, {56,64}, IEEE80211_OC_BEHAV_CHAN_UPPER},
	{44, 123, 40, {104,112,120,128,136}, IEEE80211_OC_BEHAV_CHAN_UPPER|IEEE80211_OC_BEHAV_DFS_50_100},
	{45, 123, 40, {104,112,120,128,136}, IEEE80211_OC_BEHAV_CHAN_UPPER|IEEE80211_OC_BEHAV_DFS_50_100},
	{56, 83, 40, {1,2,3,4,5,6,7,8,9}, IEEE80211_OC_BEHAV_LICEN_EXEP | IEEE80211_OC_BEHAV_CHAN_LOWWER},
	{57, 84, 40, {5,6,7,8,9,10,11,12,13}, IEEE80211_OC_BEHAV_LICEN_EXEP | IEEE80211_OC_BEHAV_CHAN_UPPER},
	{128, 128, 80, {36,40,44,48,52,56,60,64,100,104,108,112,116,120,124,128},
		IEEE80211_OC_BEHAV_EIRP_TXPOWENV},
	{130, 130, 80, {36,40,44,48,52,56,60,64,100,104,108,112,116,120,124,128},
		IEEE80211_OC_BEHAV_80PLUS | IEEE80211_OC_BEHAV_EIRP_TXPOWENV},
};

static const struct operating_class_table gb_oper_class_table[] = {
	{81, 81, 25, {1,2,3,4,5,6,7,8,9,10,11,12,13}, 0},
	{82, 82, 25, {14}, 0},
	{83, 83, 40, {1,2,3,4,5,6,7,8,9}, IEEE80211_OC_BEHAV_CHAN_LOWWER},
	{84, 84, 40, {5,6,7,8,9,10,11,12,13}, IEEE80211_OC_BEHAV_CHAN_UPPER},
	{115, 115, 20, {36,40,44,48}, 0},
	{116, 116, 40, {36,44}, IEEE80211_OC_BEHAV_CHAN_LOWWER},
	{117, 117, 40, {40,48}, IEEE80211_OC_BEHAV_CHAN_UPPER},
	{118, 118, 20, {52,56,60,64}, IEEE80211_OC_BEHAV_DFS_50_100},
	{119, 119, 40, {52,60}, IEEE80211_OC_BEHAV_CHAN_LOWWER|IEEE80211_OC_BEHAV_DFS_50_100},
	{120, 120, 40, {56,64}, IEEE80211_OC_BEHAV_CHAN_UPPER|IEEE80211_OC_BEHAV_DFS_50_100},
	{121, 121, 20, {100,104,108,112,116,120,124,128,132,136,140,144}, IEEE80211_OC_BEHAV_DFS_50_100},
	{122, 122, 40, {100,108,116,124,132,140}, IEEE80211_OC_BEHAV_CHAN_LOWWER|IEEE80211_OC_BEHAV_DFS_50_100},
	{123, 123, 40, {104,112,120,128,136,144}, IEEE80211_OC_BEHAV_CHAN_UPPER|IEEE80211_OC_BEHAV_DFS_50_100},
	{124, 124, 20, {149,153,157,161}, IEEE80211_OC_BEHAV_NOMADIC},
	{126, 126, 40, {149,157}, IEEE80211_OC_BEHAV_CHAN_LOWWER},
	{127, 127, 40, {153,161}, IEEE80211_OC_BEHAV_CHAN_UPPER},
	{128, 128, 80, {36,40,44,48,52,56,60,64,100,104,108,112,116,120,124,128,132,136,140,144,149,153,157,161},
		IEEE80211_OC_BEHAV_EIRP_TXPOWENV},
	{130, 130, 80, {36,40,44,48,52,56,60,64,100,104,108,112,116,120,124,128,132,136,140,144,149,153,157,161},
		IEEE80211_OC_BEHAV_80PLUS | IEEE80211_OC_BEHAV_EIRP_TXPOWENV},
};

/* Make sure the global class entry must be the last one */
static const struct region_to_oper_class oper_class_table[] = {
	{"US", 17, {1,2,3,4,5,22,23,24,25,26,27,28,29,30,31,128,130}, 3, {12,32,33}, us_oper_class_table},
	{"EU", 11, {1,2,3,5,6,7,8,9,10,128,130}, 3, {4,11,12}, eu_oper_class_table},
	{"JP", 17, {1,32,33,34,35,36,37,38,39,40,41,42,43,44,45,128,130}, 4, {30,31,56,57}, jp_oper_class_table},
	{"GB", 14, {115,116,117,118,119,120,121,122,123,124,126,127,128,130}, 4, {81,82,83,84}, gb_oper_class_table},
};
#define OPER_CLASS_GB_INDEX (ARRAY_SIZE(oper_class_table) - 1)

static struct ieee80211_band_info ieee80211_bands[IEEE80211_BAND_IDX_MAX] = {
	/* {band_chan_step, band_first_chan, band_chan_cnt} */
	{IEEE80211_24G_CHAN_SEC_SHIFT,	1,  13},
	{IEEE80211_24G_CHAN_SEC_SHIFT,	14, 1},
	{IEEE80211_CHAN_SEC_SHIFT,	36, 4},
	{IEEE80211_CHAN_SEC_SHIFT,	52, 4},
	{IEEE80211_CHAN_SEC_SHIFT,	100, 12},
	{IEEE80211_CHAN_SEC_SHIFT,	149, 4},
	/* isolate chan 165 for IOT as per sniffer capture */
	{IEEE80211_CHAN_SEC_SHIFT,	165, 1},
};

struct ieee80211_band_info *ieee80211_get_band_info(int band_idx)
{
	if (band_idx >= IEEE80211_BAND_IDX_MAX)
		return NULL;

	return &ieee80211_bands[band_idx];
}

#if defined(QBMPS_ENABLE)
/*******************************************************************************/
/* ieee80211_sta_bmps_update: allocate, re-allocate or free BMPS NULL frame    */
/*                                                                             */
/* NOTE: this function should be called whenever a new assocation              */
/*       happens, because node id associated with the frame needs              */
/*       to be updated                                                         */
/*******************************************************************************/
int ieee80211_sta_bmps_update(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node *ni = vap->iv_bss;
	struct sk_buff *skb = NULL;

	if (!ni)
		return -1;

	ieee80211_ref_node(ni);

	if (ic->ic_flags_qtn & IEEE80211_QTN_BMPS) {
		/* set null frame */
		skb = ieee80211_get_nulldata(ni);
		if (!skb) {
			ieee80211_free_node(ni);
			return -1;
		}
		if (ic->ic_bmps_set_frame(ic, ni, skb)) {
			dev_kfree_skb(skb);
			ieee80211_free_node(ni);
			return -1;
		}
	} else {
		/* free null frame */
		ic->ic_bmps_release_frame(ic);
	}

	ieee80211_free_node(ni);

	return 0;
}
EXPORT_SYMBOL(ieee80211_sta_bmps_update);
#endif

int ieee80211_is_idle_state(struct ieee80211com *ic)
{
	int ret = 1;
	struct ieee80211vap *vap;
	int wds_link_active = 0;
	struct ieee80211_node *wds_ni;
	struct ieee80211vap *sta_vap = NULL;
	int nvaps = 0;
#if defined(QBMPS_ENABLE)
	struct qdrv_vap *qv;
#endif

	IEEE80211_LOCK_IRQ(ic);

	if (ic->ic_ocac.ocac_running) {
		ret = 0;
		goto quit;
	}

	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		nvaps ++;
		if (vap->iv_opmode == IEEE80211_M_STA)
			sta_vap = vap;
	}


	/* Checking non-sta mode for WDS link */
	if (!(sta_vap && (nvaps == 1)) && (ic->ic_sta_assoc > 0)) {
		if (ic->ic_wds_links == ic->ic_sta_assoc) {
			TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
				if (vap->iv_opmode == IEEE80211_M_WDS) {
					wds_ni = ieee80211_get_wds_peer_node_noref(vap);
					if (wds_ni) {
						if (IEEE80211_BA_IS_COMPLETE(wds_ni->ni_ba_rx[IEEE80211_WDS_LINK_MAINTAIN_BA_TID].state) ||
							IEEE80211_BA_IS_COMPLETE(wds_ni->ni_ba_tx[IEEE80211_WDS_LINK_MAINTAIN_BA_TID].state)) {
							wds_link_active = 1;
							break;
						}
					}
				}
			}

			if (!wds_link_active) {
				ret = 1;
				goto quit;
			}
		}

		ret = 0;
		goto quit;
	}

#if defined(QBMPS_ENABLE)
	/* here is the logic which decide should power-save or not */
	if (sta_vap) {
		ret = 0;
		if (nvaps > 1) {
			/* multiple VAPS, and one of them is STA */
			/* force power-saving off */
			goto quit;
		}
		if ((sta_vap->iv_state == IEEE80211_S_RUN) &&
		    (ic->ic_flags_qtn & IEEE80211_QTN_BMPS) &&
		    !(ic->ic_flags & IEEE80211_F_SCAN) &&
		    !(ic->ic_flags_qtn & IEEE80211_QTN_BGSCAN) &&
		    !(ic->ic_flags_qtn & IEEE80211_QTN_SAMP_CHAN) &&
		    !sta_vap->iv_swbmiss_bmps_warning) {
			/* for single STA VAP: mark as idle only if */
			/* 1. BMPS power-saving is enabled, and */
			/* 2. not in SCAN process, and */
			/* 3. not in SCS sample channel process, and */
			/* 4. no beacon missing warning */
			qv = container_of(sta_vap, struct qdrv_vap, iv);
			if (qv->qv_bmps_mode == BMPS_MODE_MANUAL) {
				/* manual mode */
				ret = 1;
				goto quit;
			} else if ((qv->qv_bmps_mode == BMPS_MODE_AUTO) &&
				   (!sta_vap->iv_bmps_tput_high)) {
				/* auto mode */
				/* and tput is low */
				ret = 1;
				goto quit;
			}
		}
	}
#else
	if (sta_vap) {
		ret = 0;
		goto quit;
	}
#endif

quit:
	IEEE80211_UNLOCK_IRQ(ic);

	return ret;
}
EXPORT_SYMBOL(ieee80211_is_idle_state);

int ieee80211_is_on_weather_channel(struct ieee80211com *ic, struct ieee80211_channel *chan)
{
	int is_weather_chan = chan->ic_flags & IEEE80211_CHAN_WEATHER;
	int cur_bw = ieee80211_get_bw(ic);

	if (chan == NULL || chan == IEEE80211_CHAN_ANYC)
		return 0;

	if (cur_bw >= BW_HT40) {
		is_weather_chan |= chan->ic_flags & IEEE80211_CHAN_WEATHER_40M;
		if (cur_bw >= BW_HT80)
			is_weather_chan |= chan->ic_flags & IEEE80211_CHAN_WEATHER_80M;
	}

	return !!is_weather_chan;
}
EXPORT_SYMBOL(ieee80211_is_on_weather_channel);

#if defined(QBMPS_ENABLE)
/************************************************************/
/* ieee80211_bmps_tput_check: calculate TX/RX tput          */
/*                                                          */
/* NOTE: this tput information will be used to decide       */
/*       entering/exiting power-saving state automatically  */
/*       while BMPS works in AUTO mode                      */
/************************************************************/
static void
ieee80211_bmps_tput_check(unsigned long arg)
{
	struct ieee80211com *ic = (struct ieee80211com *)arg;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	struct net_device_stats *stats;
	uint32_t rx_bytes_diff, tx_bytes_diff, curr_tput_kbps;

	if (vap) {
		stats = &vap->iv_devstats;
		if (vap->iv_ic->ic_get_shared_vap_stats) {
			/* get VAP TX/RX bytes stats info */
			vap->iv_ic->ic_get_shared_vap_stats(vap);
			/* calculate overall TX & RX tput */
			/* over the past measuring period */
			rx_bytes_diff = stats->rx_bytes -
						ic->ic_bmps_tput_check.prev_rx_bytes;
			tx_bytes_diff = stats->tx_bytes -
						ic->ic_bmps_tput_check.prev_tx_bytes;

			ic->ic_bmps_tput_check.prev_rx_bytes = stats->rx_bytes;
			ic->ic_bmps_tput_check.prev_tx_bytes = stats->tx_bytes;

			curr_tput_kbps = ((rx_bytes_diff + tx_bytes_diff) * 8) /
							(BMPS_TPUT_MEASURE_PERIOD_MS);
			if (curr_tput_kbps > BMPS_TPUT_THRESHOLD_UPPER) {
				/* tput is above upper threshold */
				/* it is time to exit BMPS power-saving */
				if (!vap->iv_bmps_tput_high ||
				    (vap->iv_bmps_tput_high == -1)) {
					vap->iv_bmps_tput_high = 1;
					ieee80211_pm_queue_work(ic);
				}
			} else if (curr_tput_kbps < BMPS_TPUT_THRESHOLD_LOWER){
				/* tput is below lower threshold */
				/* it is time to enter BMPS power-saving */
				if (vap->iv_bmps_tput_high ||
				    (vap->iv_bmps_tput_high == -1)) {
					vap->iv_bmps_tput_high = 0;
					ieee80211_pm_queue_work(ic);
				}
			}
		}
	}

	mod_timer(&ic->ic_bmps_tput_check.tput_timer,
			jiffies + (BMPS_TPUT_MEASURE_PERIOD_MS / 1000) * HZ);
}
#endif

static void
ieee80211_update_pm(struct work_struct *work)
{
	struct ieee80211com *ic = container_of(work, struct ieee80211com, pm_work.work);
	int level = ieee80211_is_idle_state(ic) ?
		BOARD_PM_LEVEL_IDLE : PM_QOS_DEFAULT_VALUE;
	pm_qos_update_requirement(PM_QOS_POWER_SAVE, BOARD_PM_GOVERNOR_WLAN, level);
}

static void
ieee80211_pm_period_change(unsigned long arg)
{
	struct ieee80211com *ic = (struct ieee80211com *)arg;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	static int cnt = 0;
	int value;
	int v = 0;
	int period_cnt = BOARD_PM_PERIOD_CNT;
	int period_change_interval = BOARD_PM_PERIOD_CHANGE_INTERVAL;

	if (vap) {
		if ((ic->ic_pm_state[QTN_PM_PERIOD_CNT] >= 1) &&
				(ic->ic_pm_state[QTN_PM_PERIOD_CNT] <= BOARD_PM_PERIOD_CNT)) {
			period_cnt = ic->ic_pm_state[QTN_PM_PERIOD_CNT];
		}

		v = (ic->ic_pm_state[QTN_PM_PERIOD_GROUP] >> (8 * (cnt % period_cnt))) & 0xFF;
		value = QTN_PM_PACK_PARAM_VALUE(QTN_PM_PDUTY_PERIOD_MS, v);
		ic->ic_setparam(vap->iv_bss, IEEE80211_PARAM_PWR_SAVE, value, NULL, 0);
	}

	if (ic->ic_pm_state[QTN_PM_PERIOD_CHANGE_INTERVAL] >= BOARD_PM_PERIOD_CHANGE_INTERVAL) {
		period_change_interval = ic->ic_pm_state[QTN_PM_PERIOD_CHANGE_INTERVAL];
	}

	mod_timer(&ic->ic_pm_period_change, jiffies + period_change_interval * HZ);
	cnt++;
}

void
ieee80211_pm_queue_work(struct ieee80211com *ic)
{
	unsigned long delay;
	int idle = ieee80211_is_idle_state(ic);

	if (idle) {
#if defined(QBMPS_ENABLE)
		if ((ic->ic_flags_qtn & IEEE80211_QTN_BMPS) &&
		    (ic->ic_opmode & IEEE80211_M_STA))
			delay = BOARD_PM_WLAN_STA_IDLE_TIMEOUT;
		else
#endif
			delay = BOARD_PM_WLAN_IDLE_TIMEOUT;
	} else
		delay = BOARD_PM_WLAN_DEFAULT_TIMEOUT;

	pm_queue_work(&ic->pm_work, delay);
}
EXPORT_SYMBOL(ieee80211_pm_queue_work);

static void
ieee80211_vap_remove_ie(struct ieee80211vap *vap)
{
	int i;

	for (i = 0; i < IEEE80211_APPIE_NUM_OF_FRAME; i++) {
		if (vap->app_ie[i].ie != NULL) {
			FREE(vap->app_ie[i].ie, M_DEVBUF);
			vap->app_ie[i].ie = NULL;
			vap->app_ie[i].length = 0;
		}
	}

	if (vap->iv_opt_ie != NULL) {
		FREE(vap->iv_opt_ie, M_DEVBUF);
		vap->iv_opt_ie = NULL;
		vap->iv_opt_ie_len = 0;
	}

	if (vap->qtn_pairing_ie.ie != NULL) {
		FREE(vap->qtn_pairing_ie.ie, M_DEVBUF);
		vap->qtn_pairing_ie.ie = NULL;
		vap->qtn_pairing_ie.length = 0;
	}

}

void init_wowlan_params(struct ieee80211_wowlan *wowlan)
{
	wowlan->host_state = 0;
	wowlan->wowlan_match = 0;
	wowlan->L2_ether_type = 0x0842;
	wowlan->L3_udp_port = 0xffff;
	wowlan->pattern.len = 0;
	memset(wowlan->pattern.magic_pattern, 0, 256);
}

static void
ieee80211_extender_start_scan(unsigned long arg)
{
	struct ieee80211com *ic = (struct ieee80211com *)arg;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);

	if (ic->ic_extender_role != IEEE80211_EXTENDER_ROLE_RBS)
		return;

	if (!vap || vap->iv_opmode != IEEE80211_M_HOSTAP) {
		mod_timer(&ic->ic_extender_scan_timer, jiffies +
				IEEE80211_EXTENDER_SCAN_MBS_INTERVAL * HZ);
		return;
	}

	if (time_after(jiffies, ic->ic_extender_mbs_detected_jiffies +
				IEEE80211_EXTENDER_MBS_INVALID_TIMEOUT * HZ)) {
		(void) ieee80211_start_scan(vap,
			IEEE80211_SCAN_ACTIVE |
			IEEE80211_SCAN_ONCE |
			IEEE80211_SCAN_QTN_SEARCH_MBS |
			IEEE80211_SCAN_NOPICK,
			IEEE80211_SCAN_FOREVER,
			0, NULL);
	}

	mod_timer(&ic->ic_extender_scan_timer, jiffies +
			IEEE80211_EXTENDER_SCAN_MBS_INTERVAL * HZ);
}

int
ieee80211_ifattach(struct ieee80211com *ic)
{
	struct ieee80211_channel *c;
	struct ifmediareq imr;
	int i;

	_MOD_INC_USE(THIS_MODULE, return -ENODEV);

	/*
	 * Pick an initial operating mode until we have a vap
	 * created to lock it down correctly.  This is only
	 * drivers have something defined for configuring the
	 * hardware at startup.
	 */
	ic->ic_opmode = IEEE80211_M_STA;	/* everyone supports this */

	/*
	 * Fill in 802.11 available channel set, mark
	 * all available channels as active, and pick
	 * a default channel if not already specified.
	 */
	KASSERT(0 < ic->ic_nchans && ic->ic_nchans < IEEE80211_CHAN_MAX,
		("invalid number of channels specified: %u", ic->ic_nchans));
	memset(ic->ic_chan_avail, 0, sizeof(ic->ic_chan_avail));
	ic->ic_modecaps |= 1<<IEEE80211_MODE_AUTO;
	for (i = 0; i < ic->ic_nchans; i++) {
		c = &ic->ic_channels[i];
		KASSERT(c->ic_flags != 0, ("channel with no flags"));
		KASSERT(c->ic_ieee < IEEE80211_CHAN_MAX,
			("channel with bogus ieee number %u", c->ic_ieee));
		/* make sure only valid 2.4G or 5G channels are set as available */
                if (((c->ic_ieee >= QTN_2G_FIRST_OPERATING_CHAN) && (c->ic_ieee <= QTN_2G_LAST_OPERATING_CHAN)) ||
                    ((c->ic_ieee >= QTN_5G_FIRST_OPERATING_CHAN) && (c->ic_ieee <= QTN_5G_LAST_OPERATING_CHAN))) {
                        setbit(ic->ic_chan_avail, c->ic_ieee);
                }

		/*
		 * Identify mode capabilities.
		 */
		if (IEEE80211_IS_CHAN_A(c))
			ic->ic_modecaps |= 1<<IEEE80211_MODE_11A;
		if (IEEE80211_IS_CHAN_B(c))
			ic->ic_modecaps |= 1<<IEEE80211_MODE_11B;
		if (IEEE80211_IS_CHAN_PUREG(c))
			ic->ic_modecaps |= 1<<IEEE80211_MODE_11G;
		if (IEEE80211_IS_CHAN_FHSS(c))
			ic->ic_modecaps |= 1<<IEEE80211_MODE_FH;
		if (IEEE80211_IS_CHAN_108A(c))
			ic->ic_modecaps |= 1<<IEEE80211_MODE_TURBO_A;
		if (IEEE80211_IS_CHAN_108G(c))
			ic->ic_modecaps |= 1<<IEEE80211_MODE_TURBO_G;
		if (IEEE80211_IS_CHAN_11NG(c))
			ic->ic_modecaps |= 1<<IEEE80211_MODE_11NG;
		if (IEEE80211_IS_CHAN_11NA(c))
			ic->ic_modecaps |= 1<<IEEE80211_MODE_11NA;
		if (IEEE80211_IS_CHAN_11NG_HT40PLUS(c))
			ic->ic_modecaps |= 1<<IEEE80211_MODE_11NG_HT40PM;
		if (IEEE80211_IS_CHAN_11NG_HT40MINUS(c))
			ic->ic_modecaps |= 1<<IEEE80211_MODE_11NG_HT40PM;
		if (IEEE80211_IS_CHAN_11NA_HT40PLUS(c))
			ic->ic_modecaps |= 1<<IEEE80211_MODE_11NA_HT40PM;
		if (IEEE80211_IS_CHAN_11NA_HT40MINUS(c))
			ic->ic_modecaps |= 1<<IEEE80211_MODE_11NA_HT40PM;
		if (IEEE80211_IS_CHAN_11AC(c))
			ic->ic_modecaps |= 1<<IEEE80211_MODE_11AC_VHT20PM;
		if (IEEE80211_IS_CHAN_11AC_VHT40PLUS(c))
			ic->ic_modecaps |= 1<<IEEE80211_MODE_11AC_VHT40PM;
		if (IEEE80211_IS_CHAN_11AC_VHT40MINUS(c))
			ic->ic_modecaps |= 1<<IEEE80211_MODE_11AC_VHT40PM;
		if (IEEE80211_IS_CHAN_11AC_VHT80_EDGEPLUS(c))
			ic->ic_modecaps |= 1<<IEEE80211_MODE_11AC_VHT80PM;
		if (IEEE80211_IS_CHAN_11AC_VHT80_CNTRPLUS(c))
			ic->ic_modecaps |= 1<<IEEE80211_MODE_11AC_VHT80PM;
		if (IEEE80211_IS_CHAN_11AC_VHT80_CNTRMINUS(c))
			ic->ic_modecaps |= 1<<IEEE80211_MODE_11AC_VHT80PM;
		if (IEEE80211_IS_CHAN_11AC_VHT80_EDGEMINUS(c))
			ic->ic_modecaps |= 1<<IEEE80211_MODE_11AC_VHT80PM;
	}
	/* initialize candidate channels to all available */
	memcpy(ic->ic_chan_active, ic->ic_chan_avail,
		sizeof(ic->ic_chan_avail));

	memset(ic->ic_chan_availability_status, IEEE80211_CHANNEL_STATUS_AVAILABLE, sizeof(ic->ic_chan_availability_status));
	/* validate ic->ic_curmode */
	if ((ic->ic_modecaps & (1<<ic->ic_curmode)) == 0)
		ic->ic_curmode = IEEE80211_MODE_AUTO;
	/*
	 * When 11g is supported, force the rate set to
	 * include basic rates suitable for a mixed b/g bss.
	 */
	if (ic->ic_modecaps & (1<<IEEE80211_MODE_11G))
		ieee80211_set11gbasicrates(
			&ic->ic_sup_rates[IEEE80211_MODE_11G],
			IEEE80211_MODE_11G);

	/* 11n also checks for 11g basic rates for capabilities */
	if (ic->ic_modecaps & (1<<IEEE80211_MODE_11NG))
		ieee80211_set11gbasicrates(
			&ic->ic_sup_rates[IEEE80211_MODE_11NG],
			IEEE80211_MODE_11G);

	/* setup initial channel settings */
	ic->ic_bsschan = IEEE80211_CHAN_ANYC;
	ic->ic_des_chan = IEEE80211_CHAN_ANYC;

	/* arbitrarily pick the first channel */
	ic->ic_curchan = &ic->ic_channels[1];
	ic->ic_prevchan = ic->ic_curchan;

	/* Enable marking of dfs by default */
	ic->ic_flags_ext |= IEEE80211_FEXT_MARKDFS;

	/* Phytype OFDM. FIXME: this may change with RFIC5 */
	ic->ic_phytype = IEEE80211_T_OFDM;

	/* Enable LDPC by default */
	ic->ldpc_enabled = 1;

	ic->ic_gi_select_enable = QTN_GLOBAL_INIT_SELECT_GI_ENABLE;
	ic->ic_pppc_select_enable = QTN_GLOBAL_INIT_SELECT_PPPC_ENABLE;

	ic->ic_def_matrix = QTN_GLOBAL_INIT_DEF_MATRIX;

	/*
	 * Enable WME by default if we're capable.
	 */
	if (ic->ic_caps & IEEE80211_C_WME)
		ic->ic_flags |= IEEE80211_F_WME;
	(void) ieee80211_setmode(ic, ic->ic_curmode);

	if (ic->ic_lintval == 0) {
		ic->ic_lintval = IEEE80211_BINTVAL_DEFAULT;
		ic->ic_lintval_backup = IEEE80211_BINTVAL_DEFAULT;
	}
	ic->ic_bmisstimeout = 7 * ic->ic_lintval;	/* default 7 beacons */
	IEEE80211_LOCK_INIT(ic, "ieee80211com");
	IEEE80211_VAPS_LOCK_INIT(ic, "ieee80211com_vaps");
	TAILQ_INIT(&ic->ic_vaps);

	ic->ic_txpowlimit = IEEE80211_TXPOWER_MAX;
	ic->ic_txpowlimit = IEEE80211_TXPOWER_MIN;
	ic->ic_newtxpowlimit = IEEE80211_TXPOWER_MAX;

	ic->ic_extender_role = IEEE80211_EXTENDER_ROLE_NONE;
	ic->ic_extender_mbs_best_rssi = IEEE80211_EXTENDER_DEFAULT_MBS_BEST_RSSI;
	ic->ic_extender_rbs_best_rssi = IEEE80211_EXTENDER_DEFAULT_RBS_BEST_RSSI;
	ic->ic_extender_mbs_wgt = IEEE80211_EXTENDER_DEFAULT_MBS_WGT;
	ic->ic_extender_rbs_wgt = IEEE80211_EXTENDER_DEFAULT_RBS_WGT;
	init_timer(&ic->ic_extender_scan_timer);
	ic->ic_extender_scan_timer.function = ieee80211_extender_start_scan;
	ic->ic_extender_scan_timer.data = (unsigned long)ic;
	ic->ic_extender_mbs_detected_jiffies = jiffies;
	ic->ic_extender_rssi_continue = 0;
	ic->ic_scan_opchan_enable = 0;
	ic->ic_extender_bgscanintvl = IEEE80211_BGSCAN_INTVAL_DEFAULT * HZ;
	ic->ic_extender_mbs_rssi_margin = IEEE80211_EXTENDER_DEFAULT_MBS_RSSI_MARGIN;
	ic->ic_scan_tbl_len_max = IEEE80211_SCAN_TBL_LEN_MAX_DFLT;
	ic->ic_max_system_bw = BW_HT80;
	ic->ic_oper_class_table = &oper_class_table[OPER_CLASS_GB_INDEX];

	ieee80211_crypto_attach(ic);
	ieee80211_node_attach(ic);
	ieee80211_power_attach(ic);
	ieee80211_proto_attach(ic);
	ieee80211_scan_attach(ic);
	ieee80211_tpc_query_init(&ic->ic_tpc_query_info, ic, TPC_INTERVAL_DEFAULT);
	ieee80211_doth_measurement_init(ic);

	ieee80211_media_setup(ic, &ic->ic_media, ic->ic_caps,
		ieee80211com_media_change, ieee80211com_media_status);
	ieee80211com_media_status((void *) ic, &imr);
	ifmedia_set(&ic->ic_media, imr.ifm_active);

	INIT_DELAYED_WORK(&ic->pm_work, ieee80211_update_pm);
	pm_qos_add_requirement(PM_QOS_POWER_SAVE, BOARD_PM_GOVERNOR_WLAN, PM_QOS_DEFAULT_VALUE);
	init_timer(&ic->ic_pm_period_change);
	ic->ic_pm_period_change.function = ieee80211_pm_period_change;
	ic->ic_pm_period_change.data = (unsigned long) ic;

#if defined(QBMPS_ENABLE)
	init_timer(&ic->ic_bmps_tput_check.tput_timer);
	ic->ic_bmps_tput_check.tput_timer.function = ieee80211_bmps_tput_check;
	ic->ic_bmps_tput_check.tput_timer.data = (unsigned long) ic;
#endif

	ic->ic_offchan_protect.offchan_stop_expire.function = ieee80211_off_channel_timeout;
	init_timer(&ic->ic_offchan_protect.offchan_stop_expire);

	init_waitqueue_head(&ic->ic_scan_comp);

	init_wowlan_params(&ic->ic_wowlan);

	ic->ic_vap_default_state = IEEE80211_VAP_STATE_ENABLED;

	ic->ic_max_boot_cac_duration = -1;

	ic->ic_boot_cac_end_jiffy = 0;
	ic->ic_rx_bar_sync = QTN_RX_BAR_SYNC_QTN;

	return 0;
}
EXPORT_SYMBOL(ieee80211_ifattach);

void
ieee80211_ifdetach(struct ieee80211com *ic)
{
	struct ieee80211vap *vap;

#if defined(QBMPS_ENABLE)
	del_timer(&ic->ic_bmps_tput_check.tput_timer);
#endif
	del_timer_sync(&ic->ic_offchan_protect.offchan_stop_expire);

	pm_flush_work(&ic->pm_work);
	pm_qos_remove_requirement(PM_QOS_POWER_SAVE, BOARD_PM_GOVERNOR_WLAN);

	rtnl_lock();
	while ((vap = TAILQ_FIRST(&ic->ic_vaps)) != NULL)
		ic->ic_vap_delete(vap);
	rtnl_unlock();

	ieee80211_scs_free_tdls_stats_list(ic);
	ieee80211_doth_measurement_deinit(ic);
	ieee80211_tpc_query_deinit(&ic->ic_tpc_query_info);
	ieee80211_scan_detach(ic);
	ieee80211_proto_detach(ic);
	ieee80211_crypto_detach(ic);
	ieee80211_power_detach(ic);
	ieee80211_node_detach(ic);
	ifmedia_removeall(&ic->ic_media);

	IEEE80211_VAPS_LOCK_DESTROY(ic);
	IEEE80211_LOCK_DESTROY(ic);

	_MOD_DEC_USE(THIS_MODULE);
}
EXPORT_SYMBOL(ieee80211_ifdetach);

static void ieee80211_vap_init_tdls(struct ieee80211vap *vap)
{
	if (vap->iv_opmode == IEEE80211_M_STA) {
		/* AP support tdls and STA disable tdls by default */
		vap->iv_flags_ext |= IEEE80211_FEXT_TDLS_PROHIB;
		vap->iv_flags_ext &= ~IEEE80211_FEXT_TDLS_CS_PROHIB;
		vap->iv_flags_ext &= ~IEEE80211_FEXT_AP_TDLS_PROHIB;
		vap->tdls_discovery_interval = DEFAULT_TDLS_DISCOVER_INTERVAL;
		vap->tdls_node_life_cycle = DEFAULT_TDLS_LIFE_CYCLE;
		vap->tdls_path_sel_prohibited = DEFAULT_TDLS_PATH_SEL_MODE;
		vap->tdls_timeout_time = DEFAULT_TDLS_TIMEOUT_TIME;
		vap->tdls_path_sel_weight = DEFAULT_TDLS_LINK_WEIGHT;
		vap->tdls_training_pkt_cnt = DEFAULT_TDLS_RATE_DETECTION_PKT_CNT;
		vap->tdls_uapsd_indicat_wnd = DEFAULT_TDLS_UAPSD_INDICATION_WND;
		vap->tdls_path_sel_pps_thrshld = DEFAULT_TDLS_PATH_SEL_PPS_THRSHLD;
		vap->tdls_path_sel_rate_thrshld = DEFAULT_TDLS_PATH_SEL_RATE_THRSHLD;
		vap->tdls_verbose = DEFAULT_TDLS_VERBOSE;
		vap->tdls_min_valid_rssi = DEFAULT_TDLS_MIN_RSSI;
		vap->tdls_switch_ints = DEFAULT_TDLS_LINK_SWITCH_INV;
		vap->tdls_phy_rate_wgt = DEFAULT_TDLS_PHY_RATE_WEIGHT;
		vap->tdls_fixed_off_chan = DEFAULT_TDLS_FIXED_OFF_CHAN;
		vap->tdls_fixed_off_chan_bw = BW_INVALID;
		vap->tdls_chan_switching = 0;
		vap->tdls_cs_disassoc_pending = 0;
		vap->tdls_cs_node = NULL;
		spin_lock_init(&vap->tdls_ps_lock);
	}
}

int
ieee80211_vap_setup(struct ieee80211com *ic, struct net_device *dev,
	const char *name, int unit, int opmode, int flags)
{
#define	IEEE80211_C_OPMODE \
	(IEEE80211_C_IBSS | IEEE80211_C_HOSTAP | IEEE80211_C_AHDEMO | \
	 IEEE80211_C_MONITOR)
	struct ieee80211vap *vap = netdev_priv(dev);
	struct net_device_ops *pndo = (struct net_device_ops *)dev->netdev_ops;
	int err;

	if (name != NULL) {
		if (strchr(name, '%')) {
			if ((err = dev_alloc_name(dev, name)) < 0) {
				printk(KERN_ERR "can't alloc name %s\n", name);
				return err;
			}
		} else {
			strncpy(dev->name, name, sizeof(dev->name));
		}
	}
	pndo->ndo_get_stats = ieee80211_getstats;
	pndo->ndo_open = ieee80211_open;
	pndo->ndo_stop = ieee80211_stop;
	pndo->ndo_set_multicast_list = ieee80211_set_multicast_list;
	pndo->ndo_change_mtu = ieee80211_change_mtu;
	dev->tx_queue_len = QTN_BUFS_WMAC_TX_QDISC;

	/*
	 * The caller is assumed to allocate the device with
	 * alloc_etherdev or similar so we arrange for the
	 * space to be reclaimed accordingly.
	 */
	dev->destructor = free_netdev;

	vap->iv_ic = ic;
	vap->iv_dev = dev;			/* back pointer */
	vap->iv_unit = unit;
	vap->iv_flags = ic->ic_flags;		/* propagate common flags */
	vap->iv_flags_ext = ic->ic_flags_ext;
	vap->iv_xrvap = NULL;
	vap->iv_ath_cap = ic->ic_ath_cap;
	/* Default Multicast traffic to lowest rate of 1000 Kbps*/
	vap->iv_mcast_rate = 1000;

	vap->iv_caps = ic->ic_caps &~ IEEE80211_C_OPMODE;

	/* Enabling short GI by default. This may be right place to set it */
	vap->iv_ht_flags |= IEEE80211_HTF_SHORTGI_ENABLED;
	vap->iv_ht_flags |= IEEE80211_HTF_LDPC_ENABLED;

	/* Initialize vht capability flags  */
	vap->iv_vht_flags = ic->ic_vhtcap.cap_flags;

	/* Disable STBC by default  */
	vap->iv_ht_flags &= ~(IEEE80211_HTCAP_C_TXSTBC | IEEE80211_HTCAP_C_RXSTBC);
	vap->iv_vht_flags &= ~(IEEE80211_VHTCAP_C_TX_STBC);

	vap->iv_rx_amsdu_enable = QTN_RX_AMSDU_DYNAMIC;
	vap->iv_rx_amsdu_threshold_cca = IEEE80211_RX_AMSDU_THRESHOLD_CCA;
	vap->iv_rx_amsdu_threshold_pmbl = IEEE80211_RX_AMSDU_THRESHOLD_PMBL;
	vap->iv_rx_amsdu_pmbl_wf_sp = IEEE80211_RX_AMSDU_PMBL_WF_SP;
	vap->iv_rx_amsdu_pmbl_wf_lp = IEEE80211_RX_AMSDU_PMBL_WF_LP;

	switch (opmode) {
	case IEEE80211_M_STA:
		/* WDS/Repeater */
		if (flags & IEEE80211_NO_STABEACONS)
		{
			vap->iv_flags_ext |= IEEE80211_FEXT_SWBMISS;
			vap->iv_link_loss_enabled = 1;
			vap->iv_bcn_miss_thr = 0;
		}
		vap->iv_caps |= IEEE80211_C_WDS;
		vap->iv_flags_ext |= IEEE80211_FEXT_WDS;
		/* do DBS specific initialization, keep it open for all chipset,
		 * as we don't want chip specific execution here, for RF with no
		 * dual band support, these initiazation won't be referenced.
		 */
		vap->iv_pref_band = IEEE80211_5Ghz;
		/* 2,4ghz specific station profile */
		vap->iv_2_4ghz_prof.phy_mode = IEEE80211_MODE_11NG_HT40PM;
		vap->iv_2_4ghz_prof.vht = 0;
		vap->iv_2_4ghz_prof.bw = 40;

		/* 5ghz specific station profile */
		vap->iv_5ghz_prof.phy_mode = IEEE80211_MODE_11AC_VHT80PM;
		vap->iv_5ghz_prof.vht = 1;
		vap->iv_5ghz_prof.bw = 80;
		break;
	case IEEE80211_M_IBSS:
		vap->iv_caps |= IEEE80211_C_IBSS;
		vap->iv_ath_cap &= ~IEEE80211_ATHC_XR;
		break;
	case IEEE80211_M_AHDEMO:
		vap->iv_caps |= IEEE80211_C_AHDEMO;
		vap->iv_ath_cap &= ~IEEE80211_ATHC_XR;
		break;
	case IEEE80211_M_HOSTAP:
		vap->iv_caps |= IEEE80211_C_HOSTAP;
		vap->iv_ath_cap &= ~IEEE80211_ATHC_TURBOP;
		if ((vap->iv_flags & IEEE80211_VAP_XR) == 0)
			vap->iv_ath_cap &= ~IEEE80211_ATHC_XR;
		vap->iv_caps |= IEEE80211_C_WDS;
		vap->iv_flags_ext |= IEEE80211_FEXT_WDS;
		vap->iv_flags |= IEEE80211_F_DROPUNENC;
		break;
	case IEEE80211_M_MONITOR:
		vap->iv_caps |= IEEE80211_C_MONITOR;
		vap->iv_ath_cap &= ~(IEEE80211_ATHC_XR | IEEE80211_ATHC_TURBOP);
		break;
	case IEEE80211_M_WDS:
		vap->iv_caps |= IEEE80211_C_WDS;
		vap->iv_ath_cap &= ~(IEEE80211_ATHC_XR | IEEE80211_ATHC_TURBOP);
		vap->iv_flags_ext |= IEEE80211_FEXT_WDS;
		/* Set WDS according to Extender Role */
		ieee80211_vap_wds_mode_change(vap);
		break;
	}
	vap->iv_opmode = opmode;
	IEEE80211_INIT_TQUEUE(&vap->iv_stajoin1tq, ieee80211_sta_join1_tasklet, vap);

	vap->iv_chanchange_count = 0;

	/*
	 * Enable various functionality by default if we're capable.
	 */
	if (vap->iv_caps & IEEE80211_C_WME)
		vap->iv_flags |= IEEE80211_F_WME;
	if (vap->iv_caps & IEEE80211_C_FF)
		vap->iv_flags |= IEEE80211_F_FF;

	vap->iv_dtim_period = IEEE80211_DTIM_DEFAULT;

	vap->iv_monitor_crc_errors = 0;
	vap->iv_monitor_phy_errors = 0;

	/* Defaults for implicit BA and global BA mask */
	vap->iv_ba_control = 0xFFFF;
	vap->iv_implicit_ba = 0x1;
	vap->iv_max_ba_win_size = IEEE80211_DEFAULT_BA_WINSIZE;

	vap->iv_mcs_config = IEEE80211_MCS_AUTO_RATE_ENABLE;

	/* initialize TDLS Function */
	ieee80211_vap_init_tdls(vap);

	/* Only need the peer entry in AID table for WDS mode VAP */
	if (opmode == IEEE80211_M_WDS)
		vap->iv_max_aid = 1;

	IEEE80211_ADDR_COPY(vap->iv_myaddr, dev->dev_addr);
	/* NB: defer setting dev_addr so driver can override */

	vap->iv_blacklist_timeout = msecs_to_jiffies(IEEE80211_BLACKLIST_TIMEOUT * MSEC_PER_SEC);
#define IEEE80211_RATE_TRAINING_COUNT_DEFAULT		300
#define IEEE80211_RATE_TRAINING_BURST_COUNT_DEFAULT	32
	vap->iv_rate_training_count = IEEE80211_RATE_TRAINING_COUNT_DEFAULT;
	vap->iv_rate_training_burst_count = IEEE80211_RATE_TRAINING_BURST_COUNT_DEFAULT;
	vap->iv_mc_to_uc = IEEE80211_QTN_MC_TO_UC_LEGACY;
	vap->iv_reliable_bcst = 1;
	vap->iv_ap_fwd_lncb = 1;
	vap->iv_tx_amsdu = 1;
	vap->iv_tx_max_amsdu = IEEE80211_VHTCAP_MAX_MPDU_11454;
	vap->allow_tkip_for_vht = 0;
	vap->is_block_all_assoc = 0;
	vap->iv_vap_state = IEEE80211_VAP_STATE_ENABLED;

	ieee80211_crypto_vattach(vap);
	ieee80211_node_vattach(vap);
	ieee80211_power_vattach(vap);
	ieee80211_proto_vattach(vap);
	ieee80211_scan_vattach(vap);
	ieee80211_vlan_vattach(vap);
	ieee80211_ioctl_vattach(vap);
	ieee80211_sysctl_vattach(vap);

	return 1;
#undef IEEE80211_C_OPMODE
}
EXPORT_SYMBOL(ieee80211_vap_setup);

int
ieee80211_vap_attach(struct ieee80211vap *vap,
	ifm_change_cb_t media_change, ifm_stat_cb_t media_status)
{
	struct net_device *dev = vap->iv_dev;
	struct ieee80211com *ic = vap->iv_ic;
	struct ifmediareq imr;

	ieee80211_node_latevattach(vap);	/* XXX move into vattach */
	ieee80211_power_latevattach(vap);	/* XXX move into vattach */

	memset(vap->wds_mac, 0x00, IEEE80211_ADDR_LEN);

	(void) ieee80211_media_setup(ic, &vap->iv_media,
		vap->iv_caps, media_change, media_status);
	ieee80211_media_status((void *) vap, &imr);
	ifmedia_set(&vap->iv_media, imr.ifm_active);

	IEEE80211_LOCK_IRQ(ic);
	TAILQ_INSERT_TAIL(&ic->ic_vaps, vap, iv_next);
	IEEE80211_UNLOCK_IRQ(ic);

	IEEE80211_ADDR_COPY(dev->dev_addr, vap->iv_myaddr);

	ieee80211_scanner_get(vap->iv_opmode, 1);

	ieee80211_pm_queue_work(ic);

	ieee80211_wme_initparams(vap);

	/* Fix issue that tx power will be abnormal when dynamically switch from station mode to AP mode*/
	ieee80211_pwr_adjust(vap, 0);

	ieee80211_tdls_vattach(vap);

	INIT_LIST_HEAD(&vap->sample_sta_list);
	spin_lock_init(&vap->sample_sta_lock);
	vap->sample_sta_count = 0;

	/* NB: rtnl is held on entry so don't use register_netdev */
	if (register_netdevice(dev)) {
		printk(KERN_ERR "%s: unable to register device\n", dev->name);
		return 0;
	} else {
		return 1;
	}
}
EXPORT_SYMBOL(ieee80211_vap_attach);

void
ieee80211_vap_detach(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;

	IEEE80211_CANCEL_TQUEUE(&vap->iv_stajoin1tq);
	IEEE80211_LOCK_IRQ(ic);
	TAILQ_REMOVE(&ic->ic_vaps, vap, iv_next);
	if (TAILQ_EMPTY(&ic->ic_vaps))		/* reset to supported mode */
		ic->ic_opmode = IEEE80211_M_STA;
	IEEE80211_UNLOCK_IRQ(ic);

	/*
	 * Change state to 'INIT' to disassociate WDS peer node
	 */
	if (vap->iv_opmode == IEEE80211_M_WDS)
		ieee80211_new_state(vap, IEEE80211_S_INIT, 0);

	ifmedia_removeall(&vap->iv_media);

	sample_rel_client_data(vap);
	ieee80211_sysctl_vdetach(vap);
	ieee80211_proc_cleanup(vap);
	ieee80211_ioctl_vdetach(vap);
	ieee80211_vlan_vdetach(vap);
	ieee80211_scan_vdetach(vap);
	ieee80211_proto_vdetach(vap);
	ieee80211_crypto_vdetach(vap);
	ieee80211_power_vdetach(vap);
	ieee80211_tdls_vdetach(vap);
	ieee80211_node_vdetach(vap);
	ieee80211_vap_remove_ie(vap);
	ieee80211_extender_vdetach(vap);

	ieee80211_pm_queue_work(ic);

}
EXPORT_SYMBOL(ieee80211_vap_detach);

void
ieee80211_vap_detach_late(struct ieee80211vap *vap)
{
	/* NB: rtnl is held on entry so don't use unregister_netdev */
	unregister_netdevice(vap->iv_dev);
}
EXPORT_SYMBOL(ieee80211_vap_detach_late);

/*
 * Convert MHz frequency to IEEE channel number.
 */
u_int
ieee80211_mhz2ieee(u_int freq, u_int flags)
{
	if (flags & IEEE80211_CHAN_2GHZ) {	/* 2GHz band */
		if (freq == 2484)		/* Japan */
			return 14;
		if ((freq >= 2412) && (freq < 2484)) /* don't number non-IEEE channels */
			return (freq - 2407) / 5;
		return 0;
	} else if (flags & IEEE80211_CHAN_5GHZ)	{	/* 5Ghz band */
		if ((freq >= 5150) && (freq <= 5845))	/* don't number non-IEEE channels */
			return (freq - 5000) / 5;
		return 0;
	} else {
		/* something is fishy, don't do anything */
		return 0;
	}
}
EXPORT_SYMBOL(ieee80211_mhz2ieee);

/*
 * Convert channel to IEEE channel number.
 */
u_int
ieee80211_chan2ieee(struct ieee80211com *ic, const struct ieee80211_channel *c)
{
	if (c == NULL) {
		printk("invalid channel (NULL)\n");
		return 0;
	}
	return (c == IEEE80211_CHAN_ANYC ?  IEEE80211_CHAN_ANY : c->ic_ieee);
}
EXPORT_SYMBOL(ieee80211_chan2ieee);

/*
 * Convert IEEE channel number to MHz frequency.
 */
u_int
ieee80211_ieee2mhz(u_int chan, u_int flags)
{
	if (flags & IEEE80211_CHAN_2GHZ) {	/* 2GHz band */
		if (chan == 14)
			return 2484;
		if (chan < 14)
			return 2407 + chan * 5;
		else
			return 2512 + ((chan - 15) * 20);
	} else if (flags & IEEE80211_CHAN_5GHZ) {	/* 5Ghz band */
		return 5000 + (chan * 5);
	} else {					/* either, guess */
		if (chan == 14)
			return 2484;
		if (chan < 14)			/* 0-13 */
			return 2407 + chan * 5;
		if (chan < 27)			/* 15-26 */
			return 2512 + ((chan - 15) * 20);
		return 5000 + (chan * 5);
	}
}
EXPORT_SYMBOL(ieee80211_ieee2mhz);

/*
 * Locate a channel given a frequency+flags.  We cache
 * the previous lookup to optimize swithing between two
 * channels--as happens with dynamic turbo.
 */
struct ieee80211_channel *
ieee80211_find_channel(struct ieee80211com *ic, int freq, int flags)
{
	struct ieee80211_channel *c;
	int i;

	flags &= IEEE80211_CHAN_ALLTURBO;
	c = ic->ic_prevchan;
	if (c != NULL && c->ic_freq == freq &&
	    (c->ic_flags & IEEE80211_CHAN_ALLTURBO) == flags)
		return c;
	/* brute force search */
	for (i = 0; i < ic->ic_nchans; i++) {
		c = &ic->ic_channels[i];
		if (c->ic_freq == freq) /* &&
		    (c->ic_flags & IEEE80211_CHAN_ALLTURBO) == flags) */
			return c;
	}
	return NULL;
}
EXPORT_SYMBOL(ieee80211_find_channel);

/*
 * Setup the media data structures according to the channel and
 * rate tables.  This must be called by the driver after
 * ieee80211_attach and before most anything else.
 */
int
ieee80211_media_setup(struct ieee80211com *ic,
	struct ifmedia *media, u_int32_t caps,
	ifm_change_cb_t media_change, ifm_stat_cb_t media_stat)
{
#define	ADD(_media, _s, _o) \
	ifmedia_add(_media, IFM_MAKEWORD(IFM_IEEE80211, (_s), (_o), 0), 0, NULL)
	int i, j, mode, rate, maxrate, mword, mopt, r;
	struct ieee80211_rateset *rs;
	struct ieee80211_rateset allrates;

	/*
	 * Fill in media characteristics.
	 */
	ifmedia_init(media, 0, media_change, media_stat);
	maxrate = 0;
	memset(&allrates, 0, sizeof(allrates));
	for (mode = IEEE80211_MODE_AUTO; mode < IEEE80211_MODE_MAX; mode++) {
		static const u_int mopts[] = { 
			IFM_AUTO,
			IFM_IEEE80211_11A,
			IFM_IEEE80211_11B,
			IFM_IEEE80211_11G,
			IFM_IEEE80211_FH,
			IFM_IEEE80211_11A | IFM_IEEE80211_TURBO,
			IFM_IEEE80211_11G | IFM_IEEE80211_TURBO,
			IFM_IEEE80211_11NA,
			IFM_IEEE80211_11NG,
			IFM_IEEE80211_11NG_HT40PM,
			IFM_IEEE80211_11NA_HT40PM,
			IFM_IEEE80211_11AC_VHT20PM,
			IFM_IEEE80211_11AC_VHT40PM,
			IFM_IEEE80211_11AC_VHT80PM,
			IFM_IEEE80211_11AC_VHT160PM,
		};
		if ((ic->ic_modecaps & (1<<mode)) == 0)
			continue;
		mopt = mopts[mode];
		ADD(media, IFM_AUTO, mopt);	/* e.g. 11a auto */
		if (caps & IEEE80211_C_IBSS)
			ADD(media, IFM_AUTO, mopt | IFM_IEEE80211_ADHOC);
		if (caps & IEEE80211_C_HOSTAP)
			ADD(media, IFM_AUTO, mopt | IFM_IEEE80211_HOSTAP);
		if (caps & IEEE80211_C_AHDEMO)
			ADD(media, IFM_AUTO, mopt | IFM_IEEE80211_ADHOC | IFM_FLAG0);
		if (caps & IEEE80211_C_MONITOR)
			ADD(media, IFM_AUTO, mopt | IFM_IEEE80211_MONITOR);
		if (caps & IEEE80211_C_WDS)
			ADD(media, IFM_AUTO, mopt | IFM_IEEE80211_WDS);
		if (mode == IEEE80211_MODE_AUTO)
			continue;
		rs = &ic->ic_sup_rates[mode];
		for (i = 0; i < rs->rs_nrates; i++) {
			rate = rs->rs_rates[i];
			if(mode < IEEE80211_MODE_11NA)
				mword = ieee80211_rate2media(ic, rate, mode);
			else
			{
				/* This may contain both legacy and 11n rates */
				if(i < IEEE80211_RATE_SIZE) // 8 Legacy rates
					rate = rate & IEEE80211_RATE_VAL;

				mword = ieee80211_mcs2media(ic, rate, mode);
			}

			if (mword == 0)
				continue;
			ADD(media, mword, mopt);
			if (caps & IEEE80211_C_IBSS)
				ADD(media, mword, mopt | IFM_IEEE80211_ADHOC);
			if (caps & IEEE80211_C_HOSTAP)
				ADD(media, mword, mopt | IFM_IEEE80211_HOSTAP);
			if (caps & IEEE80211_C_AHDEMO)
				ADD(media, mword, mopt | IFM_IEEE80211_ADHOC | IFM_FLAG0);
			if (caps & IEEE80211_C_MONITOR)
				ADD(media, mword, mopt | IFM_IEEE80211_MONITOR);
			if (caps & IEEE80211_C_WDS)
				ADD(media, mword, mopt | IFM_IEEE80211_WDS);
			/*
			 * Add rate to the collection of all rates.
			 */
			r = rate & IEEE80211_RATE_VAL;
			for (j = 0; j < allrates.rs_nrates; j++)
				if (allrates.rs_rates[j] == r)
					break;
			if (j == allrates.rs_nrates) {
				/* unique, add to the set */
				allrates.rs_rates[j] = r;
				allrates.rs_nrates++;
			}
			rate = (rate & IEEE80211_RATE_VAL) / 2;
			if (rate > maxrate)
				maxrate = rate;
		}
	}
	for (i = 0; i < allrates.rs_nrates; i++) {
		if(mode < IEEE80211_MODE_11NA)
			mword = ieee80211_rate2media(ic, allrates.rs_rates[i],
					IEEE80211_MODE_AUTO);
		else
			mword = ieee80211_mcs2media(ic, allrates.rs_rates[i],
					IEEE80211_MODE_AUTO);
		if (mword == 0)
			continue;
		mword = IFM_SUBTYPE(mword);	/* remove media options */
		ADD(media, mword, 0);
		if (caps & IEEE80211_C_IBSS)
			ADD(media, mword, IFM_IEEE80211_ADHOC);
		if (caps & IEEE80211_C_HOSTAP)
			ADD(media, mword, IFM_IEEE80211_HOSTAP);
		if (caps & IEEE80211_C_AHDEMO)
			ADD(media, mword, IFM_IEEE80211_ADHOC | IFM_FLAG0);
		if (caps & IEEE80211_C_MONITOR)
			ADD(media, mword, IFM_IEEE80211_MONITOR);
		if (caps & IEEE80211_C_WDS)
			ADD(media, mword, IFM_IEEE80211_WDS);
	}
	return maxrate;
#undef ADD
}

void
ieee80211_announce(struct ieee80211com *ic)
{
	int i, mode, rate, mword;
	struct ieee80211_rateset *rs;

	for (mode = IEEE80211_MODE_11A; mode < IEEE80211_MODE_MAX; mode++) {
		if ((ic->ic_modecaps & (1<<mode)) == 0)
			continue;
		printk("%s rates: ", ieee80211_phymode_name[mode]);
		rs = &ic->ic_sup_rates[mode];
		for (i = 0; i < rs->rs_nrates; i++) {
			rate = rs->rs_rates[i];
			mword = ieee80211_rate2media(ic, rate, mode);
			if (mword == 0)
				continue;
			printf("%s%d%sMbps", (i != 0 ? " " : ""),
			    (rate & IEEE80211_RATE_VAL) / 2,
			    ((rate & 0x1) != 0 ? ".5" : ""));
		}
		printf("\n");
	}

	printk("H/W encryption support:");

	if (ic->ic_caps & IEEE80211_C_WEP)
		printk(" WEP");
	if (ic->ic_caps & IEEE80211_C_AES)
		printk(" AES");
	if (ic->ic_caps & IEEE80211_C_AES_CCM)
		printk(" AES_CCM");
	if (ic->ic_caps & IEEE80211_C_CKIP)
		printk(" CKIP");
	if (ic->ic_caps & IEEE80211_C_TKIP)
		printk(" TKIP");
	printk("\n");
}
EXPORT_SYMBOL(ieee80211_announce);

void
ieee80211_announce_channels(struct ieee80211com *ic)
{
	const struct ieee80211_channel *c;
	char type;
	int i;

	printf("Chan  Freq  RegPwr  MinPwr  MaxPwr\n");
	for (i = 0; i < ic->ic_nchans; i++) {
		c = &ic->ic_channels[i];
		if (IEEE80211_IS_CHAN_ST(c))
			type = 'S';
		else if (IEEE80211_IS_CHAN_108A(c))
			type = 'T';
		else if (IEEE80211_IS_CHAN_108G(c))
			type = 'G';
		else if (IEEE80211_IS_CHAN_A(c))
			type = 'a';
		else if (IEEE80211_IS_CHAN_11NG(c))
			type = 'n';
		else if (IEEE80211_IS_CHAN_11NA(c))
			type = 'n';
		else if (IEEE80211_IS_CHAN_ANYG(c))
			type = 'g';
		else if (IEEE80211_IS_CHAN_B(c))
			type = 'b';
		else
			type = 'f';
		printf("%4d  %4d%c %6d  %6d  %6d\n"
			, c->ic_ieee, c->ic_freq, type
			, c->ic_maxregpower
			, c->ic_minpower, c->ic_maxpower
		);
	}
}
EXPORT_SYMBOL(ieee80211_announce_channels);

/*
 * Common code to calculate the media status word
 */
static int
media_status(enum ieee80211_opmode opmode, u_int16_t mode)
{
	int status;

	status = IFM_IEEE80211;
	switch (opmode) {
	case IEEE80211_M_STA:
		break;
	case IEEE80211_M_AHDEMO:
		status |= IFM_IEEE80211_ADHOC | IFM_FLAG0;
		break;
	case IEEE80211_M_IBSS:
		status |= IFM_IEEE80211_ADHOC;
		break;
	case IEEE80211_M_HOSTAP:
		status |= IFM_IEEE80211_HOSTAP;
		break;
	case IEEE80211_M_MONITOR:
		status |= IFM_IEEE80211_MONITOR;
		break;
	case IEEE80211_M_WDS:
		status |= IFM_IEEE80211_WDS;
		break;
	}

	status |= IFM_MAKEMODE(mode);

	return status;
}

/*
 * Handle a media requests on the base interface.
 */
static void
ieee80211com_media_status(void *data, struct ifmediareq *imr)
{
	struct ieee80211com *ic = (struct ieee80211com *) data;

	imr->ifm_status = IFM_AVALID;
	if (!TAILQ_EMPTY(&ic->ic_vaps))
		imr->ifm_status |= IFM_ACTIVE;
	imr->ifm_active = media_status(ic->ic_opmode, 0);
}

/*
 * Convert a media specification to an 802.11 phy mode.
 */
static int
media2mode(const struct ifmedia_entry *ime, enum ieee80211_phymode *mode)
{

	switch (IFM_MODE(ime->ifm_media)) {
	case IFM_IEEE80211_11A:
		*mode = IEEE80211_MODE_11A;
		break;
	case IFM_IEEE80211_11B:
		*mode = IEEE80211_MODE_11B;
		break;
	case IFM_IEEE80211_11G:
		*mode = IEEE80211_MODE_11G;
		break;
	case IFM_IEEE80211_11NG:
		*mode = IEEE80211_MODE_11NG;
		break;
	case IFM_IEEE80211_11NA:
		*mode = IEEE80211_MODE_11NA;
		break;
	case IFM_IEEE80211_11NG_HT40PM:
		*mode = IEEE80211_MODE_11NG_HT40PM;
		break;
	case IFM_IEEE80211_11NA_HT40PM:
		*mode = IEEE80211_MODE_11NA_HT40PM;
		break;
	case IFM_IEEE80211_FH:
		*mode = IEEE80211_MODE_FH;
		break;
	case IFM_IEEE80211_11AC_VHT20PM:
		*mode = IEEE80211_MODE_11AC_VHT20PM;
		break;
	case IFM_IEEE80211_11AC_VHT40PM:
		*mode = IEEE80211_MODE_11AC_VHT40PM;
		break;
	case IFM_IEEE80211_11AC_VHT80PM:
		*mode = IEEE80211_MODE_11AC_VHT80PM;
		break;
	case IFM_IEEE80211_11AC_VHT160PM:
		*mode = IEEE80211_MODE_11AC_VHT160PM;
		break;
	case IFM_AUTO:
		*mode = IEEE80211_MODE_AUTO;
		break;
	default:
		return 0;
	}
	/*
	 * Turbo mode is an ``option''.  
	 * XXX: Turbo currently does not apply to AUTO
	 */
	if (ime->ifm_media & IFM_IEEE80211_TURBO) {
		if (*mode == IEEE80211_MODE_11A)
			*mode = IEEE80211_MODE_TURBO_A;
		else if (*mode == IEEE80211_MODE_11G)
			*mode = IEEE80211_MODE_TURBO_G;
		else
			return 0;
	}
	return 1;
}

static int
ieee80211com_media_change(void *data)
{
	struct ieee80211com *ic = (struct ieee80211com *) data;
	struct ieee80211vap *vap;
	struct ifmedia_entry *ime = ic->ic_media.ifm_cur;
	enum ieee80211_phymode newphymode;
	int j, error = 0;

	/* XXX is rtnl held here? */
	/*
	 * First, identify the phy mode.
	 */
	if (!media2mode(ime, &newphymode))
		return -EINVAL;
	/* NB: mode must be supported, no need to check */
	/*
	 * Autoselect doesn't make sense when operating as an AP.
	 * If no phy mode has been selected, pick one and lock it
	 * down so rate tables can be used in forming beacon frames
	 * and the like.
	 */

	if (ic->ic_opmode == IEEE80211_M_HOSTAP &&
	    newphymode == IEEE80211_MODE_AUTO) {
		for (j = IEEE80211_MODE_11A; j < IEEE80211_MODE_MAX; j++)
			if (ic->ic_modecaps & (1 << j)) {
				newphymode = j;
				break;
			}
	}

	/*
	 * Handle phy mode change.
	 */

	IEEE80211_LOCK_IRQ(ic);
	if (ic->ic_curmode != newphymode) {		/* change phy mode */
		error = ieee80211_setmode(ic, newphymode);
		if (error != 0) {
			IEEE80211_UNLOCK_IRQ_EARLY(ic);
			return error;
		}
		TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
			/* reset WME state */
			ieee80211_wme_initparams_locked(vap);
			ieee80211_adjust_wme_by_vappri(ic);
			/*
			 * Setup an initial rate set according to the
			 * current/default channel selected above.  This
			 * will be changed when scanning but must exist
			 * now so drivers have a consistent state.
			 */
			KASSERT(vap->iv_bss != NULL, ("no bss node"));
			vap->iv_bss->ni_rates = ic->ic_sup_rates[newphymode];
		}
		error = -ENETRESET;
	}
	IEEE80211_UNLOCK_IRQ(ic);

#ifdef notdef
	if (error == 0)
		ifp->if_baudrate = ifmedia_baudrate(ime->ifm_media);
#endif
	return error;
}

static int
findrate(struct ieee80211com *ic, enum ieee80211_phymode mode, int rate)
{
#define	IEEERATE(_ic,_m,_i) \
	((_ic)->ic_sup_rates[_m].rs_rates[_i] & IEEE80211_RATE_VAL)
#define	IEEE11NRATE(_ic,_m,_i) \
	((_ic)->ic_sup_rates[_m].rs_rates[_i])
	int i, nrates = ic->ic_sup_rates[mode].rs_nrates;
	for (i = 0; i < nrates; i++)
	{
		if(i < IEEE80211_RATE_SIZE)
		{
			/* Legacy Rates */
			if (IEEERATE(ic, mode, i) == rate)
				return i;
		}
		else
		{
			/* 11n rates */
			if (IEEE11NRATE(ic, mode, i) == rate)
				return i;
		}
	}
	return -1;
#undef IEEERATE
#undef IEEE11NRATE
}

/*
 * Convert a media specification to a rate index and possibly a mode
 * (if the rate is fixed and the mode is specified as ``auto'' then
 * we need to lock down the mode so the index is meaningful).
 */
static int
checkrate(struct ieee80211com *ic, enum ieee80211_phymode mode, int rate)
{

	/*
	 * Check the rate table for the specified/current phy.
	 */
	if (mode == IEEE80211_MODE_AUTO) {
		int i;
		/*
		 * In autoselect mode search for the rate.
		 */
		for (i = IEEE80211_MODE_11A; i < IEEE80211_MODE_MAX; i++) {
			if ((ic->ic_modecaps & (1 << i)) &&
			    findrate(ic, i, rate) != -1)
				return 1;
		}
		return 0;
	} else {
		/*
		 * Mode is fixed, check for rate.
		 */
		return (findrate(ic, mode, rate) != -1);
	}
}

/*
 * Handle a media change request; the only per-vap
 * information that is meaningful is the fixed rate
 * and desired phy mode.
 */
int
ieee80211_media_change(void *data)
{
	struct ieee80211vap *vap = (struct ieee80211vap *) data;
	struct ieee80211com *ic = vap->iv_ic;
	struct ifmedia_entry *ime = vap->iv_media.ifm_cur;
	enum ieee80211_phymode newmode;
	int newrate, error;

	/*
	 * First, identify the desired phy mode.
	 */
	if (!media2mode(ime, &newmode)) {
		return -EINVAL;
	}

	/*
	 * Check for fixed/variable rate.
	 */
	if (IFM_SUBTYPE(ime->ifm_media) != IFM_AUTO) {
		/*
		 * Convert media subtype to rate and potentially
		 * lock down the mode.
		 */

		if(newmode >= IEEE80211_MODE_11NA)
			newrate = ieee80211_media2mcs(ime->ifm_media);
		else
			newrate = ieee80211_media2rate(ime->ifm_media);

		if (newrate == 0 || !checkrate(ic, newmode, newrate))
		{
			return -EINVAL;
		}
	} else
		newrate = IEEE80211_FIXED_RATE_NONE;

	/*
	 * Install the rate+mode settings.
	 */
	error = 0;
	if (vap->iv_fixed_rate != newrate ||
		newrate == IEEE80211_FIXED_RATE_NONE) {
		vap->iv_fixed_rate = newrate;		/* fixed tx rate */
		error = -ENETRESET;

		if (newrate == IEEE80211_FIXED_RATE_NONE)
			newrate = 0x90; // To put MuC in Auto Rate

		/* Forward these parameters to the driver and MuC */
		ieee80211_param_to_qdrv(vap, IEEE80211_PARAM_FIXED_TX_RATE, newrate, NULL, 0);
	}

	if (ic->ic_des_mode != newmode) {
		ic->ic_des_mode = newmode;		/* desired phymode */
		error = -ENETRESET;
	}
	return error;
}
EXPORT_SYMBOL(ieee80211_media_change);

void
ieee80211_media_status(void *data, struct ifmediareq *imr)
{
	struct ieee80211vap *vap = (struct ieee80211vap *) data;
	struct ieee80211com *ic = vap->iv_ic;
	enum ieee80211_phymode mode;
	int mediarate = IFM_AUTO;

	imr->ifm_status = IFM_AVALID;
	/*
	 * NB: use the current channel's mode to lock down a xmit
	 * rate only when running; otherwise we may have a mismatch
	 * in which case the rate will not be convertible.
	 */
	if (vap->iv_state == IEEE80211_S_RUN) {
		imr->ifm_status |= IFM_ACTIVE;
		mode = ic->ic_curmode;
	} else {
		mode = IEEE80211_MODE_AUTO;
	}

	/*
	 * FIXME: Bug #2324
	 * Assumption that QTN devices support 5Ghz N channels so we
	 * calculate the IFM based on desired mode only
	 */
	imr->ifm_active = media_status(vap->iv_opmode, ic->ic_des_mode);

	/*
	 * Calculate a current rate if possible.
	 */
	if (vap->iv_state == IEEE80211_S_RUN) {
		if (vap->iv_fixed_rate != IEEE80211_FIXED_RATE_NONE) {
			/*
			 * A fixed rate is set, report that.
			 */
			if (mode < IEEE80211_MODE_11NA) {
				/* Legacy mode */
				imr->ifm_active |= ieee80211_rate2media(ic,
					vap->iv_fixed_rate, mode);
			} else {
				/* 11n mode */
				mediarate |= ieee80211_mcs2media(ic,
					vap->iv_fixed_rate, mode);
				if (IFM_AUTO == mediarate)
					SCSDBG(SCSLOG_INFO, "Couldn't find compatible mediarate\n");
				imr->ifm_active |= mediarate;
			}
		} else {
			imr->ifm_active |= IFM_AUTO;
		}
	}
}
EXPORT_SYMBOL(ieee80211_media_status);

/*
 * Set the current phy mode.
 */
int
ieee80211_setmode(struct ieee80211com *ic, enum ieee80211_phymode mode)
{
	if (ic->ic_des_mode != IEEE80211_MODE_11B &&
		ic->ic_des_mode != IEEE80211_MODE_11A) {
		ieee80211_reset_erp(ic, mode);	/* reset ERP state */
	}

	ic->ic_curmode = mode;		/* NB: must do post reset_erp */
	return 0;
}
EXPORT_SYMBOL(ieee80211_setmode);

/*
 * Return the phy mode for with the specified channel.
 */
enum ieee80211_phymode
ieee80211_chan2mode(const struct ieee80211_channel *chan)
{
	/*
	 * Callers should handle this case properly, rather than
	 * just relying that this function returns a sane value.
	 * XXX Probably needs to be revised.
	 */
	KASSERT(chan != IEEE80211_CHAN_ANYC, ("channel not setup"));

	if (IEEE80211_IS_CHAN_11AC_VHT80_EDGEPLUS(chan))
		return IEEE80211_MODE_11AC_VHT80PM;
	else if (IEEE80211_IS_CHAN_11AC_VHT80_CNTRPLUS(chan))
		return IEEE80211_MODE_11AC_VHT80PM;
	else if (IEEE80211_IS_CHAN_11AC_VHT80_CNTRMINUS(chan))
		return IEEE80211_MODE_11AC_VHT80PM;
	else if (IEEE80211_IS_CHAN_11AC_VHT80_EDGEMINUS(chan))
		return IEEE80211_MODE_11AC_VHT80PM;
	if (IEEE80211_IS_CHAN_11AC_VHT40PLUS(chan))
		return IEEE80211_MODE_11AC_VHT40PM;
	else if (IEEE80211_IS_CHAN_11AC_VHT40MINUS(chan))
		return IEEE80211_MODE_11AC_VHT40PM;
	if (IEEE80211_IS_CHAN_11AC(chan))
		return IEEE80211_MODE_11AC_VHT20PM;
	if (IEEE80211_IS_CHAN_11NG_HT40PLUS(chan))
		return IEEE80211_MODE_11NG_HT40PM;
	else if (IEEE80211_IS_CHAN_11NG_HT40MINUS(chan))
		return IEEE80211_MODE_11NG_HT40PM;
	if (IEEE80211_IS_CHAN_11NA_HT40PLUS(chan))
		return IEEE80211_MODE_11NA_HT40PM;
	else if (IEEE80211_IS_CHAN_11NA_HT40MINUS(chan))
		return IEEE80211_MODE_11NA_HT40PM;
	if (IEEE80211_IS_CHAN_11NG(chan))
		return IEEE80211_MODE_11NG;
	else if (IEEE80211_IS_CHAN_11NA(chan))
		return IEEE80211_MODE_11NA;
	else if (IEEE80211_IS_CHAN_108G(chan))
		return IEEE80211_MODE_TURBO_G;
	else if (IEEE80211_IS_CHAN_TURBO(chan))
		return IEEE80211_MODE_TURBO_A;
	else if (IEEE80211_IS_CHAN_A(chan))
		return IEEE80211_MODE_11A;
	else if (IEEE80211_IS_CHAN_ANYG(chan))
		return IEEE80211_MODE_11G;
	else if (IEEE80211_IS_CHAN_B(chan))
		return IEEE80211_MODE_11B;
	else if (IEEE80211_IS_CHAN_FHSS(chan))
		return IEEE80211_MODE_FH;

	/* NB: should not get here */
	printk("%s: cannot map channel to mode; freq %u flags 0x%x\n",
		__func__, chan->ic_freq, chan->ic_flags);
	return IEEE80211_MODE_11B;
}
EXPORT_SYMBOL(ieee80211_chan2mode);

/*
 * convert IEEE80211 rate value to ifmedia subtype.
 * ieee80211 rate is in unit of 0.5Mbps.
 */
int
ieee80211_rate2media(struct ieee80211com *ic, int rate, enum ieee80211_phymode mode)
{
#define	N(a)	(sizeof(a) / sizeof(a[0]))
	static const struct {
		u_int	m;	/* rate + mode */
		u_int	r;	/* if_media rate */
	} rates[] = {
		{   2 | IFM_IEEE80211_FH, IFM_IEEE80211_FH1 },
		{   4 | IFM_IEEE80211_FH, IFM_IEEE80211_FH2 },
		{   2 | IFM_IEEE80211_11B, IFM_IEEE80211_DS1 },
		{   4 | IFM_IEEE80211_11B, IFM_IEEE80211_DS2 },
		{  11 | IFM_IEEE80211_11B, IFM_IEEE80211_DS5 },
		{  22 | IFM_IEEE80211_11B, IFM_IEEE80211_DS11 },
		{  44 | IFM_IEEE80211_11B, IFM_IEEE80211_DS22 },
		{   3 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM1_50 },
		{   4 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM2_25 },
		{   6 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM3 },
		{   9 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM4_50 },
		{  12 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM6 },
		{  18 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM9 },
		{  24 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM12 },
		{  27 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM13_5 },
		{  36 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM18 },
		{  48 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM24 },
		{  54 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM27 },
		{  72 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM36 },
		{  96 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM48 },
		{ 108 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM54 },
		{   2 | IFM_IEEE80211_11G, IFM_IEEE80211_DS1 },
		{   4 | IFM_IEEE80211_11G, IFM_IEEE80211_DS2 },
		{  11 | IFM_IEEE80211_11G, IFM_IEEE80211_DS5 },
		{  22 | IFM_IEEE80211_11G, IFM_IEEE80211_DS11 },
		{  12 | IFM_IEEE80211_11G, IFM_IEEE80211_OFDM6 },
		{  18 | IFM_IEEE80211_11G, IFM_IEEE80211_OFDM9 },
		{  24 | IFM_IEEE80211_11G, IFM_IEEE80211_OFDM12 },
		{  36 | IFM_IEEE80211_11G, IFM_IEEE80211_OFDM18 },
		{  48 | IFM_IEEE80211_11G, IFM_IEEE80211_OFDM24 },
		{  72 | IFM_IEEE80211_11G, IFM_IEEE80211_OFDM36 },
		{  96 | IFM_IEEE80211_11G, IFM_IEEE80211_OFDM48 },
		{ 108 | IFM_IEEE80211_11G, IFM_IEEE80211_OFDM54 },
		/* NB: OFDM72 doesn't really exist so we don't handle it */
	};
	u_int mask, i;

	mask = rate & IEEE80211_RATE_VAL;
	switch (mode) {
	case IEEE80211_MODE_11A:
	case IEEE80211_MODE_TURBO_A:
		mask |= IFM_IEEE80211_11A;
		break;
	case IEEE80211_MODE_11B:
		mask |= IFM_IEEE80211_11B;
		break;
	case IEEE80211_MODE_FH:
		mask |= IFM_IEEE80211_FH;
		break;
	case IEEE80211_MODE_AUTO:
		/* NB: ic may be NULL for some drivers */
		if (ic && ic->ic_phytype == IEEE80211_T_FH) {
			mask |= IFM_IEEE80211_FH;
			break;
		}
		/* NB: hack, 11g matches both 11b+11a rates */
		/* fall thru... */
	case IEEE80211_MODE_11G:
	case IEEE80211_MODE_TURBO_G:
		mask |= IFM_IEEE80211_11G;
		break;
	default:
		break;
	}
	for (i = 0; i < N(rates); i++)
		if (rates[i].m == mask)
			return rates[i].r;
	return IFM_AUTO;
#undef N
}
EXPORT_SYMBOL(ieee80211_rate2media);

int
ieee80211_mcs2media(struct ieee80211com *ic, int mcs, enum ieee80211_phymode mode)
{
#define	N(a)	(sizeof(a) / sizeof(a[0]))
	static const struct {
		u_int	m;	/* rate + mode */
		u_int	r;	/* if_media rate */
	} rates[] = {

		/* Only MCS0-MCS15 (2 streams) are supported */
		{  12 | IFM_IEEE80211_11NA, IFM_IEEE80211_OFDM_HT_LEG_6 },
		{  18 | IFM_IEEE80211_11NA, IFM_IEEE80211_OFDM_HT_LEG_9 },
		{  24 | IFM_IEEE80211_11NA, IFM_IEEE80211_OFDM_HT_LEG_12 },
		{  36 | IFM_IEEE80211_11NA, IFM_IEEE80211_OFDM_HT_LEG_18 },
		{  48 | IFM_IEEE80211_11NA, IFM_IEEE80211_OFDM_HT_LEG_24 },
		{  72 | IFM_IEEE80211_11NA, IFM_IEEE80211_OFDM_HT_LEG_36 },
		{  96 | IFM_IEEE80211_11NA, IFM_IEEE80211_OFDM_HT_LEG_48 },
		{ 108 | IFM_IEEE80211_11NA, IFM_IEEE80211_OFDM_HT_LEG_54 },
		{  12 | IFM_IEEE80211_11NG, IFM_IEEE80211_OFDM_HT_LEG_6 },
		{  18 | IFM_IEEE80211_11NG, IFM_IEEE80211_OFDM_HT_LEG_9 },
		{  24 | IFM_IEEE80211_11NG, IFM_IEEE80211_OFDM_HT_LEG_12 },
		{  36 | IFM_IEEE80211_11NG, IFM_IEEE80211_OFDM_HT_LEG_18 },
		{  48 | IFM_IEEE80211_11NG, IFM_IEEE80211_OFDM_HT_LEG_24 },
		{  72 | IFM_IEEE80211_11NG, IFM_IEEE80211_OFDM_HT_LEG_36 },
		{  96 | IFM_IEEE80211_11NG, IFM_IEEE80211_OFDM_HT_LEG_48 },
		{ 108 | IFM_IEEE80211_11NG, IFM_IEEE80211_OFDM_HT_LEG_54 },

		{  12 | IFM_IEEE80211_11NA_HT40PM, IFM_IEEE80211_OFDM_HT_LEG_6 },
		{  18 | IFM_IEEE80211_11NA_HT40PM, IFM_IEEE80211_OFDM_HT_LEG_9 },
		{  24 | IFM_IEEE80211_11NA_HT40PM, IFM_IEEE80211_OFDM_HT_LEG_12 },
		{  36 | IFM_IEEE80211_11NA_HT40PM, IFM_IEEE80211_OFDM_HT_LEG_18 },
		{  48 | IFM_IEEE80211_11NA_HT40PM, IFM_IEEE80211_OFDM_HT_LEG_24 },
		{  72 | IFM_IEEE80211_11NA_HT40PM, IFM_IEEE80211_OFDM_HT_LEG_36 },
		{  96 | IFM_IEEE80211_11NA_HT40PM, IFM_IEEE80211_OFDM_HT_LEG_48 },
		{ 108 | IFM_IEEE80211_11NA_HT40PM, IFM_IEEE80211_OFDM_HT_LEG_54 },
		{  12 | IFM_IEEE80211_11NG_HT40PM, IFM_IEEE80211_OFDM_HT_LEG_6 },
		{  18 | IFM_IEEE80211_11NG_HT40PM, IFM_IEEE80211_OFDM_HT_LEG_9 },
		{  24 | IFM_IEEE80211_11NG_HT40PM, IFM_IEEE80211_OFDM_HT_LEG_12 },
		{  36 | IFM_IEEE80211_11NG_HT40PM, IFM_IEEE80211_OFDM_HT_LEG_18 },
		{  48 | IFM_IEEE80211_11NG_HT40PM, IFM_IEEE80211_OFDM_HT_LEG_24 },
		{  72 | IFM_IEEE80211_11NG_HT40PM, IFM_IEEE80211_OFDM_HT_LEG_36 },
		{  96 | IFM_IEEE80211_11NG_HT40PM, IFM_IEEE80211_OFDM_HT_LEG_48 },
		{ 108 | IFM_IEEE80211_11NG_HT40PM, IFM_IEEE80211_OFDM_HT_LEG_54 },
#if 0
		{  13  | IFM_IEEE80211_11NA, IFM_IEEE80211_OFDM_HT_0 },
		{  26  | IFM_IEEE80211_11NA, IFM_IEEE80211_OFDM_HT_1 },
		{  39  | IFM_IEEE80211_11NA, IFM_IEEE80211_OFDM_HT_2 },
		{  52  | IFM_IEEE80211_11NA, IFM_IEEE80211_OFDM_HT_3 },
		{  78  | IFM_IEEE80211_11NA, IFM_IEEE80211_OFDM_HT_4 },
		{  104 | IFM_IEEE80211_11NA, IFM_IEEE80211_OFDM_HT_5 },
		{  117 | IFM_IEEE80211_11NA, IFM_IEEE80211_OFDM_HT_6 },
		{  130 | IFM_IEEE80211_11NA, IFM_IEEE80211_OFDM_HT_7 },
		{  26  | IFM_IEEE80211_11NA, IFM_IEEE80211_OFDM_HT_8 },
		{  52  | IFM_IEEE80211_11NA, IFM_IEEE80211_OFDM_HT_9 },
		{  78  | IFM_IEEE80211_11NA, IFM_IEEE80211_OFDM_HT_10 },
		{  104 | IFM_IEEE80211_11NA, IFM_IEEE80211_OFDM_HT_11 },
		{  156 | IFM_IEEE80211_11NA, IFM_IEEE80211_OFDM_HT_12 },
		{  208 | IFM_IEEE80211_11NA, IFM_IEEE80211_OFDM_HT_13 },
		{  234 | IFM_IEEE80211_11NA, IFM_IEEE80211_OFDM_HT_14 },
		{  260 | IFM_IEEE80211_11NA, IFM_IEEE80211_OFDM_HT_15 },
		{  13  | IFM_IEEE80211_11NG, IFM_IEEE80211_OFDM_HT_0 },
		{  26  | IFM_IEEE80211_11NG, IFM_IEEE80211_OFDM_HT_1 },
		{  39  | IFM_IEEE80211_11NG, IFM_IEEE80211_OFDM_HT_2 },
		{  52  | IFM_IEEE80211_11NG, IFM_IEEE80211_OFDM_HT_3 },
		{  78  | IFM_IEEE80211_11NG, IFM_IEEE80211_OFDM_HT_4 },
		{  104 | IFM_IEEE80211_11NG, IFM_IEEE80211_OFDM_HT_5 },
		{  117 | IFM_IEEE80211_11NG, IFM_IEEE80211_OFDM_HT_6 },
		{  130 | IFM_IEEE80211_11NG, IFM_IEEE80211_OFDM_HT_7 },
		{  26  | IFM_IEEE80211_11NG, IFM_IEEE80211_OFDM_HT_8 },
		{  52  | IFM_IEEE80211_11NG, IFM_IEEE80211_OFDM_HT_9 },
		{  78  | IFM_IEEE80211_11NG, IFM_IEEE80211_OFDM_HT_10 },
		{  104 | IFM_IEEE80211_11NG, IFM_IEEE80211_OFDM_HT_11 },
		{  156 | IFM_IEEE80211_11NG, IFM_IEEE80211_OFDM_HT_12 },
		{  208 | IFM_IEEE80211_11NG, IFM_IEEE80211_OFDM_HT_13 },
		{  234 | IFM_IEEE80211_11NG, IFM_IEEE80211_OFDM_HT_14 },
		{  260 | IFM_IEEE80211_11NG, IFM_IEEE80211_OFDM_HT_15 },
#else
		{  0x80  | IFM_IEEE80211_11NA, IFM_IEEE80211_OFDM_HT_0 },
		{  0x81  | IFM_IEEE80211_11NA, IFM_IEEE80211_OFDM_HT_1 },
		{  0x82  | IFM_IEEE80211_11NA, IFM_IEEE80211_OFDM_HT_2 },
		{  0x83  | IFM_IEEE80211_11NA, IFM_IEEE80211_OFDM_HT_3 },
		{  0x84  | IFM_IEEE80211_11NA, IFM_IEEE80211_OFDM_HT_4 },
		{  0x85 | IFM_IEEE80211_11NA, IFM_IEEE80211_OFDM_HT_5 },
		{  0x86 | IFM_IEEE80211_11NA, IFM_IEEE80211_OFDM_HT_6 },
		{  0x87 | IFM_IEEE80211_11NA, IFM_IEEE80211_OFDM_HT_7 },
		{  0x88  | IFM_IEEE80211_11NA, IFM_IEEE80211_OFDM_HT_8 },
		{  0x89  | IFM_IEEE80211_11NA, IFM_IEEE80211_OFDM_HT_9 },
		{  0x8A  | IFM_IEEE80211_11NA, IFM_IEEE80211_OFDM_HT_10 },
		{  0x8B | IFM_IEEE80211_11NA, IFM_IEEE80211_OFDM_HT_11 },
		{  0x8C | IFM_IEEE80211_11NA, IFM_IEEE80211_OFDM_HT_12 },
		{  0x8D | IFM_IEEE80211_11NA, IFM_IEEE80211_OFDM_HT_13 },
		{  0x8E | IFM_IEEE80211_11NA, IFM_IEEE80211_OFDM_HT_14 },
		{  0x8F | IFM_IEEE80211_11NA, IFM_IEEE80211_OFDM_HT_15 },
		{  0x80  | IFM_IEEE80211_11NG, IFM_IEEE80211_OFDM_HT_0 },
		{  0x81  | IFM_IEEE80211_11NG, IFM_IEEE80211_OFDM_HT_1 },
		{  0x82  | IFM_IEEE80211_11NG, IFM_IEEE80211_OFDM_HT_2 },
		{  0x83  | IFM_IEEE80211_11NG, IFM_IEEE80211_OFDM_HT_3 },
		{  0x84  | IFM_IEEE80211_11NG, IFM_IEEE80211_OFDM_HT_4 },
		{  0x85 | IFM_IEEE80211_11NG, IFM_IEEE80211_OFDM_HT_5 },
		{  0x86 | IFM_IEEE80211_11NG, IFM_IEEE80211_OFDM_HT_6 },
		{  0x87 | IFM_IEEE80211_11NG, IFM_IEEE80211_OFDM_HT_7 },
		{  0x88  | IFM_IEEE80211_11NG, IFM_IEEE80211_OFDM_HT_8 },
		{  0x89  | IFM_IEEE80211_11NG, IFM_IEEE80211_OFDM_HT_9 },
		{  0x8A  | IFM_IEEE80211_11NG, IFM_IEEE80211_OFDM_HT_10 },
		{  0x8B | IFM_IEEE80211_11NG, IFM_IEEE80211_OFDM_HT_11 },
		{  0x8C | IFM_IEEE80211_11NG, IFM_IEEE80211_OFDM_HT_12 },
		{  0x8D | IFM_IEEE80211_11NG, IFM_IEEE80211_OFDM_HT_13 },
		{  0x8E | IFM_IEEE80211_11NG, IFM_IEEE80211_OFDM_HT_14 },
		{  0x8F | IFM_IEEE80211_11NG, IFM_IEEE80211_OFDM_HT_15 },

		{  0x80  | IFM_IEEE80211_11NA_HT40PM, IFM_IEEE80211_OFDM_HT_0 },
		{  0x81  | IFM_IEEE80211_11NA_HT40PM, IFM_IEEE80211_OFDM_HT_1 },
		{  0x82  | IFM_IEEE80211_11NA_HT40PM, IFM_IEEE80211_OFDM_HT_2 },
		{  0x83  | IFM_IEEE80211_11NA_HT40PM, IFM_IEEE80211_OFDM_HT_3 },
		{  0x84  | IFM_IEEE80211_11NA_HT40PM, IFM_IEEE80211_OFDM_HT_4 },
		{  0x85 | IFM_IEEE80211_11NA_HT40PM, IFM_IEEE80211_OFDM_HT_5 },
		{  0x86 | IFM_IEEE80211_11NA_HT40PM, IFM_IEEE80211_OFDM_HT_6 },
		{  0x87 | IFM_IEEE80211_11NA_HT40PM, IFM_IEEE80211_OFDM_HT_7 },
		{  0x88  | IFM_IEEE80211_11NA_HT40PM, IFM_IEEE80211_OFDM_HT_8 },
		{  0x89  | IFM_IEEE80211_11NA_HT40PM, IFM_IEEE80211_OFDM_HT_9 },
		{  0x8A  | IFM_IEEE80211_11NA_HT40PM, IFM_IEEE80211_OFDM_HT_10 },
		{  0x8B | IFM_IEEE80211_11NA_HT40PM, IFM_IEEE80211_OFDM_HT_11 },
		{  0x8C | IFM_IEEE80211_11NA_HT40PM, IFM_IEEE80211_OFDM_HT_12 },
		{  0x8D | IFM_IEEE80211_11NA_HT40PM, IFM_IEEE80211_OFDM_HT_13 },
		{  0x8E | IFM_IEEE80211_11NA_HT40PM, IFM_IEEE80211_OFDM_HT_14 },
		{  0x8F | IFM_IEEE80211_11NA_HT40PM, IFM_IEEE80211_OFDM_HT_15 },
		{  0x80  | IFM_IEEE80211_11NG_HT40PM, IFM_IEEE80211_OFDM_HT_0 },
		{  0x81  | IFM_IEEE80211_11NG_HT40PM, IFM_IEEE80211_OFDM_HT_1 },
		{  0x82  | IFM_IEEE80211_11NG_HT40PM, IFM_IEEE80211_OFDM_HT_2 },
		{  0x83  | IFM_IEEE80211_11NG_HT40PM, IFM_IEEE80211_OFDM_HT_3 },
		{  0x84  | IFM_IEEE80211_11NG_HT40PM, IFM_IEEE80211_OFDM_HT_4 },
		{  0x85 | IFM_IEEE80211_11NG_HT40PM, IFM_IEEE80211_OFDM_HT_5 },
		{  0x86 | IFM_IEEE80211_11NG_HT40PM, IFM_IEEE80211_OFDM_HT_6 },
		{  0x87 | IFM_IEEE80211_11NG_HT40PM, IFM_IEEE80211_OFDM_HT_7 },
		{  0x88  | IFM_IEEE80211_11NG_HT40PM, IFM_IEEE80211_OFDM_HT_8 },
		{  0x89  | IFM_IEEE80211_11NG_HT40PM, IFM_IEEE80211_OFDM_HT_9 },
		{  0x8A  | IFM_IEEE80211_11NG_HT40PM, IFM_IEEE80211_OFDM_HT_10 },
		{  0x8B | IFM_IEEE80211_11NG_HT40PM, IFM_IEEE80211_OFDM_HT_11 },
		{  0x8C | IFM_IEEE80211_11NG_HT40PM, IFM_IEEE80211_OFDM_HT_12 },
		{  0x8D | IFM_IEEE80211_11NG_HT40PM, IFM_IEEE80211_OFDM_HT_13 },
		{  0x8E | IFM_IEEE80211_11NG_HT40PM, IFM_IEEE80211_OFDM_HT_14 },
		{  0x8F | IFM_IEEE80211_11NG_HT40PM, IFM_IEEE80211_OFDM_HT_15 },
#endif

	};
	u_int mask, i;

	mask = mcs;
	switch (mode) {
	case IEEE80211_MODE_11NA:
		mask |= IFM_IEEE80211_11NA;
		break;
	case IEEE80211_MODE_11NG:
		mask |= IFM_IEEE80211_11NG;
		break;
	case IEEE80211_MODE_11NA_HT40PM:
		mask |= IFM_IEEE80211_11NA_HT40PM;
		break;
	case IEEE80211_MODE_11NG_HT40PM:
		mask |= IFM_IEEE80211_11NG_HT40PM;
		break;
	case IEEE80211_MODE_11AC_VHT20PM:
		mask |= IFM_IEEE80211_11AC_VHT20PM;
		break;
	case IEEE80211_MODE_11AC_VHT40PM:
		mask |= IFM_IEEE80211_11AC_VHT40PM;
		break;
	case IEEE80211_MODE_11AC_VHT80PM:
		mask |= IFM_IEEE80211_11AC_VHT80PM;
		break;
	default:
		break;
	}
	for (i = 0; i < N(rates); i++)
		if (rates[i].m == mask)
			return rates[i].r;
	return IFM_AUTO;
#undef N
}
EXPORT_SYMBOL(ieee80211_mcs2media);

int
ieee80211_mcs2rate(int mcs, int mode, int sgi, int vht)
{
#define N(a)    (sizeof(a[0]) / sizeof(a[0][0][0]))
	u_int32_t rates[2][2][77] = {{{

			  /* LGI & 20 MHz */
			  /* MCS0-MC31 (4 streams) are supported */
			  13, 26, 39, 52, 78, 104, 117, 130,
			  26, 52, 78, 104, 156, 208, 234, 260,
			  39, 78, 117, 156, 234, 312, 351, 390,
			  52, 104, 156, 208, 312, 416, 468, 520,

			  12, 78, 104, 130, 117, 156, 195, 104, /* UEQM */
			  130, 130, 156, 182, 182, 208, 156, 195,
			  195, 234, 273, 273, 312, 130, 156, 182,
			  156, 182, 208, 234, 208, 234, 260, 260,
			  286, 195, 234, 273, 234, 273, 312, 351,
			  312, 351, 390, 390, 429},
		  {

			  /* LGI & 40 MHz */
			  /* MCS0-MCS31 (4 streams) are supported */
			  27, 54, 81, 108, 162, 216, 243, 270,
			  54, 108, 162, 216, 324, 432, 486, 540,
			  81, 162, 243, 324, 486, 648, 729, 810,
			  108, 216, 324, 432, 648, 864, 972, 1080,

			  12, 162, 216, 270, 243, 324, 405, 216, /* UEQM */
			  270, 270, 324, 378, 378, 432, 324, 405,
			  405, 486, 567, 567, 648, 270, 324, 378,
			  324, 378, 432, 486, 432, 486, 540, 540,
			  594, 405, 486, 567, 486, 567, 648, 729,
			  648, 729, 810, 810, 891}},
		  {{

			   /* SGI & 20 MHz */
			   /* MCS0-MC31 (4 streams) are supported */
			   14, 28, 42, 56, 86, 114, 130, 144,
			   28, 56, 86, 114, 172, 230, 260, 288,
			   42, 86, 130, 172, 260, 346, 390, 432,
			   56, 114, 172, 230, 346, 462, 520, 576,

			   12, 86, 114, 144, 130, 172, 216, 86,	/* UEQM */
			   114, 114, 172, 202, 202, 230, 172, 216,
			   216, 260, 302, 302, 346, 144, 172, 202,
			   172, 202, 230, 260, 230, 260, 288, 288,
			   316, 216, 260, 302, 260, 302, 346, 390,
			   346, 390, 432, 432, 476},
		  {

			  /* SGI * 40 MHz */
			  /* MCS0-MC31 (4 streams) are supported */
			  30, 60, 90, 120, 180, 240, 270, 300,
			  60, 120, 180, 240, 360, 480, 540, 600,
			  90, 180, 270, 360, 540, 720, 810, 900,
			  120, 240, 360, 480, 720, 960, 1080,1200,

			  12, 180, 240, 300, 270, 360, 450, 240, /* UEQM */
			  300, 300, 360, 420, 420, 480, 360, 450,
			  450, 540, 630, 630, 720, 300, 360, 420,
			  360, 420, 480, 540, 480, 540, 600, 600,
			  660, 450, 540, 630, 540, 630, 720, 810,
			  720, 810, 900, 900, 990}}
		  };

	u_int32_t vht_rates[2][2][10] = {
		{{
			  /* LGI & 80 MHz */
			  /* MCS0-MC9 */
			  59, 117, 176, 234, 351, 468, 527, 585, 702, 780
		 },
		 {
			  /* LGI & 160/80+80 MHz */
			  /* MCS0-MC9 */
			  117, 234, 351, 468, 702, 936, 1053, 1170, 1404, 1560
		 }},
		{{
			  /* SGI & 80 MHz */
			  /* MCS0-MC9 */
			  65, 130, 195, 260, 390, 520, 585, 650, 780, 867
		 },
		 {
			  /* SGI & 160 MHz */
			  /* MCS0-MC9 */
			  130, 260, 390, 520, 780, 1040, 1170, 1300, 1560, 1733
		 }}};
	if (vht) {
		if(mcs >= 10)
			return -1;

		return (vht_rates[sgi][mode][mcs]);
	} else {
		if(mcs >= N(rates))
			return -1;

		return (rates[sgi][mode][mcs]);
	}

#undef N
}
EXPORT_SYMBOL(ieee80211_mcs2rate);

int
ieee80211_rate2mcs(int rate, int mode, int sgi)
{
#define N(a)    (sizeof(a[0]) / sizeof(a[0][0][0]))
	static const struct {
		u_int   r;      /* rate */
		u_int   m;      /* mcs */
	} rates[2][2][16] = {{{

			/* Only MCS0-MCS15 (2 streams) are supported */
			{13,          0x80 },
			{26,          0x81 },
			{39,          0x82 },
			{52,          0x83 },
			{78,          0x84 },
			{104,         0x85 },
			{117,         0x86 },
			{130,         0x87 },
			{26,          0x88 },
			{52,          0x89 },
			{78,          0x8A },
			{104,         0x8B },
			{156,         0x8C },
			{208,         0x8D },
			{234,         0x8E },
			{260,         0x8F },
		},
		{

			/* Only MCS0-MCS15 (2 streams) are supported */
			{27,          0x80 },
			{54,          0x81 },
			{81,          0x82 },
			{108,         0x83 },
			{162,         0x84 },
			{216,         0x85 },
			{243,         0x86 },
			{270,         0x87 },
			{54,          0x88 },
			{108,         0x89 },
			{162,         0x8A },
			{216,         0x8B },
			{324,         0x8C },
			{432,         0x8D },
			{486,         0x8E },
			{540,         0x8F },
		}},
		{{

			 /* Only MCS0-MCS15 (2 streams) are supported */
			 {14,          0x80 },
			 {28,          0x81 },
			 {42,          0x82 },
			 {56,          0x83 },
			 {86,          0x84 },
			 {114,         0x85 },
			 {130,         0x86 },
			 {144,         0x87 },
			 {28,          0x88 },
			 {56,          0x89 },
			 {86,          0x8A },
			 {114,         0x8B },
			 {172,         0x8C },
			 {230,         0x8D },
			 {260,         0x8E },
			 {288,         0x8F },
		 },
		{

			/* Only MCS0-MCS15 (2 streams) are supported */
			{30,          0x80 },
			{60,          0x81 },
			{90,          0x82 },
			{120,         0x83 },
			{180,         0x84 },
			{240,         0x85 },
			{270,         0x86 },
			{300,         0x87 },
			{60,          0x88 },
			{120,         0x89 },
			{180,         0x8A },
			{240,         0x8B },
			{360,         0x8C },
			{480,         0x8D },
			{540,         0x8E },
			{600,         0x8F },
		}}};
	int i;

	for (i = 0; i < N(rates); i++)
	{
		if (rates[sgi][mode][i].r == rate)
			return (rates[sgi][mode][i].m);
	}
	return -1;

#undef N
}
EXPORT_SYMBOL(ieee80211_rate2mcs);

int
ieee80211_media2rate(int mword)
{
#define	N(a)	(sizeof(a) / sizeof(a[0]))
	static const int ieeerates[] = {
		-1,		/* IFM_AUTO */
		0,		/* IFM_MANUAL */
		0,		/* IFM_NONE */
		2,		/* IFM_IEEE80211_FH1 */
		4,		/* IFM_IEEE80211_FH2 */
		2,		/* IFM_IEEE80211_DS1 */
		4,		/* IFM_IEEE80211_DS2 */
		11,		/* IFM_IEEE80211_DS5 */
		22,		/* IFM_IEEE80211_DS11 */
		44,		/* IFM_IEEE80211_DS22 */
		3,		/* IFM_IEEE80211_OFDM1_50 */
		4,		/* IFM_IEEE80211_OFDM2_25 */
		6,		/* IFM_IEEE80211_OFDM3 */
		9,		/* IFM_IEEE80211_OFDM4_50 */
		12,		/* IFM_IEEE80211_OFDM6 */
		18,		/* IFM_IEEE80211_OFDM9 */
		24,		/* IFM_IEEE80211_OFDM12 */
		27,		/* IFM_IEEE80211_OFDM13_5 */
		36,		/* IFM_IEEE80211_OFDM18 */
		48,		/* IFM_IEEE80211_OFDM24 */
		54,		/* IFM_IEEE80211_OFDM27 */
		72,		/* IFM_IEEE80211_OFDM36 */
		96,		/* IFM_IEEE80211_OFDM48 */
		108,		/* IFM_IEEE80211_OFDM54 */
		144,		/* IFM_IEEE80211_OFDM72 */
	};
	return IFM_SUBTYPE(mword) < N(ieeerates) ?
		ieeerates[IFM_SUBTYPE(mword)] : 0;
#undef N
}
EXPORT_SYMBOL(ieee80211_media2rate);

int
ieee80211_media2mcs(int mword)
{
#define	N(a)	(sizeof(a) / sizeof(a[0]))
	static const int ieee11nrates[] = {
		-1,		/* IFM_AUTO */
		12,		/* IFM_IEEE80211_OFDM_HT_LEG6 */
		18,		/* IFM_IEEE80211_OFDM_HT_LEG9 */
		24,		/* IFM_IEEE80211_OFDM_HT_LEG12 */
		36,		/* IFM_IEEE80211_OFDM_HT_LEG18 */
		48,		/* IFM_IEEE80211_OFDM_HT_LEG24 */
		72,		/* IFM_IEEE80211_OFDM_HT_LEG36 */
		96,		/* IFM_IEEE80211_OFDM_HT_LEG48 */
		108,	/* IFM_IEEE80211_OFDM_HT_LEG54 */
		0x80,		/* IFM_MCS_0 */
		0x81,		/* IFM_MCS_1 */
		0x82,		/* IFM_MCS_2 */
		0x83,		/* IFM_MCS_3 */
		0x84,		/* IFM_MCS_4 */
		0x85,		/* IFM_MCS_5 */
		0x86,		/* IFM_MCS_6 */
		0x87,		/* IFM_MCS_7 */
		0x88,		/* IFM_MCS_8 */
		0x89,		/* IFM_MCS_9 */
		0x8A,		/* IFM_MCS_10 */
		0x8B,		/* IFM_MCS_11 */
		0x8C,		/* IFM_MCS_12 */
		0x8D,		/* IFM_MCS_13 */
		0x8E,		/* IFM_MCS_14 */
		0x8F,		/* IFM_MCS_15 */
	};
	return IFM_SUBTYPE(mword) < N(ieee11nrates) ?
		ieee11nrates[IFM_SUBTYPE(mword)] : 0;
#undef N
}
EXPORT_SYMBOL(ieee80211_media2mcs);
/*
 * Return netdevice statistics.
 */
static struct net_device_stats *
ieee80211_getstats(struct net_device *dev)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct net_device_stats *stats = &vap->iv_devstats;

	if (vap->iv_ic->ic_get_shared_vap_stats && (vap->iv_ic->ic_get_shared_vap_stats(vap)) < 0)
		return stats;

	uint32_t extra_tx_errors = vap->iv_stats.is_tx_nodefkey
				 + vap->iv_stats.is_tx_noheadroom
				 + vap->iv_stats.is_crypto_enmicfail;
	uint32_t extra_tx_dropped = vap->iv_stats.is_tx_nobuf
				  + vap->iv_stats.is_tx_nonode
				  + vap->iv_stats.is_tx_unknownmgt
				  + vap->iv_stats.is_tx_badcipher
				  + vap->iv_stats.is_tx_nodefkey;
	uint32_t extra_rx_errors = vap->iv_stats.is_rx_tooshort
				  + vap->iv_stats.is_rx_wepfail
				  + vap->iv_stats.is_rx_decap
				  + vap->iv_stats.is_rx_nobuf
				  + vap->iv_stats.is_rx_decryptcrc
				  + vap->iv_stats.is_rx_ccmpmic
				  + vap->iv_stats.is_rx_tkipmic
				  + vap->iv_stats.is_rx_tkipicv;

	vap->iv_stats.is_tx_nodefkey = 0;
	vap->iv_stats.is_tx_noheadroom = 0;
	vap->iv_stats.is_crypto_enmicfail = 0;

	vap->iv_stats.is_tx_nobuf = 0;
	vap->iv_stats.is_tx_nonode = 0;
	vap->iv_stats.is_tx_unknownmgt = 0;
	vap->iv_stats.is_tx_badcipher = 0;
	vap->iv_stats.is_tx_nodefkey = 0;

	vap->iv_stats.is_rx_tooshort = 0;
	vap->iv_stats.is_rx_wepfail = 0;
	vap->iv_stats.is_rx_decap = 0;
	vap->iv_stats.is_rx_nobuf = 0;
	vap->iv_stats.is_rx_decryptcrc = 0;
	vap->iv_stats.is_rx_ccmpmic = 0;
	vap->iv_stats.is_rx_tkipmic = 0;
	vap->iv_stats.is_rx_tkipicv = 0;

	/* XXX total guess as to what to count where */
	/* update according to private statistics */
	stats->tx_errors += extra_tx_errors;
	stats->tx_dropped += extra_tx_dropped;
	stats->rx_errors += extra_rx_errors;
	stats->rx_crc_errors = 0;

	return stats;
}

static int
ieee80211_change_mtu(struct net_device *dev, int mtu)
{
	if (!(IEEE80211_MTU_MIN < mtu && mtu <= IEEE80211_MTU_MAX))
		return -EINVAL;
	dev->mtu = mtu;
	/* XXX coordinate with parent device */
	return 0;
}

static void
ieee80211_set_multicast_list(struct net_device *dev)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;

	IEEE80211_LOCK_IRQ(ic);
	if (dev->flags & IFF_PROMISC) {
		if ((vap->iv_flags & IEEE80211_F_PROMISC) == 0) {
			vap->iv_flags |= IEEE80211_F_PROMISC;
			ic->ic_promisc++;
		}
	} else {
		if (vap->iv_flags & IEEE80211_F_PROMISC) {
			vap->iv_flags &= ~IEEE80211_F_PROMISC;
			ic->ic_promisc--;
		}
	}
	if (dev->flags & IFF_ALLMULTI) {
		if ((vap->iv_flags & IEEE80211_F_ALLMULTI) == 0) {
			vap->iv_flags |= IEEE80211_F_ALLMULTI;
			ic->ic_allmulti++;
		}
	} else {
		if (vap->iv_flags & IEEE80211_F_ALLMULTI) {
			vap->iv_flags &= ~IEEE80211_F_ALLMULTI;
			ic->ic_allmulti--;
		}
	}
	IEEE80211_UNLOCK_IRQ(ic);

}

/*
 * 0: OK
 * <0: NOK, with value derived from errno vals.
 */

#define	N(a)	(sizeof (a) / sizeof (a[0]))

int
ieee80211_country_string_to_countryid( const char *input_str, u_int16_t *p_iso_code )
{
	int	retval = -EINVAL;
	int	iter;

	if (strnlen( input_str, 3 ) >= 3) {
		return( -E2BIG );
	}

	for (iter = 0; iter < N(country_strings) && retval < 0; iter++) {
		if (strcasecmp( country_strings[iter].iso_name, input_str ) == 0) {
			*p_iso_code = country_strings[iter].iso_code;
			retval = 0;
		}
	}

	return( retval );
}

int
ieee80211_countryid_to_country_string( const u_int16_t iso_code, char *output_str )
{
	int	retval = -EINVAL;
	int	iter;

	for (iter = 0; iter < N(country_strings) && retval < 0; iter++) {
		if (iso_code == country_strings[iter].iso_code) {
			strncpy( output_str, country_strings[iter].iso_name, 2 );
			output_str[ 2 ] = '\0';
			retval = 0;
		}
	}

	return( retval );
}

int
ieee80211_region_to_operating_class(struct ieee80211com *ic, char *region_str)
{
	int retval = -EINVAL;
	int i;
	int j;

	for (i = 0; i < ARRAY_SIZE(oper_class_table); i++) {
		if (strcasecmp(oper_class_table[i].region_name, region_str) == 0) {
			for (j = 0; j < oper_class_table[i].class_num_5g; j++)
				setbit(ic->ic_oper_class, oper_class_table[i].classes_5g[j]);

			if (ic->ic_rf_chipid == CHIPID_DUAL) {
				for (j = 0; j < oper_class_table[i].class_num_24g; j++)
					setbit(ic->ic_oper_class, oper_class_table[i].classes_24g[j]);
			}

			ic->ic_oper_class_table = &oper_class_table[i];

			retval = 0;
			break;
		}
	}

	if (retval < 0) {
		for (j = 0; j < oper_class_table[OPER_CLASS_GB_INDEX].class_num_5g; j++)
				setbit(ic->ic_oper_class, oper_class_table[OPER_CLASS_GB_INDEX].classes_5g[j]);

		if (ic->ic_rf_chipid == CHIPID_DUAL) {
			for (j = 0; j < oper_class_table[OPER_CLASS_GB_INDEX].class_num_24g; j++)
				setbit(ic->ic_oper_class, oper_class_table[OPER_CLASS_GB_INDEX].classes_24g[j]);
		}

		ic->ic_oper_class_table = &oper_class_table[OPER_CLASS_GB_INDEX];

		retval = 0;
	}

	return retval;
}

void
ieee80211_get_prichan_list_by_operating_class(struct ieee80211com *ic,
			int bw,
			uint8_t *chan_list,
			uint32_t flag)
{
	int i;
	int j;
	uint32_t table_size = ic->ic_oper_class_table->class_num_5g +
				ic->ic_oper_class_table->class_num_24g;

	KASSERT(ic->ic_oper_class_table, ("Uninitialized operating table"));

	for (i = 0; i < table_size; i++) {
		if (ic->ic_oper_class_table->class_table[i].bandwidth == bw &&
				(ic->ic_oper_class_table->class_table[i].behavior & flag)) {
			for (j = 0; j < ARRAY_SIZE(ic->ic_oper_class_table->class_table[i].chan_set) &&
					ic->ic_oper_class_table->class_table[i].chan_set[j]; j++) {
				setbit(chan_list, ic->ic_oper_class_table->class_table[i].chan_set[j]);
			}
		}
	}
}

int
ieee80211_get_current_operating_class(uint16_t iso_code, int chan, int bw)
{
	int i;
	int j;

	switch (iso_code) {
	case CTRY_UNITED_STATES:
		for (i = 0; i < ARRAY_SIZE(us_oper_class_table); i++) {
			if (us_oper_class_table[i].bandwidth == bw) {
				for (j = 0; j < sizeof(us_oper_class_table[i].chan_set); j++) {
					if (us_oper_class_table[i].chan_set[j] == chan)
						return us_oper_class_table[i].index;
				}
			}
		}
		break;
	case CTRY_EUROPE:
		for (i = 0; i < ARRAY_SIZE(eu_oper_class_table); i++) {
			if (eu_oper_class_table[i].bandwidth == bw) {
				for (j = 0; j < sizeof(eu_oper_class_table[i].chan_set); j++) {
					if (eu_oper_class_table[i].chan_set[j] == chan)
						return eu_oper_class_table[i].index;
				}
			}
		}
		break;
	case CTRY_JAPAN:
		for (i = 0; i < ARRAY_SIZE(jp_oper_class_table); i++) {
			if (jp_oper_class_table[i].bandwidth == bw) {
				for (j = 0; j < sizeof(jp_oper_class_table[i].chan_set); j++) {
					if (jp_oper_class_table[i].chan_set[j] == chan)
						return jp_oper_class_table[i].index;
				}
			}
		}
		break;
	default:
		for (i = 0; i < ARRAY_SIZE(gb_oper_class_table); i++) {
			if (gb_oper_class_table[i].bandwidth == bw) {
				for (j = 0; j < sizeof(gb_oper_class_table[i].chan_set); j++) {
					if (gb_oper_class_table[i].chan_set[j] == chan)
						return gb_oper_class_table[i].index;
				}
			}
		}
		break;
	}

	return 0;
}

void
ieee80211_build_countryie(struct ieee80211com *ic)
{
	int i, j, chanflags, found;
	struct ieee80211_channel *c;
	u_int8_t chanlist[IEEE80211_CHAN_MAX + 1];
	u_int8_t chancnt = 0;
	u_int8_t *cur_runlen, *cur_chan, *cur_pow, prevchan;

	/*
	 * Fill in country IE.
	 */
	memset(&ic->ic_country_ie, 0, sizeof(ic->ic_country_ie));
	ic->ic_country_ie.country_id = IEEE80211_ELEMID_COUNTRY;
	ic->ic_country_ie.country_len = 0; /* init needed by following code */

	/* initialize country IE */
	found = -EINVAL;
	if (ic->ic_spec_country_code != CTRY_DEFAULT) {
		found = ieee80211_countryid_to_country_string(ic->ic_spec_country_code,
					(char *)ic->ic_country_ie.country_str);
	} else {
		found = ieee80211_countryid_to_country_string(ic->ic_country_code,
					(char *)ic->ic_country_ie.country_str);
	}
	if (found < 0) {
		printk("bad country string ignored: %d\n",
			ic->ic_country_code);
		ic->ic_country_ie.country_str[0] = ' ';
		ic->ic_country_ie.country_str[1] = ' ';
	}

	/*
	 * indoor/outdoor portion in country string.
	 * It should be one of:
	 *     'I' indoor only
	 *     'O' outdoor only
	 *     ' ' all enviroments
	 *  Default: we currently support both indoor and outdoor.
	 *  If we need support other options later,
	 *  use 'ic->ic_country_outdoor' to control it.
	 */
	ic->ic_country_ie.country_str[2] = ' ';

	ic->ic_country_ie.country_len += 3;	/* Country string - 3 characters added in */

	/*
	 * runlength encoded channel max tx power info.
	 */
	cur_runlen = &ic->ic_country_ie.country_triplet[1];
	cur_chan = &ic->ic_country_ie.country_triplet[0];
	cur_pow = &ic->ic_country_ie.country_triplet[2];
	prevchan = 0;

	if ((ic->ic_flags_ext & IEEE80211_FEXT_REGCLASS) && ic->ic_nregclass) {
		/*
		 * Add regulatory triplets.
		 * chan/no_of_chans/tx power triplet is overridden as
		 * as follows:
		 * cur_chan == REGULATORY EXTENSION ID.
		 * cur_runlen = Regulatory class.
		 * cur_pow = coverage class.
		 */
		for (i=0; i < ic->ic_nregclass; i++) {
			*cur_chan = IEEE80211_REG_EXT_ID;
			*cur_runlen = ic->ic_regclassids[i];
			*cur_pow = ic->ic_coverageclass;

			cur_runlen +=3;
			cur_chan += 3;
			cur_pow += 3;
			ic->ic_country_ie.country_len += 3;
		}
	} else if (ic->ic_bsschan != IEEE80211_CHAN_ANYC) {
		if (IEEE80211_IS_CHAN_5GHZ(ic->ic_bsschan))
			chanflags = IEEE80211_CHAN_5GHZ;
		else
			chanflags = IEEE80211_CHAN_2GHZ;

		memset(&chanlist[0], 0, sizeof(chanlist));
		/* XXX not right due to duplicate entries */
		for (i = 0; i < ic->ic_nchans; i++) {
			c = &ic->ic_channels[i];

			if (c == NULL || isclr(ic->ic_chan_active, c->ic_ieee))
				continue;

			/* Does channel belong to current operation mode */
			if (!(c->ic_flags & chanflags))
				continue;

			/* Skip previously reported channels */
			for (j = 0; j < chancnt; j++)
				if (c->ic_ieee == chanlist[j])
					break;
			
			if (j != chancnt) /* found a match */
				continue;

			chanlist[chancnt] = c->ic_ieee;
			chancnt++;
	
			/* Skip turbo channels */
			if (IEEE80211_IS_CHAN_TURBO(c))
				continue;
	
			/* Skip half/quarter rate channels */
			if (IEEE80211_IS_CHAN_HALF(c) || 
			    IEEE80211_IS_CHAN_QUARTER(c))
				continue;
	
			if (*cur_runlen == 0) {
				(*cur_runlen)++;
				*cur_pow = c->ic_maxregpower;
				*cur_chan = c->ic_ieee;
				prevchan = c->ic_ieee;
				ic->ic_country_ie.country_len += 3;
			} else if (*cur_pow == c->ic_maxregpower &&
			    c->ic_ieee == prevchan + 1) {
				(*cur_runlen)++;
				prevchan = c->ic_ieee;
			} else {
				cur_runlen +=3;
				cur_chan += 3;
				cur_pow += 3;
				(*cur_runlen)++;
				*cur_pow = c->ic_maxregpower;
				*cur_chan = c->ic_ieee;
				prevchan = c->ic_ieee;
				ic->ic_country_ie.country_len += 3;
			}
		}
	}

	/* pad */
	if (ic->ic_country_ie.country_len & 1)
		ic->ic_country_ie.country_len++;

#undef N
}

u_int
ieee80211_get_chanflags(enum ieee80211_phymode mode)
{
	KASSERT(mode < ARRAY_SIZE(ieee80211_chanflags), ("Unexpected mode %u", mode));
	return ieee80211_chanflags[mode];
}
EXPORT_SYMBOL(ieee80211_get_chanflags);

/*
 * Change the WDS mode of a specified WDS VAP, called in following circumstances:
 * WDS VAP initialization
 * WDS Extender Role changing
 */
int
ieee80211_vap_wds_mode_change(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;

	switch (ic->ic_extender_role) {
	case IEEE80211_EXTENDER_ROLE_MBS:
		IEEE80211_VAP_WDS_SET_MBS(vap);
		break;
	case IEEE80211_EXTENDER_ROLE_RBS:
		IEEE80211_VAP_WDS_SET_RBS(vap);
		break;
	case IEEE80211_EXTENDER_ROLE_NONE:
		IEEE80211_VAP_WDS_SET_NONE(vap);
	default:
		break;
	}

	return 0;
}

int
ieee80211_dual_sec_chan_supported(struct ieee80211vap *vap, int chan)
{
	struct ieee80211com *ic = vap->iv_ic;
	int max_chan;

	if (ic->ic_country_code == CTRY_UNITED_STATES)
		max_chan = IEEE80211_MAX_DUAL_EXT_CHAN_24G_US;
	else
		max_chan = IEEE80211_MAX_DUAL_EXT_CHAN_24G;

	if ((chan >= IEEE80211_MIN_DUAL_EXT_CHAN_24G) &&
			(chan <= max_chan))
		return 1;

	return 0;
}

void
ieee80211_update_sec_chan_offset(struct ieee80211_channel *chan, int offset)
{
	if (offset == IEEE80211_HTINFO_CHOFF_SCA) {
		chan->ic_flags |= IEEE80211_CHAN_HT40U;
		chan->ic_flags &= ~IEEE80211_CHAN_HT40D;
		chan->ic_center_f_40MHz = chan->ic_ieee + IEEE80211_40M_CENT_FREQ_OFFSET;
	} else if (offset == IEEE80211_HTINFO_CHOFF_SCB) {
		chan->ic_flags |= IEEE80211_CHAN_HT40D;
		chan->ic_flags &= ~IEEE80211_CHAN_HT40U;
		chan->ic_center_f_40MHz = chan->ic_ieee - IEEE80211_40M_CENT_FREQ_OFFSET;
	}
}

int
ieee80211_get_max_ap_bw(const struct ieee80211_scan_entry *se)
{
	struct ieee80211_ie_htcap *htcap =
		(struct ieee80211_ie_htcap *)se->se_htcap_ie;
	struct ieee80211_ie_htinfo *htinfo =
		(struct ieee80211_ie_htinfo *)se->se_htinfo_ie;
	struct ieee80211_ie_vhtcap *vhtcap =
		(struct ieee80211_ie_vhtcap *)se->se_vhtcap_ie;
	struct ieee80211_ie_vhtop *vhtop =
		(struct ieee80211_ie_vhtop *)se->se_vhtop_ie;
	int max_bw = BW_HT20;

	if (htinfo) {
		if (htinfo->hi_byte1 & IEEE80211_HTINFO_B1_REC_TXCHWIDTH_40)
			max_bw = BW_HT40;
	} else if (htcap) {
		if (htcap->hc_cap[0] & IEEE80211_HTCAP_C_CHWIDTH40)
			max_bw = BW_HT40;
	}

	if (vhtop) {
		int vhtop_bw = IEEE80211_VHTOP_GET_CHANWIDTH(vhtop);
		if ((vhtop_bw == IEEE80211_VHTOP_CHAN_WIDTH_160MHZ) ||
				(vhtop_bw == IEEE80211_VHTOP_CHAN_WIDTH_80PLUS80MHZ))
			max_bw = BW_HT160;
		else if (vhtop_bw == IEEE80211_VHTOP_CHAN_WIDTH_80MHZ)
			max_bw = BW_HT80;
	} else if (vhtcap) {
		int vhtcap_bw = IEEE80211_VHTCAP_GET_CHANWIDTH(vhtcap);
		if (vhtcap_bw == IEEE80211_VHTCAP_CW_80M_ONLY)
			max_bw = BW_HT80;
		else if ((vhtcap_bw == IEEE80211_VHTCAP_CW_160M) ||
				(vhtcap_bw == IEEE80211_VHTCAP_CW_160_AND_80P80M))
			max_bw = BW_HT160;
	}

	return max_bw;
}

int
ieee80211_get_max_node_bw(struct ieee80211_node *ni)
{
	int max_bw = BW_HT20;

	if (IEEE80211_NODE_IS_HT(ni)) {
		if (ni->ni_htcap.cap & IEEE80211_HTCAP_C_CHWIDTH40)
			max_bw = BW_HT40;
	}

	if (IEEE80211_NODE_IS_VHT(ni)) {
		if (ni->ni_vhtcap.chanwidth == IEEE80211_VHTCAP_CW_80M_ONLY)
			max_bw = BW_HT80;
		else if ((ni->ni_vhtcap.chanwidth == IEEE80211_VHTCAP_CW_160M) ||
				(ni->ni_vhtcap.chanwidth == IEEE80211_VHTCAP_CW_160_AND_80P80M))
			max_bw = BW_HT160;
	}

	return max_bw;
}

int
ieee80211_get_max_system_bw(struct ieee80211com *ic)
{
	int max_bw = ic->ic_max_system_bw;

	if (max_bw >= BW_HT80) {
		if (!IS_IEEE80211_VHT_ENABLED(ic) ||
				!ieee80211_swfeat_is_supported(SWFEAT_ID_VHT, 1))
			max_bw = BW_HT40;
	}

	if (max_bw >= BW_HT40) {
		if (ic->ic_curmode <= IEEE80211_MODE_11NG)
			max_bw = BW_HT20;
	}

	return max_bw;
}

int
ieee80211_get_max_channel_bw(struct ieee80211com *ic, int channel)
{
	if (isset(ic->ic_chan_active_80, channel))
		return BW_HT80;
	else if (isset(ic->ic_chan_active_40, channel))
		return BW_HT40;
	else
		return BW_HT20;
}

int
ieee80211_get_max_bw(struct ieee80211vap *vap,
		struct ieee80211_node *ni, uint32_t chan)
{
	struct ieee80211com *ic = vap->iv_ic;
	int max_bw = ieee80211_get_max_node_bw(ni);
	int ic_bw = ieee80211_get_max_system_bw(ic);
	int chan_bw = ieee80211_get_max_channel_bw(ic, chan);

	max_bw = MIN(max_bw, ic_bw);
	max_bw = MIN(max_bw, chan_bw);

	return max_bw;
}

