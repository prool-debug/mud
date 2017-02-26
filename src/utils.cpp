/**************************************************************************
*   File: utils.cpp                                     Part of Bylins    *
*  Usage: various internal functions of a utility nature                  *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
* 									  *
*  $Author$                                                        *
*  $Date$                                           *
*  $Revision$                                                      *
************************************************************************ */

#include "utils.h"

#include "object.prototypes.hpp"
#include "logger.hpp"
#include "obj.hpp"
#include "db.h"
#include "comm.h"
#include "screen.h"
#include "spells.h"
#include "handler.h"
#include "interpreter.h"
#include "constants.h"
#include "im.h"
#include "dg_scripts.h"
#include "features.hpp"
#include "privilege.hpp"
#include "char.hpp"
#include "room.hpp"
#include "modify.h"
#include "house.h"
#include "player_races.hpp"
#include "depot.hpp"
#include "objsave.h"
#include "fight.h"
#include "skills.h"
#include "exchange.h"
#include "sets_drop.hpp"
#include "structs.h"
#include "sysdep.h"
#include "conf.h"
#include "obj_sets.hpp"

#ifdef HAVE_ICONV
#include <iconv.h>
#endif

#include <boost/bind.hpp>
#include <boost/algorithm/string/trim.hpp>

#include <vector>
#include <string>
#include <fstream>
#include <cmath>
#include <iomanip>
#include <algorithm>
#include <sstream>

extern DESCRIPTOR_DATA *descriptor_list;
extern CHAR_DATA *mob_proto;
extern const char *weapon_class[];
// local functions
TIME_INFO_DATA *real_time_passed(time_t t2, time_t t1);
TIME_INFO_DATA *mud_time_passed(time_t t2, time_t t1);
void prune_crlf(char *txt);
int valid_email(const char *address);

// external functions
int attack_best(CHAR_DATA * ch, CHAR_DATA * victim);
void perform_drop_gold(CHAR_DATA * ch, int amount, byte mode, room_rnum RDR);

char AltToKoi[] =
{
	"�����������������������������������������������А�����+++����+�������������������++���++�����������������������ѳ�����������??��"
};
char KoiToAlt[] =
{
	"ĳڿ��ô�������ް��+�+�+++�+++�+ͺ���������Ⱦ���̵������������+椥�娩����������㦢������Ꞁ������������������������������"
};
char WinToKoi[] =
{
	"++++++++++++++++++++++++++++++++�++++�++���++++��+���++��+�++++�����������������������������������������������������������������"
};
char KoiToWin[] =
{
	"++++++++++++++++++++++++++�+�+�++++��+��+++++�+++++��+��+++++�+�����������������������������������������������������������������"
};
char KoiToWin2[] =
{
	"++++++++++++++++++++++++++�+�+�++++��+��+++++�+++++��+��+++++�+������������������z����������������������������������������������"
};
char AltToLat[] =
{
	"����������������������������������������������������������������0abcdefghijklmnopqrstY1v23z456780ABCDEFGHIJKLMNOPQRSTY1V23Z45678"
};

const char *ACTNULL = "<NULL>";

// return char with UID n
CHAR_DATA *find_char(long n)
{
	for (CHAR_DATA* ch = character_list; ch; ch = ch->get_next())
	{
		if (GET_ID(ch) == n)
		{
			return ch;
		}
	}

	return nullptr;
}

bool check_spell_on_player(CHAR_DATA *ch, int spell_num)
{
	for (const auto af : ch->affected)
	{
		if (af->type == spell_num)
		{
			return true;
		}
	}

	return false;
}

int MIN(int a, int b)
{
	return (a < b ? a : b);
}

int MAX(int a, int b)
{
	return (a > b ? a : b);
}

char * CAP(char *txt)
{
	*txt = UPPER(*txt);
	return (txt);
}

// Create and append to dynamic length string - Alez
char *str_add(char *dst, const char *src)
{
	if (dst == NULL)
	{
		dst = (char *) malloc(strlen(src) + 1);
		strcpy(dst, src);
	}
	else
	{
		dst = (char *) realloc(dst, strlen(dst) + strlen(src) + 1);
		strcat(dst, src);
	};
	return dst;
}

// Create a duplicate of a string
char *str_dup(const char *source)
{
	char *new_z = NULL;
	if (source)
	{
		CREATE(new_z, strlen(source) + 1);
		return (strcpy(new_z, source));
	}
	CREATE(new_z, 1);
	return (strcpy(new_z, ""));
}

// * Strips \r\n from end of string.
void prune_crlf(char *txt)
{
	size_t i = strlen(txt) - 1;

	while (txt[i] == '\n' || txt[i] == '\r')
	{
		txt[i--] = '\0';
	}
}

bool is_head(std::string name)
{
	if ((name == "�������") || (name == "�������"))
		return true;
	return false;
}

int get_virtual_race(CHAR_DATA *mob)
{
	if (mob->get_role(MOB_ROLE_BOSS))
	{
		return NPC_BOSS;
	}
	std::map<int, int>::iterator it;
	std::map<int, int> unique_mobs = SetsDrop::get_unique_mob();
	for (it = unique_mobs.begin(); it != unique_mobs.end(); it++)
	{
		if (GET_MOB_VNUM(mob) == it->first)
			return NPC_UNIQUE;
	}
	return -1;
}

/*
 * str_cmp: a case-insensitive version of strcmp().
 * Returns: 0 if equal, > 0 if arg1 > arg2, or < 0 if arg1 < arg2.
 *
 * Scan until strings are found different or we reach the end of both.
 */
int str_cmp(const char *arg1, const char *arg2)
{
	int chk, i;

	if (arg1 == NULL || arg2 == NULL)
	{
		log("SYSERR: str_cmp() passed a NULL pointer, %p or %p.", arg1, arg2);
		return (0);
	}

	for (i = 0; arg1[i] || arg2[i]; i++)
		if ((chk = LOWER(arg1[i]) - LOWER(arg2[i])) != 0)
			return (chk);	// not equal

	return (0);
}

int str_cmp(const std::string &arg1, const char *arg2)
{
	int chk;
	std::string::size_type i;

	if (arg2 == NULL)
	{
		log("SYSERR: str_cmp() passed a NULL pointer, %p.", arg2);
		return (0);
	}

	for (i = 0; i != arg1.length() && *arg2; i++, arg2++)
		if ((chk = LOWER(arg1[i]) - LOWER(*arg2)) != 0)
			return (chk);	// not equal

	if (i == arg1.length() && !*arg2)
		return (0);

	if (*arg2)
		return (LOWER('\0') - LOWER(*arg2));
	else
		return (LOWER(arg1[i]) - LOWER('\0'));
}
int str_cmp(const char *arg1, const std::string &arg2)
{
	int chk;
	std::string::size_type i;

	if (arg1 == NULL)
	{
		log("SYSERR: str_cmp() passed a NULL pointer, %p.", arg1);
		return (0);
	}

	for (i = 0; *arg1 && i != arg2.length(); i++, arg1++)
		if ((chk = LOWER(*arg1) - LOWER(arg2[i])) != 0)
			return (chk);	// not equal

	if (!*arg1 && i == arg2.length())
		return (0);

	if (*arg1)
		return (LOWER(*arg1) - LOWER('\0'));
	else
		return (LOWER('\0') - LOWER(arg2[i]));
}
int str_cmp(const std::string &arg1, const std::string &arg2)
{
	int chk;
	std::string::size_type i;

	for (i = 0; i != arg1.length() && i != arg2.length(); i++)
		if ((chk = LOWER(arg1[i]) - LOWER(arg2[i])) != 0)
			return (chk);	// not equal

	if (arg1.length() == arg2.length())
		return (0);

	if (i == arg1.length())
		return (LOWER('\0') - LOWER(arg2[i]));
	else
		return (LOWER(arg1[i]) - LOWER('\0'));
}


/*
 * strn_cmp: a case-insensitive version of strncmp().
 * Returns: 0 if equal, > 0 if arg1 > arg2, or < 0 if arg1 < arg2.
 *
 * Scan until strings are found different, the end of both, or n is reached.
 */
int strn_cmp(const char *arg1, const char *arg2, size_t n)
{
	int chk, i;

	if (arg1 == NULL || arg2 == NULL)
	{
		log("SYSERR: strn_cmp() passed a NULL pointer, %p or %p.", arg1, arg2);
		return (0);
	}

	for (i = 0; (arg1[i] || arg2[i]) && (n > 0); i++, n--)
		if ((chk = LOWER(arg1[i]) - LOWER(arg2[i])) != 0)
			return (chk);	// not equal

	return (0);
}

int strn_cmp(const std::string &arg1, const char *arg2, size_t n)
{
	int chk;
	std::string::size_type i;

	if (arg2 == NULL)
	{
		log("SYSERR: strn_cmp() passed a NULL pointer, %p.", arg2);
		return (0);
	}

	for (i = 0; i != arg1.length() && *arg2 && (n > 0); i++, arg2++, n--)
		if ((chk = LOWER(arg1[i]) - LOWER(*arg2)) != 0)
			return (chk);	// not equal

	if (i == arg1.length() && (!*arg2 || n == 0))
		return (0);

	if (*arg2)
		return (LOWER('\0') - LOWER(*arg2));
	else
		return (LOWER(arg1[i]) - LOWER('\0'));
}

int strn_cmp(const char *arg1, const std::string &arg2, size_t n)
{
	int chk;
	std::string::size_type i;

	if (arg1 == NULL)
	{
		log("SYSERR: strn_cmp() passed a NULL pointer, %p.", arg1);
		return (0);
	}

	for (i = 0; *arg1 && i != arg2.length() && (n > 0); i++, arg1++, n--)
		if ((chk = LOWER(*arg1) - LOWER(arg2[i])) != 0)
			return (chk);	// not equal

	if (!*arg1 && (i == arg2.length() || n == 0))
		return (0);

	if (*arg1)
		return (LOWER(*arg1) - LOWER('\0'));
	else
		return (LOWER('\0') - LOWER(arg2[i]));
}

int strn_cmp(const std::string &arg1, const std::string &arg2, size_t n)
{
	int chk;
	std::string::size_type i;

	for (i = 0; i != arg1.length() && i != arg2.length() && (n > 0); i++, n--)
		if ((chk = LOWER(arg1[i]) - LOWER(arg2[i])) != 0)
			return (chk);	// not equal

	if (arg1.length() == arg2.length() || (n == 0))
		return (0);

	if (i == arg1.length())
		return (LOWER('\0') - LOWER(arg2[i]));
	else
		return (LOWER(arg1[i]) - LOWER('\0'));
}

// ����������� �������� ������ ����� ��� ������ ������ ��� �����
std::list<FILE *> opened_files;

bool no_bad_affects(OBJ_DATA *obj)
{
	static std::list<EWeaponAffectFlag> bad_waffects =
	{
		EWeaponAffectFlag::WAFF_HOLD,
		EWeaponAffectFlag::WAFF_SANCTUARY,
		EWeaponAffectFlag::WAFF_PRISMATIC_AURA,
		EWeaponAffectFlag::WAFF_POISON,
		EWeaponAffectFlag::WAFF_SILENCE,
		EWeaponAffectFlag::WAFF_DEAFNESS,
		EWeaponAffectFlag::WAFF_HAEMORRAGIA,
		EWeaponAffectFlag::WAFF_BLINDNESS,
		EWeaponAffectFlag::WAFF_SLEEP,
		EWeaponAffectFlag::WAFF_HOLY_DARK
	};
	for (const auto wa : bad_waffects)
	{
		if (OBJ_AFFECT(obj, wa))
		{
			return false;
		}
	}
	return true;
}

/**
* ���� ������������� ���� ���� ����������� ���� ��� �� ������ ���� ������ � ����.
* ���������� ��������� ����� � ������ �� � �������� (��������� ��� con_close).
*/
void pers_log(CHAR_DATA *ch, const char *format, ...)
{
	if (!ch)
	{
		log("NULL character resieved! (%s %s %d)", __FILE__, __func__, __LINE__);
		return;
	}

	if (!format)
	{
		format = "SYSERR: pers_log received a NULL format.";
	}

	if (!ch->desc->pers_log)
	{
		char filename[128], name[64], *ptr;
		strcpy(name, GET_NAME(ch));
		for (ptr = name; *ptr; ptr++)
		{
			*ptr = LOWER(AtoL(*ptr));
		}
		sprintf(filename, "../log/perslog/%s.log", name);
		ch->desc->pers_log = fopen(filename, "a");
		if (!ch->desc->pers_log)
		{
			log("SYSERR: error open %s (%s %s %d)", filename, __FILE__, __func__, __LINE__);
			return;
		}
		opened_files.push_back(ch->desc->pers_log);
	}

	write_time(ch->desc->pers_log);
	va_list args;
	va_start(args, format);
	vfprintf(ch->desc->pers_log, format, args);
	va_end(args);
	fprintf(ch->desc->pers_log, "\n");
}

// the "touch" command, essentially.
int touch(const char *path)
{
	FILE *fl;

	if (!(fl = fopen(path, "a")))
	{
		log("SYSERR: %s: %s", path, strerror(errno));
		return (-1);
	}
	else
	{
		fclose(fl);
		return (0);
	}
}

void sprinttype(int type, const char *names[], char *result)
{
	int nr = 0;

	while (type && *names[nr] != '\n')
	{
		type--;
		nr++;
	}

	if (*names[nr] != '\n')
		strcpy(result, names[nr]);
	else
		strcpy(result, "UNDEF");
}

// * Calculate the REAL time passed over the last t2-t1 centuries (secs)
TIME_INFO_DATA *real_time_passed(time_t t2, time_t t1)
{
	long secs;
	static TIME_INFO_DATA now;

	secs = (long)(t2 - t1);

	now.hours = (secs / SECS_PER_REAL_HOUR) % 24;	// 0..23 hours //
	secs -= SECS_PER_REAL_HOUR * now.hours;

	now.day = (secs / SECS_PER_REAL_DAY);	// 0..34 days  //
	// secs -= SECS_PER_REAL_DAY * now.day; - Not used. //

	now.month = -1;
	now.year = -1;

	return (&now);
}



// Calculate the MUD time passed over the last t2-t1 centuries (secs) //
TIME_INFO_DATA *mud_time_passed(time_t t2, time_t t1)
{
	long secs;
	static TIME_INFO_DATA now;

	secs = (long)(t2 - t1);

	now.hours = (secs / (SECS_PER_MUD_HOUR * TIME_KOEFF)) % HOURS_PER_DAY;	// 0..23 hours //
	secs -= SECS_PER_MUD_HOUR * TIME_KOEFF * now.hours;

	now.day = (secs / (SECS_PER_MUD_DAY * TIME_KOEFF)) % DAYS_PER_MONTH;	// 0..29 days  //
	secs -= SECS_PER_MUD_DAY * TIME_KOEFF * now.day;

	now.month = (secs / (SECS_PER_MUD_MONTH * TIME_KOEFF)) % MONTHS_PER_YEAR;	// 0..11 months //
	secs -= SECS_PER_MUD_MONTH * TIME_KOEFF * now.month;

	now.year = (secs / (SECS_PER_MUD_YEAR * TIME_KOEFF));	// 0..XX? years //

	return (&now);
}



TIME_INFO_DATA *age(CHAR_DATA * ch)
{
	static TIME_INFO_DATA player_age;

	player_age = *mud_time_passed(time(0), ch->player_data.time.birth);

	player_age.year += 17;	// All players start at 17 //

	return (&player_age);
}

// Check if making CH follow VICTIM will create an illegal //
// Follow "Loop/circle"                                    //
bool circle_follow(CHAR_DATA * ch, CHAR_DATA * victim)
{
	for (auto k = victim; k; k = k->get_master())
	{
		if (k->get_master() == k)
		{
			k->set_master(nullptr);
			return false;
		}

		if (k == ch)
		{
			return true;
		}
	}

	return false;
}

void make_horse(CHAR_DATA * horse, CHAR_DATA * ch)
{
	AFF_FLAGS(horse).set(EAffectFlag::AFF_HORSE);
	ch->add_follower(horse);
	MOB_FLAGS(horse).unset(MOB_WIMPY);
	MOB_FLAGS(horse).unset(MOB_SENTINEL);
	MOB_FLAGS(horse).unset(MOB_HELPER);
	MOB_FLAGS(horse).unset(MOB_AGGRESSIVE);
	MOB_FLAGS(horse).unset(MOB_MOUNTING);
	AFF_FLAGS(horse).unset(EAffectFlag::AFF_TETHERED);
}

int on_horse(CHAR_DATA * ch)
{
	return (AFF_FLAGGED(ch, EAffectFlag::AFF_HORSE) && has_horse(ch, TRUE));
}

int has_horse(CHAR_DATA * ch, int same_room)
{
	struct follow_type *f;

	if (IS_NPC(ch))
		return (FALSE);

	for (f = ch->followers; f; f = f->next)
	{
		if (IS_NPC(f->follower)
			&& AFF_FLAGGED(f->follower, EAffectFlag::AFF_HORSE)
			&& (!same_room
				|| ch->in_room == IN_ROOM(f->follower)))
		{
			return (TRUE);
		}
	}
	return (FALSE);
}

CHAR_DATA *get_horse(CHAR_DATA * ch)
{
	struct follow_type *f;

	if (IS_NPC(ch))
		return (NULL);

	for (f = ch->followers; f; f = f->next)
	{
		if (IS_NPC(f->follower)
			&& AFF_FLAGGED(f->follower, EAffectFlag::AFF_HORSE))
		{
			return (f->follower);
		}
	}
	return (NULL);
}

void horse_drop(CHAR_DATA * ch)
{
	if (ch->has_master())
	{
		act("$N �������$G ��� �� ����� �����.", FALSE, ch->get_master(), 0, ch, TO_CHAR);
		AFF_FLAGS(ch->get_master()).unset(EAffectFlag::AFF_HORSE);
		WAIT_STATE(ch->get_master(), 3 * PULSE_VIOLENCE);

		if (GET_POS(ch->get_master()) > POS_SITTING)
		{
			GET_POS(ch->get_master()) = POS_SITTING;
		}
	}
}

void check_horse(CHAR_DATA * ch)
{
	if (!IS_NPC(ch)
		&& !has_horse(ch, TRUE))
	{
		AFF_FLAGS(ch).unset(EAffectFlag::AFF_HORSE);
	}
}

// Called when stop following persons, or stopping charm //
// This will NOT do if a character quits/dies!!          //
// ��� �������� 1 ������������ ch ������, �.�. ������ ����� extract_char
// TODO: �� ���� ������� �� ��������, ����� ��� ���-�� ������ ����������, ����� �������� �������� -- Krodo
// ��� ��������� �� ����� - ������� �� ������ ��������, ���� ������, ��� ������� � change_leader ����� �����
bool stop_follower(CHAR_DATA * ch, int mode)
{
	CHAR_DATA *master;
	struct follow_type *j, *k;
	int i;

	//log("[Stop follower] Start function(%s->%s)",ch ? GET_NAME(ch) : "none",
	//      ch->master ? GET_NAME(ch->master) : "none");

	if (!ch->has_master())
	{
		log("SYSERR: stop_follower(%s) without master", GET_NAME(ch));
		return (FALSE);
	}

	// ��� ����� ������ ��� ������� �����
	if (!IS_SET(mode, SF_SILENCE))
	{
		act("�� ���������� ��������� �� $N4.", FALSE, ch, 0, ch->get_master(), TO_CHAR);
		act("$n ���������$g ��������� �� $N4.", TRUE, ch, 0, ch->get_master(), TO_NOTVICT | TO_ARENA_LISTEN);
	}

	//log("[Stop follower] Stop horse");
	if (get_horse(ch->get_master()) == ch
		&& on_horse(ch->get_master()))
	{
		horse_drop(ch);
	}
	else
	{
		act("$n ���������$g ��������� �� ����.", TRUE, ch, 0, ch->get_master(), TO_VICT);
	}

	//log("[Stop follower] Remove from followers list");
	if (!ch->get_master()->followers)
	{
		log("[Stop follower] SYSERR: Followers absent for %s (master %s).", GET_NAME(ch), GET_NAME(ch->get_master()));
	}
	else if (ch->get_master()->followers->follower == ch)  	// Head of follower-list?
	{
		k = ch->get_master()->followers;
		ch->get_master()->followers = k->next;
		if (!ch->get_master()->followers
			&& !ch->get_master()->has_master())
		{
			AFF_FLAGS(ch->get_master()).unset(EAffectFlag::AFF_GROUP);
		}
		free(k);
	}
	else  		// locate follower who is not head of list
	{
		for (k = ch->get_master()->followers; k->next && k->next->follower != ch; k = k->next);
		if (!k->next)
		{
			log("[Stop follower] SYSERR: Undefined %s in %s followers list.", GET_NAME(ch), GET_NAME(ch->get_master()));
		}
		else
		{
			j = k->next;
			k->next = j->next;
			free(j);
		}
	}
	master = ch->get_master();
	ch->set_master(nullptr);

	AFF_FLAGS(ch).unset(EAffectFlag::AFF_GROUP);

	if (AFF_FLAGGED(ch, EAffectFlag::AFF_CHARM)
		|| AFF_FLAGGED(ch, EAffectFlag::AFF_HELPER)
		|| IS_SET(mode, SF_CHARMLOST))
	{
		if (affected_by_spell(ch, SPELL_CHARM))
		{
			affect_from_char(ch, SPELL_CHARM);
		}
		EXTRACT_TIMER(ch) = 5;
		AFF_FLAGS(ch).unset(EAffectFlag::AFF_CHARM);

		if (ch->get_fighting())
		{
			stop_fighting(ch, TRUE);
		}

		if (IS_NPC(ch))
		{
			if (MOB_FLAGGED(ch, MOB_CORPSE))
			{
				act("���������� ����� ������� $n3, �� ������� � �����.", TRUE, ch, 0, 0, TO_ROOM | TO_ARENA_LISTEN);
				GET_LASTROOM(ch) = GET_ROOM_VNUM(ch->in_room);
				perform_drop_gold(ch, ch->get_gold(), SCMD_DROP, 0);
				ch->set_gold(0);
				extract_char(ch, FALSE);
				return (TRUE);
			}
			else if (AFF_FLAGGED(ch, EAffectFlag::AFF_HELPER))
			{
				AFF_FLAGS(ch).unset(EAffectFlag::AFF_HELPER);
			}
			else
			{
				if (master &&
						!IS_SET(mode, SF_MASTERDIE) &&
						ch->in_room == IN_ROOM(master) &&
						CAN_SEE(ch, master) && !ch->get_fighting() &&
						!ROOM_FLAGGED(ch->in_room, ROOM_PEACEFUL))   //Polud - �� �� ���� ������ � ������, ������� ���
				{
					if (number(1, GET_REAL_INT(ch) * 2) > GET_REAL_CHA(master))
					{
						act("$n ��������$g, ��� �� ������������ ������!",
							FALSE, ch, 0, master, TO_VICT | CHECK_DEAF);
						act("$n ������$g : \"�� ����� �����$G ���� �� ���, �� ������ ��� �� ������!\""
						    "              \"������ ������ ���� ������ ����� �������� ���� �����!!!\"",
						    TRUE, ch, 0, master, TO_NOTVICT | CHECK_DEAF);
						set_fighting(ch, master);
					}
				}
				else
				{
					if (master
						&& !IS_SET(mode, SF_MASTERDIE)
						&& CAN_SEE(ch, master) && MOB_FLAGGED(ch, MOB_MEMORY))
					{
						remember(ch, master);
					}
				}
			}
		}
	}

	if (IS_NPC(ch)
		&& (i = GET_MOB_RNUM(ch)) >= 0)
	{
		MOB_FLAGS(ch) = MOB_FLAGS(mob_proto + i);
	}

	return (FALSE);
}



// * Called when a character that follows/is followed dies
bool die_follower(CHAR_DATA * ch)
{
	struct follow_type *j, *k = ch->followers;

	if (ch->has_master() && stop_follower(ch, SF_FOLLOWERDIE))
	{
		//  ������� �������� � stop_follower
		return true;
	}

	if (on_horse(ch))
	{
		AFF_FLAGS(ch).unset(EAffectFlag::AFF_HORSE);
	}

	for (k = ch->followers; k; k = j)
	{
		j = k->next;
		stop_follower(k->follower, SF_MASTERDIE);
	}
	return false;
}

/*
 * get_line reads the next non-blank line off of the input stream.
 * The newline character is removed from the input.  Lines which begin
 * with '*' are considered to be comments.
 *
 * Returns the number of lines advanced in the file.
 */
int get_line(FILE * fl, char *buf)
{
	char temp[256];
	int lines = 0;

	do
	{
		const char* dummy = fgets(temp, 256, fl);
		UNUSED_ARG(dummy);

		if (feof(fl))
			return (0);
		lines++;
	}
	while (*temp == '*' || *temp == '\n');

	temp[strlen(temp) - 1] = '\0';
	strcpy(buf, temp);
	return (lines);
}


int get_filename(const char *orig_name, char *filename, int mode)
{
	const char *prefix, *middle, *suffix;
	char name[64], *ptr;

	if (orig_name == NULL || *orig_name == '\0' || filename == NULL)
	{
		log("SYSERR: NULL pointer or empty string passed to get_filename(), %p or %p.", orig_name, filename);
		return (0);
	}

	switch (mode)
	{
	case ALIAS_FILE:
		prefix = LIB_PLRALIAS;
		suffix = SUF_ALIAS;
		break;
	case SCRIPT_VARS_FILE:
		prefix = LIB_PLRVARS;
		suffix = SUF_MEM;
		break;
	case PLAYERS_FILE:
		prefix = LIB_PLRS;
		suffix = SUF_PLAYER;
		break;
	case TEXT_CRASH_FILE:
		prefix = LIB_PLROBJS;
		suffix = TEXT_SUF_OBJS;
		break;
	case TIME_CRASH_FILE:
		prefix = LIB_PLROBJS;
		suffix = TIME_SUF_OBJS;
		break;
	case PERS_DEPOT_FILE:
		prefix = LIB_DEPOT;
		suffix = SUF_PERS_DEPOT;
		break;
	case SHARE_DEPOT_FILE:
		prefix = LIB_DEPOT;
		suffix = SUF_SHARE_DEPOT;
		break;
	case PURGE_DEPOT_FILE:
		prefix = LIB_DEPOT;
		suffix = SUF_PURGE_DEPOT;
		break;
	default:
		return (0);
	}

	strcpy(name, orig_name);
	for (ptr = name; *ptr; ptr++)
	{
		if (*ptr == '�' || *ptr == '�')
			*ptr = '9';
		else
			*ptr = LOWER(AtoL(*ptr));
	}

	switch (LOWER(*name))
	{
	case 'a':
	case 'b':
	case 'c':
	case 'd':
	case 'e':
		middle = LIB_A;
		break;
	case 'f':
	case 'g':
	case 'h':
	case 'i':
	case 'j':
		middle = LIB_F;
		break;
	case 'k':
	case 'l':
	case 'm':
	case 'n':
	case 'o':
		middle = LIB_K;
		break;
	case 'p':
	case 'q':
	case 'r':
	case 's':
	case 't':
		middle = LIB_P;
		break;
	case 'u':
	case 'v':
	case 'w':
	case 'x':
	case 'y':
	case 'z':
		middle = LIB_U;
		break;
	default:
		middle = LIB_Z;
		break;
	}

	sprintf(filename, "%s%s" SLASH "%s.%s", prefix, middle, name, suffix);
	return (1);
}


int num_pc_in_room(ROOM_DATA * room)
{
	int i = 0;
	CHAR_DATA *ch;

	for (ch = room->people; ch != NULL; ch = ch->next_in_room)
		if (!IS_NPC(ch))
			i++;

	return (i);
}

/*
 * This function (derived from basic fork(); abort(); idea by Erwin S.
 * Andreasen) causes your MUD to dump core (assuming you can) but
 * continue running.  The core dump will allow post-mortem debugging
 * that is less severe than assert();  Don't call this directly as
 * core_dump_unix() but as simply 'core_dump()' so that it will be
 * excluded from systems not supporting them. (e.g. Windows '95).
 *
 * You still want to call abort() or exit(1) for
 * non-recoverable errors, of course...
 *
 * XXX: Wonder if flushing streams includes sockets?
 */
void core_dump_real(const char *who, int line)
{
	log("SYSERR: Assertion failed at %s:%d!", who, line);

#if defined(CIRCLE_UNIX)
	// These would be duplicated otherwise...
	fflush(stdout);
	fflush(stderr);
	for (int i = 0; i < 1 + LAST_LOG; ++i)
	{
		fflush(runtime_config.logs(static_cast<EOutputStream>(i)).handle());
	}

	/*
	 * Kill the child so the debugger or script doesn't think the MUD
	 * crashed.  The 'autorun' script would otherwise run it again.
	 */
	if (fork() == 0)
		abort();
#endif
}

void to_koi(char *str, int from)
{
	switch (from)
	{
	case KT_ALT:
		for (; *str; *str = AtoK(*str), str++);
		break;
	case KT_WINZ:
	case KT_WIN:
		for (; *str; *str = WtoK(*str), str++);
		break;
	}
}

void from_koi(char *str, int to)
{
	switch (to)
	{
	case KT_ALT:
		for (; *str; *str = KtoA(*str), str++);
		break;
	case KT_WIN:
		for (; *str; *str = KtoW(*str), str++);
	case KT_WINZ:
		for (; *str; *str = KtoW2(*str), str++);
		break;
	}
}

void koi_to_win(char *str, int size)
{
	for (; size > 0; *str = KtoW(*str), size--, str++);
}

void koi_to_winz(char *str, int size)
{
	for (; size > 0; *str = KtoW2(*str), size--, str++);
}

void koi_to_alt(char *str, int size)
{
	for (; size > 0; *str = KtoA(*str), size--, str++);
}

// string manipulation fucntion originally by Darren Wilson //
// (wilson@shark.cc.cc.ca.us) improved and bug fixed by Chris (zero@cnw.com) //
// completely re-written again by M. Scott 10/15/96 (scottm@workcommn.net), //
// completely rewritten by Anton Gorev 05/08/2016 (kvirund@gmail.com) //
// substitute appearances of 'pattern' with 'replacement' in string //
// and return the # of replacements //
int replace_str(const AbstractStringWriter::shared_ptr& writer, const char *pattern, const char *replacement, int rep_all, int max_size)
{
	char *replace_buffer = nullptr;
	CREATE(replace_buffer, max_size);
	std::shared_ptr<char> guard(replace_buffer, free);
	size_t source_remained = writer->length();
	const size_t pattern_length = strlen(pattern);
	const size_t replacement_length = strlen(replacement);

	int count = 0;
	const char* from = writer->get_string();
	size_t remains = max_size;
	do
	{
		if (remains < source_remained)
		{
			return -1;	// destination does not have enough space.
		}

		const char* pos = strstr(from, pattern);
		if (nullptr != pos)
		{
			if (remains < source_remained - pattern_length + replacement_length)
			{
				return -1;	// destination does not have enough space.
			}

			strncpy(replace_buffer, from, pos - from);
			replace_buffer += pos - from;

			strncpy(replace_buffer, replacement, replacement_length);
			replace_buffer += replacement_length;
			remains -= replacement_length;

			const size_t processed = pos - from + pattern_length;
			source_remained -= processed;
			from += processed;

			++count;
		}
		else
		{
			strncpy(replace_buffer, from, source_remained);
			replace_buffer += source_remained;
			source_remained = 0;
		}
	} while (0 != rep_all && 0 < source_remained);

	if (count == 0)
	{
		return 0;
	}

	if (0 < source_remained)
	{
		strncpy(replace_buffer, from, std::min(remains, source_remained));
	}
	writer->set_string(guard.get());

	return count;
}

// re-formats message type formatted char * //
// (for strings edited with d->str) (mostly olc and mail)     //
void format_text(const AbstractStringWriter::shared_ptr& writer, int mode, DESCRIPTOR_DATA* /*d*/, size_t maxlen)
{
	size_t total_chars = 0;
	int cap_next = TRUE, cap_next_next = FALSE;
	const char* flow;
	const char* start = NULL;
	// warning: do not edit messages with max_str's of over this value //
	char formatted[MAX_STRING_LENGTH];
	char *pos = formatted;

	flow = writer->get_string();
	if (!flow)
	{
		return;
	}

	if (IS_SET(mode, FORMAT_INDENT))
	{
		strcpy(pos, "   ");
		total_chars = 3;
		pos += 3;
	}
	else
	{
		*pos = '\0';
		total_chars = 0;
	}

	while (*flow != '\0')
	{
		while ((*flow == '\n')
			|| (*flow == '\r')
			|| (*flow == '\f')
			|| (*flow == '\t')
			|| (*flow == '\v')
			|| (*flow == ' '))
		{
			flow++;
		}

		if (*flow != '\0')
		{
			start = flow++;
			while ((*flow != '\0')
				&& (*flow != '\n')
				&& (*flow != '\r')
				&& (*flow != '\f')
				&& (*flow != '\t')
				&& (*flow != '\v')
				&& (*flow != ' ')
				&& (*flow != '.')
				&& (*flow != '?')
				&& (*flow != '!'))
			{
				flow++;
			}

			if (cap_next_next)
			{
				cap_next_next = FALSE;
				cap_next = TRUE;
			}

			// this is so that if we stopped on a sentance .. we move off the sentance delim. //
			while ((*flow == '.') || (*flow == '!') || (*flow == '?'))
			{
				cap_next_next = TRUE;
				flow++;
			}

			if ((total_chars + (flow - start) + 1) > 79)
			{
				strcpy(pos, "\r\n");
				total_chars = 0;
				pos += 2;
			}

			if (!cap_next)
			{
				if (total_chars > 0)
				{
					strcpy(pos, " ");
					++total_chars;
					++pos;
				}
			}

			total_chars += flow - start;
			strncpy(pos, start, flow - start);
			if (cap_next)
			{
				cap_next = FALSE;
				*pos = UPPER(*pos);
			}
			pos += flow - start;
		}

		if (cap_next_next)
		{
			if ((total_chars + 3) > 79)
			{
				strcpy(pos, "\r\n");
				total_chars = 0;
				pos += 2;
			}
			else
			{
				strcpy(pos, " ");
				total_chars += 2;
				pos += 1;
			}
		}
	}
	strcpy(pos, "\r\n");

	if (static_cast<size_t>(pos - formatted) > maxlen)
	{
		formatted[maxlen] = '\0';
	}
	writer->set_string(formatted);
}


const char *some_pads[3][38] =
{
	{"����", "�����", "���", "�����", "�����", "�����", "���", "���", "����", "����", "�������", "�����", "�����", "������", "������", "������", "��������", "�����", "���������", "���������", "��������������", "������", "�������", "������", "�����", "�����", "�������", "����", "�������", "������", "�������", "����������", "���������", "������", "�������", "����������", "���������", "���������� ����"},
	{"����", "���", "���", "����", "������", "������", "����", "����", "�����", "�����", "�������", "������", "������", "�������", "�������", "�������", "������", "������", "�������", "��������", "��������������", "������", "�����", "������", "�����", "�����", "�������", "����", "������", "������", "�������", "����������", "���������", "������", "�������", "����������", "���������", "�������"},
	{"���", "����", "����", "����", "������", "������", "����", "����", "�����", "�����", "������", "������", "������", "�������", "�������", "�������", "�������", "������", "��������", "���������", "��������������", "������", "������", "������", "�����", "�����", "��������", "����", "������", "������", "�������", "����������", "���������", "������", "�������", "����������", "���������", "��������� ���"}
};

const char * desc_count(long how_many, int of_what)
{
	if (how_many < 0)
		how_many = -how_many;
	if ((how_many % 100 >= 11 && how_many % 100 <= 14) || how_many % 10 >= 5 || how_many % 10 == 0)
		return some_pads[0][of_what];
	if (how_many % 10 == 1)
		return some_pads[1][of_what];
	else
		return some_pads[2][of_what];
}

int check_moves(CHAR_DATA * ch, int how_moves)
{
	if (IS_IMMORTAL(ch) || IS_NPC(ch))
		return (TRUE);
	if (GET_MOVE(ch) < how_moves)
	{
		send_to_char("�� ������� ������.\r\n", ch);
		return (FALSE);
	}
	GET_MOVE(ch) -= how_moves;
	return (TRUE);
}

int real_sector(int room)
{
	int sector = SECT(room);

	if (ROOM_FLAGGED(room, ROOM_NOWEATHER))
		return sector;
	switch (sector)
	{
	case SECT_INSIDE:
	case SECT_CITY:
	case SECT_FLYING:
	case SECT_UNDERWATER:
	case SECT_SECRET:
	case SECT_STONEROAD:
	case SECT_ROAD:
	case SECT_WILDROAD:
		return sector;
		break;
	case SECT_FIELD:
		if (world[room]->weather.snowlevel > 20)
			return SECT_FIELD_SNOW;
		else if (world[room]->weather.rainlevel > 20)
			return SECT_FIELD_RAIN;
		else
			return SECT_FIELD;
		break;
	case SECT_FOREST:
		if (world[room]->weather.snowlevel > 20)
			return SECT_FOREST_SNOW;
		else if (world[room]->weather.rainlevel > 20)
			return SECT_FOREST_RAIN;
		else
			return SECT_FOREST;
		break;
	case SECT_HILLS:
		if (world[room]->weather.snowlevel > 20)
			return SECT_HILLS_SNOW;
		else if (world[room]->weather.rainlevel > 20)
			return SECT_HILLS_RAIN;
		else
			return SECT_HILLS;
		break;
	case SECT_MOUNTAIN:
		if (world[room]->weather.snowlevel > 20)
			return SECT_MOUNTAIN_SNOW;
		else
			return SECT_MOUNTAIN;
		break;
	case SECT_WATER_SWIM:
	case SECT_WATER_NOSWIM:
		if (world[room]->weather.icelevel > 30)
			return SECT_THICK_ICE;
		else if (world[room]->weather.icelevel > 20)
			return SECT_NORMAL_ICE;
		else if (world[room]->weather.icelevel > 10)
			return SECT_THIN_ICE;
		else
			return sector;
		break;
	}
	return SECT_INSIDE;
}

bool same_group(CHAR_DATA * ch, CHAR_DATA * tch)
{
	if (!ch || !tch)
		return false;

	// ��������� �������� ����� �� ����� ��������������� ��� �������� ������������ (������)
	if (IS_NPC(ch)
		&& ch->has_master()
		&& !IS_NPC(ch->get_master())
		&& (IS_HORSE(ch)
			|| AFF_FLAGGED(ch, EAffectFlag::AFF_CHARM)
			|| MOB_FLAGGED(ch, MOB_ANGEL)
			|| MOB_FLAGGED(ch, MOB_GHOST)))
	{
		ch = ch->get_master();
	}

	if (IS_NPC(tch)
		&& tch->has_master()
		&& !IS_NPC(tch->get_master())
		&& (IS_HORSE(tch)
			|| AFF_FLAGGED(tch, EAffectFlag::AFF_CHARM)
			|| MOB_FLAGGED(tch, MOB_ANGEL)
			|| MOB_FLAGGED(tch, MOB_GHOST)))
	{
		tch = tch->get_master();
	}

	// NPC's always in same group
	if ((IS_NPC(ch) && IS_NPC(tch))
		|| ch == tch)
	{
		return true;
	}

	if (!AFF_FLAGGED(ch, EAffectFlag::AFF_GROUP)
		|| !AFF_FLAGGED(tch, EAffectFlag::AFF_GROUP))
	{
		return false;
	}

	if (ch->get_master() == tch
		|| tch->get_master() == ch
		|| (ch->has_master()
			&& ch->get_master() == tch->get_master()))
	{
		return true;
	}

	return false;
}

// �������� �������� ������� ������.
bool is_rent(room_rnum room)
{
	// ������� � ������ �����, �� ���� �������
	if (ROOM_FLAGGED(room, ROOM_HOUSE))
	{
		const auto clan = Clan::GetClanByRoom(room);
		if (!clan)
		{
			return false;
		}
	}
	// ������� ��� ������� � ���
	for (CHAR_DATA *ch = world[room]->people; ch; ch = ch->next_in_room)
	{
		if (IS_NPC(ch) && IS_RENTKEEPER(ch))
		{
			return true;
		}
	}
	return false;
}

// �������� �������� ������� ������.
int is_post(room_rnum room)
{
	CHAR_DATA *ch;
	for (ch = world[room]->people; ch; ch = ch->next_in_room)
		if (IS_NPC(ch) && IS_POSTKEEPER(ch))
			return (TRUE);
	return (FALSE);

}

// �������������� ������ � ������������ � �������� act-a
// output act format//
char *format_act(const char *orig, CHAR_DATA * ch, OBJ_DATA * obj, const void *vict_obj)
{
	const char *i = NULL;
	char *buf, *lbuf;
	ubyte padis;
	int stopbyte;
//	CHAR_DATA *dg_victim = NULL;

	buf = (char *) malloc(MAX_STRING_LENGTH);
	lbuf = buf;

	for (stopbyte = 0; stopbyte < MAX_STRING_LENGTH; stopbyte++)
	{
		if (*orig == '$')
		{
			switch (*(++orig))
			{
			case 'n':
				if (*(orig + 1) < '0' || *(orig + 1) > '5')
					i = GET_PAD(ch, 0);
				else
				{
					padis = *(++orig) - '0';
					i = GET_PAD(ch, padis);
				}
				break;
			case 'N':
				if (*(orig + 1) < '0' || *(orig + 1) > '5')
				{
					CHECK_NULL(vict_obj, GET_PAD((const CHAR_DATA *) vict_obj, 0));
				}
				else
				{
					padis = *(++orig) - '0';
					CHECK_NULL(vict_obj, GET_PAD((const CHAR_DATA *) vict_obj, padis));
				}
				//dg_victim = (CHAR_DATA *) vict_obj;
				break;

			case 'm':
				i = HMHR(ch);
				break;
			case 'M':
				if (vict_obj)
					i = HMHR((const CHAR_DATA *) vict_obj);
				else
					CHECK_NULL(obj, OMHR(obj));
				//dg_victim = (CHAR_DATA *) vict_obj;
				break;

			case 's':
				i = HSHR(ch);
				break;
			case 'S':
				if (vict_obj)
					i = HSHR((const CHAR_DATA *) vict_obj);
				else
					CHECK_NULL(obj, OSHR(obj));
				//dg_victim = (CHAR_DATA *) vict_obj;
				break;

			case 'e':
				i = HSSH(ch);
				break;
			case 'E':
				if (vict_obj)
					i = HSSH((const CHAR_DATA *) vict_obj);
				else
					CHECK_NULL(obj, OSSH(obj));
				break;

			case 'o':
				if (*(orig + 1) < '0' || *(orig + 1) > '5')
				{
					CHECK_NULL(obj, obj->get_PName(0).c_str());
				}
				else
				{
					padis = *(++orig) - '0';
					CHECK_NULL(obj, obj->get_PName(padis > 5 ? 0 : padis).c_str());
				}
				break;
			case 'O':
				if (*(orig + 1) < '0' || *(orig + 1) > '5')
				{
					CHECK_NULL(vict_obj, ((const OBJ_DATA *) vict_obj)->get_PName(0).c_str());
				}
				else
				{
					padis = *(++orig) - '0';
					CHECK_NULL(vict_obj, ((const OBJ_DATA *) vict_obj)->get_PName(padis > 5 ? 0 : padis).c_str());
				}
				//dg_victim = (CHAR_DATA *) vict_obj;
				break;

			case 't':
				CHECK_NULL(obj, (const char *) obj);
				break;

			case 'T':
				CHECK_NULL(vict_obj, (const char *) vict_obj);
				break;

			case 'F':
				CHECK_NULL(vict_obj, (const char *) vict_obj);
				break;

			case '$':
				i = "$";
				break;

			case 'a':
				i = GET_CH_SUF_6(ch);
				break;
			case 'A':
				if (vict_obj)
					i = GET_CH_SUF_6((const CHAR_DATA *) vict_obj);
				else
					CHECK_NULL(obj, GET_OBJ_SUF_6(obj));
				//dg_victim = (CHAR_DATA *) vict_obj;
				break;

			case 'g':
				i = GET_CH_SUF_1(ch);
				break;
			case 'G':
				if (vict_obj)
					i = GET_CH_SUF_1((const CHAR_DATA *) vict_obj);
				else
					CHECK_NULL(obj, GET_OBJ_SUF_1(obj));
				//dg_victim = (CHAR_DATA *) vict_obj;
				break;

			case 'y':
				i = GET_CH_SUF_5(ch);
				break;
			case 'Y':
				if (vict_obj)
					i = GET_CH_SUF_5((const CHAR_DATA *) vict_obj);
				else
					CHECK_NULL(obj, GET_OBJ_SUF_5(obj));
				//dg_victim = (CHAR_DATA *) vict_obj;
				break;

			case 'u':
				i = GET_CH_SUF_2(ch);
				break;
			case 'U':
				if (vict_obj)
					i = GET_CH_SUF_2((const CHAR_DATA *) vict_obj);
				else
					CHECK_NULL(obj, GET_OBJ_SUF_2(obj));
				//dg_victim = (CHAR_DATA *) vict_obj;
				break;

			case 'w':
				i = GET_CH_SUF_3(ch);
				break;
			case 'W':
				if (vict_obj)
					i = GET_CH_SUF_3((const CHAR_DATA *) vict_obj);
				else
					CHECK_NULL(obj, GET_OBJ_SUF_3(obj));
				//dg_victim = (CHAR_DATA *) vict_obj;
				break;

			case 'q':
				i = GET_CH_SUF_4(ch);
				break;
			case 'Q':
				if (vict_obj)
					i = GET_CH_SUF_4((const CHAR_DATA *) vict_obj);
				else
					CHECK_NULL(obj, GET_OBJ_SUF_4(obj));
				//dg_victim = (CHAR_DATA *) vict_obj;
				break;
//Polud ������� ��������� ����������� ���(�,�,�)
			case 'z':
				if (obj)
					i = OYOU(obj);
				else
					CHECK_NULL(obj, OYOU(obj));
				break;
			case 'Z':
				if (vict_obj)
					i = HYOU((const CHAR_DATA *)vict_obj);
				else
					CHECK_NULL(vict_obj, HYOU((const CHAR_DATA *)vict_obj));
				break;
//-Polud
			default:
				log("SYSERR: Illegal $-code to act(): %c", *orig);
				log("SYSERR: %s", orig);
				i = "";
				break;
			}
			while ((*buf = *(i++)))
				buf++;
			orig++;
		}
		else if (*orig == '\\')
		{
			if (*(orig + 1) == 'r')
			{
				*(buf++) = '\r';
				orig += 2;
			}
			else if (*(orig + 1) == 'n')
			{
				*(buf++) = '\n';
				orig += 2;
			}
			else
				*(buf++) = *(orig++);
		}
		else if (!(*(buf++) = *(orig++)))
			break;
	}

	*(--buf) = '\r';
	*(++buf) = '\n';
	*(++buf) = '\0';
	return (lbuf);
}

char *rustime(const struct tm *timeptr)
{
	static char mon_name[12][10] =
	{
		"������\0", "�������\0", "�����\0", "������\0", "���\0", "����\0",
		"����\0", "�������\0", "��������\0", "�������\0", "������\0", "�������\0"
	};
	static char result[100];

	if (timeptr)
	{
		sprintf(result, "%.2d:%.2d:%.2d %2d %s %d ����",
			timeptr->tm_hour,
			timeptr->tm_min, timeptr->tm_sec, timeptr->tm_mday, mon_name[timeptr->tm_mon], 1900 + timeptr->tm_year);
	}
	else
	{
		sprintf(result, "����� ���������� ����� ����������");
	}

	return result;
}

int roundup(float fl)
{
	if ((int) fl < fl)
		return (int) fl + 1;
	else
		return (int)fl;
}

// ������� ��������� ����� �� ch ����� ������� obj � ��������� �������
// � ��������� ������ ��� � �������, ��� ����� ���������
void can_carry_obj(CHAR_DATA * ch, OBJ_DATA * obj)
{
	if (IS_CARRYING_N(ch) >= CAN_CARRY_N(ch))
	{
		send_to_char("�� �� ������ ����� ������� ���������.", ch);
		obj_to_room(obj, ch->in_room);
		obj_decay(obj);
	}
	else
	{
		if (GET_OBJ_WEIGHT(obj) + IS_CARRYING_W(ch) > CAN_CARRY_W(ch))
		{
			sprintf(buf, "��� ������� ������ ����� ��� � %s.", obj->get_PName(3).c_str());
			send_to_char(buf, ch);
			obj_to_room(obj, ch->in_room);
			// obj_decay(obj);
		}
		else
		{
			obj_to_char(obj, ch);
		}
	}
}

/**
 * ������ #define CAN_CARRY_OBJ(ch,obj)  \
   (((IS_CARRYING_W(ch) + GET_OBJ_WEIGHT(obj)) <= CAN_CARRY_W(ch)) &&   \
    ((IS_CARRYING_N(ch) + 1) <= CAN_CARRY_N(ch)))
 */
bool CAN_CARRY_OBJ(const CHAR_DATA *ch, const OBJ_DATA *obj)
{
	// ��� ��������� ���� ������ �� ������
	if (IS_NPC(ch) && !IS_CHARMICE(ch))
	{
		return true;
	}

	if (IS_CARRYING_W(ch) + GET_OBJ_WEIGHT(obj) <= CAN_CARRY_W(ch)
		&& IS_CARRYING_N(ch) + 1 <= CAN_CARRY_N(ch))
	{
		return true;
	}

	return false;
}

// shapirus: ��������, ��������� �� ��� who ���� whom
bool ignores(CHAR_DATA * who, CHAR_DATA * whom, unsigned int flag)
{
	if (IS_NPC(who)) return false;

	long ign_id;

// ���������� �� ������� �����
	if (IS_IMMORTAL(whom))
	{
		return FALSE;
	}

// ������� ������������� ������� ���� ������ ���� ���������������
	if (IS_NPC(whom)
		&& AFF_FLAGGED(whom, EAffectFlag::AFF_CHARM))
	{
		return ignores(who, whom->get_master(), flag);
	}

	ign_id = GET_IDNUM(whom);
	for (const auto& ignore : who->get_ignores())
	{
		if ((ignore->id == ign_id || ignore->id == -1)
			&& IS_SET(ignore->mode, flag))
		{
			return TRUE;
		}
	}
	return FALSE;
}

//Gorrah
int valid_email(const char *address)
{
	int count = 0;
	static std::string special_symbols("\r\n ()<>,;:\\\"[]|/&'`$");
	std::string addr = address;
	std::string::size_type dog_pos = 0, pos = 0;

	// ������� ����������� �������� ��� ��������� //
	if (addr.find_first_of(special_symbols) != std::string::npos)
		return 0;
	size_t size = addr.size();
	for (size_t i = 0; i < size; i++)
	{
		if (addr[i] <= ' ' || addr[i] >= 127)
		{
			return 0;
		}
	}
	// ������ ������ ���� ������ ���� � �� ������ � ����� ������� //
	while ((pos = addr.find_first_of('@', pos)) != std::string::npos)
	{
		dog_pos = pos;
		++count;
		++pos;
	}
	if (count != 1 || dog_pos == 0)
		return 0;
	// ��������� ������������ ���������� ������ //
	// � �������� ����� ������ ���� ��� ������� 4 �������, ������ ������ //
	if (size - dog_pos <= 3)
		return 0;
	// ����� �����������, ����������� ����� ����� ������, ��� �� ��������� ����� //
	if (addr[dog_pos + 1] == '.' || addr[size - 1] == '.' || addr.find('.', dog_pos) == std::string::npos)
		return 0;

	return 1;
}

/**
* ����� ������� (�����) � ����������� �� �� ���-�� � ����������� ������ �� �������.
* ��� �������� ����� �� 'glory ���' � '����� ����������'.
* \param flag - �� ������� 0 - 1 ������, 1 ������;
*                          1 - 1 ������, 1 ������.
*/
std::string time_format(int in_timer, int flag)
{
	char buffer[256];
	std::ostringstream out;

	double timer = in_timer;
	int one = 0, two = 0;

	if (timer < 60)
		out << timer << " " << desc_count(in_timer, flag ? WHAT_MINu : WHAT_MINa);
	else if (timer < 1440)
	{
		sprintf(buffer, "%.1f", timer / 60);
		sscanf(buffer, "%d.%d", &one, &two);
		out << one;
		if (two)
			out << "." << two  << " " << desc_count(two, WHAT_HOUR);
		else
			out << " " << desc_count(one, WHAT_HOUR);
	}
	else if (timer < 10080)
	{
		sprintf(buffer, "%.1f", timer / 1440);
		sscanf(buffer, "%d.%d", &one, &two);
		out << one;
		if (two)
			out << "." << two  << " " << desc_count(two, WHAT_DAY);
		else
			out << " " << desc_count(one, WHAT_DAY);
	}
	else if (timer < 44640)
	{
		sprintf(buffer, "%.1f", timer / 10080);
		sscanf(buffer, "%d.%d", &one, &two);
		out << one;
		if (two)
			out << "." << two  << " " << desc_count(two, WHAT_WEEK);
		else
			out << " " << desc_count(one, flag ? WHAT_WEEKu : WHAT_WEEK);
	}
	else
	{
		sprintf(buffer, "%.1f", timer / 44640);
		sscanf(buffer, "%d.%d", &one, &two);
		out << one;
		if (two)
			out << "." << two  << " " << desc_count(two, WHAT_MONTH);
		else
			out << " " << desc_count(one, WHAT_MONTH);
	}
	return out.str();
}

// * ��� ��������� ����� � ����� ��� ���� �����.
void skip_dots(char **string)
{
	for (; **string && (strchr(" .", **string) != NULL); (*string)++);
}

// Return pointer to first occurrence in string ct in
// cs, or NULL if not present.  Case insensitive
const char *str_str(const char *cs, const char *ct)
{
	if (!cs || !ct)
	{
		return NULL;
	}

	while (*cs)
	{
		const char* t = ct;

		while (*cs && (LOWER(*cs) != LOWER(*t)))
		{
			cs++;
		}

		const char* s = cs;
		while (*t && *cs && (LOWER(*cs) == LOWER(*t)))
		{
			t++;
			cs++;
		}

		if (!*t)
		{
			return s;
		}
	}

	return NULL;
}

// remove ^M's from file output
void kill_ems(char *str)
{
	char *ptr1, *ptr2;

	ptr1 = str;
	ptr2 = str;

	while (*ptr1)
	{
		if ((*(ptr2++) = *(ptr1++)) == '\r')
			if (*ptr1 == '\r')
				ptr1++;
	}
	*ptr2 = '\0';
}

// * ��������� � ����������� � word ������ ����� �� str (a_isalnum).
void cut_one_word(std::string &str, std::string &word)
{
	if (str.empty())
	{
		word.clear();
		return;
	}

	bool process = false;
	unsigned begin = 0, end = 0;
	for (unsigned i = 0; i < str.size(); ++i)
	{
		if (!process && a_isalnum(str.at(i)))
		{
			process = true;
			begin = i;
			continue;
		}
		if (process && !a_isalnum(str.at(i)))
		{
			end = i;
			break;
		}
	}

	if (process)
	{
		if (!end || end >= str.size())
		{
			word = str.substr(begin);
			str.clear();
		}
		else
		{
			word = str.substr(begin, end - begin);
			str.erase(0, end);
		}
		return;
	}

	str.clear();
	word.clear();
}

void ReadEndString(std::ifstream &file)
{
	char c;
	while (file.get(c))
	{
		if (c == '\n')
		{
			return;
		}
	}
}

void StringReplace(std::string & buffer, char s, const std::string& d)
{
	for (size_t index = 0; index = buffer.find(s, index), index != std::string::npos;)
	{
		buffer.replace(index, 1, d);
		index += d.length();
	}
}

std::string& format_news_message(std::string &text)
{
	StringReplace(text, '\n', "\n   ");
	boost::trim(text);
	text.insert(0, "   ");
	text += '\n';
	return text;
}

/**
 * ����� � http://www.openbsd.org/cgi-bin/cvsweb/src/lib/libc/string/strlcpy.c?rev=1.11&content-type=text/x-cvsweb-markup
 * Copy src to string dst of size siz.  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz == 0).
 * Returns strlen(src); if retval >= siz, truncation occurred.
 */
size_t strl_cpy(char *dst, const char *src, size_t siz)
{
	char *d = dst;
	const char *s = src;
	size_t n = siz;

	// Copy as many bytes as will fit
	if (n != 0)
	{
		while (--n != 0)
		{
			if ((*d++ = *s++) == '\0')
				break;
		}
	}

	// Not enough room in dst, add NUL and traverse rest of src
	if (n == 0)
	{
		if (siz != 0)
			*d = '\0';		// NUL-terminate dst
		while (*s++)
			;
	}

	return(s - src - 1);	// count does not include NUL
}

/**
* ������ ������� GET_REAL_DR(ch)
* ��� ����� ����������� � 50 �������� ������
* +��� ���� ������ �������, � ������ ������ �������� 30d127
*/
int get_real_dr(CHAR_DATA *ch)
{
	int dd = 0;
// ������������� ������� ��� ����������� ������ ���������/����������� �� ���������� ��������������	
	int dam[36] = {0,0,0,1,1,1,2,2,2,3,3,3,4,4,4,5,5,5,6,6,6,6,6,6,6,7,7,7,7,7,7,8,8,8,8,8};
        if (IS_SMITH(ch) || IS_GUARD(ch) || IS_RANGER(ch))  // ����� ����� ������ ����������, ��������, �������
           dd = dam[MIN(35, GET_REMORT(ch))]; //��� �� 35+ ����
           GET_HR_ADD(ch) += dd;           // ����� ��������� �� ���������� ��������������

        int level = GET_LEVEL(ch);

	int bonus = 0;
	if (IS_NPC(ch) && !IS_CHARMICE(ch))
	{
		if (level > STRONG_MOB_LEVEL)
		    bonus += level + number(0, level);
		return MAX(0, GET_DR(ch) + GET_DR_ADD(ch) + bonus);
	}
	if (can_use_feat(ch, BOWS_FOCUS_FEAT) && ch->get_skill(SKILL_ADDSHOT))
	{
		return MAX(0, GET_DR(ch) + GET_DR_ADD(ch) + dd * 2);
	}
	else
	{
		return (VPOSI(GET_DR(ch) + GET_DR_ADD(ch), -50, (IS_MORTIFIER(ch) ? 100 : 50)) + dd * 2);
	}
}

// ��� �����������
int get_real_dr_new(CHAR_DATA *ch)
{
    return MAX(0, GET_DR(ch) + GET_DR_ADD(ch));
}


////////////////////////////////////////////////////////////////////////////////
// show money

namespace MoneyDropStat
{

typedef std::map<int /* zone vnum */, long /* money count */> ZoneListType;
ZoneListType zone_list;

void add(int zone_vnum, long money)
{
	ZoneListType::iterator i = zone_list.find(zone_vnum);
	if (i != zone_list.end())
	{
		i->second += money;
	}
	else
	{
		zone_list[zone_vnum] = money;
	}
}

void print(CHAR_DATA *ch)
{
	if (!PRF_FLAGGED(ch, PRF_CODERINFO))
	{
		send_to_char(ch, "���� � ����������.\r\n");
		return;
	}

	std::map<long, int> tmp_list;
	for (ZoneListType::const_iterator i = zone_list.begin(), iend = zone_list.end(); i != iend; ++i)
	{
		if (i->second > 0)
		{
			tmp_list[i->second] = i->first;
		}
	}
	if (!tmp_list.empty())
	{
		send_to_char(ch,
				"Money drop stats:\r\n"
				"Total zones: %d\r\n"
				"  vnum - money\r\n"
				"================\r\n", tmp_list.size());
	}
	else
	{
		send_to_char(ch, "Empty.\r\n");
		return;
	}
	std::stringstream out;
	for (std::map<long, int>::const_reverse_iterator i = tmp_list.rbegin(), iend = tmp_list.rend(); i != iend; ++i)
	{
		out << "  " << std::setw(4) << i->second << " - " << i->first << "\r\n";
//		send_to_char(ch, "  %4d - %ld\r\n", i->second, i->first);
	}
	page_string(ch->desc, out.str());
}

void print_log()
{
	for (ZoneListType::const_iterator i = zone_list.begin(), iend = zone_list.end(); i != iend; ++i)
	{
		if (i->second > 0)
		{
			log("ZoneDrop: %d %ld", i->first, i->second);
		}
	}
}

} // MoneyDropStat

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// show exp
namespace ZoneExpStat
{

struct zone_data
{
	zone_data() : gain(0), lose(0) {};

	unsigned long gain;
	unsigned long lose;
};

typedef std::map<int /* zone vnum */, zone_data> ZoneListType;
ZoneListType zone_list;

void add(int zone_vnum, long exp)
{
	if (!exp)
	{
		return;
	}

	ZoneListType::iterator i = zone_list.find(zone_vnum);
	if (i != zone_list.end())
	{
		if (exp > 0)
		{
			i->second.gain += exp;
		}
		else
		{
			i->second.lose += -exp;
		}
	}
	else
	{
		zone_data tmp_zone;
		if (exp > 0)
		{
			tmp_zone.gain = exp;
		}
		else
		{
			tmp_zone.lose = -exp;
		}
		zone_list[zone_vnum] = tmp_zone;
	}
}

void print_gain(CHAR_DATA *ch)
{
	if (!PRF_FLAGGED(ch, PRF_CODERINFO))
	{
		send_to_char(ch, "���� � ����������.\r\n");
		return;
	}

	std::map<unsigned long, int> tmp_list;
	for (ZoneListType::const_iterator i = zone_list.begin(), iend = zone_list.end(); i != iend; ++i)
	{
		if (i->second.gain > 0)
		{
			tmp_list[i->second.gain] = i->first;
		}
	}
	if (!tmp_list.empty())
	{
		send_to_char(ch,
				"Gain exp stats:\r\n"
				"Total zones: %d\r\n"
				"  vnum - exp\r\n"
				"================\r\n", tmp_list.size());
	}
	else
	{
		send_to_char(ch, "Empty.\r\n");
		return;
	}
	std::stringstream out;
	for (std::map<unsigned long, int>::const_reverse_iterator i = tmp_list.rbegin(), iend = tmp_list.rend(); i != iend; ++i)
	{
		out << "  " << std::setw(4) << i->second << " - " << i->first << "\r\n";
	}
	page_string(ch->desc, out.str());
}

void print_log()
{
	for (ZoneListType::const_iterator i = zone_list.begin(), iend = zone_list.end(); i != iend; ++i)
	{
		if (i->second.gain > 0 || i->second.lose > 0)
		{
			log("ZoneExp: %d %lu %lu", i->first, i->second.gain, i->second.lose);
		}
	}
}

} // ZoneExpStat
////////////////////////////////////////////////////////////////////////////////

std::string thousands_sep(long long n)
{
	bool negative = false;
	if (n < 0)
	{
		n = -n;
		negative = true;
	}

	int size = 50;
	int curr_pos = size - 1;
	const int comma = ',';
	std::string buffer;
	buffer.resize(size);
	int i = 0;

	try
	{
		do
		{
			if (i % 3 == 0 && i != 0)
			{
				buffer.at(--curr_pos) = comma;
			}
			buffer.at(--curr_pos) = '0' + n % 10;
			n /= 10;
			i++;
		}
		while(n != 0);

		if (negative)
		{
			buffer.at(--curr_pos) = '-';
		}
	}
	catch (...)
	{
		log("SYSERROR : string.at() (%s:%d)", __FILE__, __LINE__);
		return "<OutOfRange>";
	}

	buffer = buffer.substr(curr_pos, size - 1);
	return buffer;
}

int str_bonus(int str, int type)
{
	int bonus = 0;
	str = MAX(1, str);

	switch (type)
	{
	case STR_TO_HIT:
		// -5 ... 10
		if (str < 10)
		{
			bonus = (str - 11) / 2;
		}
		else
		{
			bonus = (str - 10) / 4;
		}
		break;
	case STR_TO_DAM:
		// -5 ... 36
		if (str < 15)
		{
			bonus = (str - 11) / 2;
		}
		else
		{
			bonus = str - 14;
		}
		break;
	case STR_CARRY_W:
		// 50 ... 2500
		bonus = str * 50;
		break;
	case STR_WIELD_W:
		if (str <= 20)
		{
			// w 1 .. 20
			bonus = str;
		}
		else if (str < 30)
		{
			// w 20,5 .. 24,5
			bonus = 20 + (str - 20) / 2;
		}
		else
		{
			// w >= 25
			bonus = 25 + (str - 30) / 4;
		}
		break;
	case STR_HOLD_W:
		if (str <= 29)
		{
			// w 1 .. 15
			bonus = (str + 1) / 2;
		}
		else
		{
			// w >= 16
			bonus = 15 + (str - 29) / 4;
		}
		break;
	default:
		log("SYSERROR: str=%d, type=%d (%s %s %d)",
				str, type, __FILE__, __func__, __LINE__);
	}

	return bonus;
}

int dex_bonus(int dex)
{
	int bonus = 0;
	dex = MAX(1, dex);
	// -5 ... 40
	if (dex < 10)
	{
		bonus = (dex - 11) / 2;
	}
	else
	{
		bonus = dex - 10;
	}
	return bonus;
}

int dex_ac_bonus(int dex)
{
	int bonus = 0;
	dex = MAX(1, dex);
	// -3 ... 35
	if (dex <= 15)
	{
		bonus = (dex - 10) / 3;
	}
	else
	{
		bonus = dex - 15;
	}
	return bonus;
}

int wis_bonus(int stat, int type)
{
	int bonus = 0;
	stat = MAX(1, stat);

	switch (type)
	{
	case WIS_MAX_LEARN_L20:
		// 28 .. 175
		bonus = 28 + (stat - 1) * 3;
		break;
	case WIS_SPELL_SUCCESS:
		// -80 .. 116
		bonus = stat * 4 - 84;
		break;
	case WIS_MAX_SKILLS:
		// 1 .. 15
		if (stat <= 15)
		{
			bonus = stat;
		}
		// 15 .. 32
		else
		{
			bonus = 15 + (stat - 15) / 2;
		}
		break;
	case WIS_FAILS:
		// 34 .. 66
		if (stat <= 9)
		{
			bonus = 30 + stat * 4;
		}
		// 140 .. 940
		else
		{
			bonus = 120 + (stat - 9) * 20;
		}
		break;
	default:
		log("SYSERROR: stat=%d, type=%d (%s %s %d)",
				stat, type, __FILE__, __func__, __LINE__);
	}

	return bonus;
}

int calc_str_req(int weight, int type)
{
	int str = 0;
	weight = MAX(1, weight);

	switch (type)
	{
	case STR_WIELD_W:
		if (weight <= 20)
		{
			str = weight;
		}
		else if (weight <= 24)
		{
			str = 2 * weight - 20;
		}
		else
		{
			str = 4 * weight - 70;
		}
		break;
	case STR_HOLD_W:
		if (weight <= 15)
		{
			str = 2 * weight - 1;
		}
		else
		{
			str = 4 * weight - 31;
		}
		break;
	case STR_BOTH_W:
		if (weight <= 31)
		{
			str = weight - (weight + 1) / 3;
		}
		else if (weight <= 40)
		{
			str = weight - 10;
		}
		else
		{
			str = (weight - 39) * 2 + 29 - (weight + 1) % 2;
		}
		break;
	default:
		log("SYSERROR: weight=%d, type=%d (%s %s %d)",
				weight, type, __FILE__, __func__, __LINE__);
	}

	return str;
}

void message_str_need(CHAR_DATA *ch, OBJ_DATA *obj, int type)
{
	if (GET_POS(ch) == POS_DEAD)
		return;
	int need_str = 0;
	switch (type)
	{
	case STR_WIELD_W:
		need_str = calc_str_req(GET_OBJ_WEIGHT(obj), STR_WIELD_W);
		break;
	case STR_HOLD_W:
		need_str = calc_str_req(GET_OBJ_WEIGHT(obj), STR_HOLD_W);
		break;
	case STR_BOTH_W:
		need_str = calc_str_req(GET_OBJ_WEIGHT(obj), STR_BOTH_W);
		break;
	case STR_SHIELD_W:
		need_str = calc_str_req((GET_OBJ_WEIGHT(obj)+1)/2, STR_HOLD_W);
		break;
	default:
		log("SYSERROR: ch=%s, weight=%d, type=%d (%s %s %d)",
				GET_NAME(ch), GET_OBJ_WEIGHT(obj), type,
				__FILE__, __func__, __LINE__);
		return;
	}
	send_to_char(ch, "��� ����� ��������� %d %s.\r\n",
		need_str, desc_count(need_str, WHAT_STR));
}

bool GetAffectNumByName(const std::string& affName, EAffectFlag& result)
{
	int base = 0, offset = 0, counter = 0;
	bool endOfArray = false;
	while (!endOfArray)
	{
		if (affName == std::string(affected_bits[counter]))
		{
			result = static_cast<EAffectFlag>((base << 30) | (1 << offset));
			return true;
		}
		offset++;
		if (*affected_bits[counter] == '\n')
		{
			base++;
			offset = 0;
			if (*affected_bits[counter+1] == '\n')
				endOfArray = true;
		}
		counter++;
	}
	return false;
}

/// ������� ���-�� ������ &R � �.�. � ������
/// size_t len = 0 - �� ������� ������� strlen(str)
size_t count_colors(const char * str, size_t len)
{
	unsigned int c, cc = 0;
	len = len ? len : strlen(str);

	for (c = 0; c < len - 1; c++)
	{
		if (*(str+c)=='&' && *(str+c+1)!='&')
			cc++;
	}
	return cc;
}

//���������� ������ ����� len + ���-�� ������*2 ��� ���� ���� � �������� ��� ���� ���������
//left_align ������������ ������ �����
char* colored_name(const char * str, size_t len, const bool left_align)
{
	static char cstr[128];
	static char fmt[6];
	size_t cc = len + count_colors(str) * 2;

	if (strlen(str) < cc)
	{
		snprintf(fmt, sizeof(fmt), "%%%s%ds", (left_align ? "-" : ""), static_cast<int>(cc));
		snprintf(cstr, sizeof(cstr), fmt, str);
	}
	else
	{
		snprintf(cstr, sizeof(cstr), "%s", str);
	}

	return cstr;
}

/// ����� ������ ��� �������� ����� �� count_colors()
size_t strlen_no_colors(const char *str)
{
	const size_t len = strlen(str);
	return len - count_colors(str, len) * 2;
}

// ��������� ����� �� ����
void tell_to_char(CHAR_DATA *keeper, CHAR_DATA *ch, const char *arg)
{
	if (AFF_FLAGGED(ch, EAffectFlag::AFF_DEAFNESS) || PRF_FLAGGED(ch, PRF_NOTELL))
		return;
	char local_buf[MAX_INPUT_LENGTH];
	snprintf(local_buf, MAX_INPUT_LENGTH,
		"%s ������%s ��� : '%s'", GET_NAME(keeper), GET_CH_SUF_1(keeper), arg);
	send_to_char(ch, "%s%s%s\r\n",
		CCICYN(ch, C_NRM), CAP(local_buf), CCNRM(ch, C_NRM));
}

int CAN_CARRY_N(const CHAR_DATA* ch)
{
	int n = 5 + GET_REAL_DEX(ch) / 2 + GET_LEVEL(ch) / 2;
	if (HAVE_FEAT(ch, JUGGLER_FEAT))
	{
		n += GET_LEVEL(ch) / 2;
		if (GET_CLASS(ch) == CLASS_DRUID)
		{
			n += 5;
		}
	}
	if (GET_CLASS(ch) == CLASS_DRUID)
	{
		n += 5;
	}
	return std::max(n, 1);
}

bool ParseFilter::init_type(const char *str)
{
	if (is_abbrev(str, "����")
		|| is_abbrev(str, "light"))
	{
		type = OBJ_DATA::ITEM_LIGHT;
	}
	else if (is_abbrev(str, "������")
		|| is_abbrev(str, "scroll"))
	{
		type = OBJ_DATA::ITEM_SCROLL;
	}
	else if (is_abbrev(str, "�������")
		|| is_abbrev(str, "wand"))
	{
		type = OBJ_DATA::ITEM_WAND;
	}
	else if (is_abbrev(str, "�����")
		|| is_abbrev(str, "staff"))
	{
		type = OBJ_DATA::ITEM_STAFF;
	}
	else if (is_abbrev(str, "������")
		|| is_abbrev(str, "weapon"))
	{
		type = OBJ_DATA::ITEM_WEAPON;
	}
	else if (is_abbrev(str, "�����")
		|| is_abbrev(str, "armor"))
	{
		type = OBJ_DATA::ITEM_ARMOR;
	}
	else if (is_abbrev(str, "�������")
		|| is_abbrev(str, "potion"))
	{
		type = OBJ_DATA::ITEM_POTION;
	}
	else if (is_abbrev(str, "������")
		|| is_abbrev(str, "������")
		|| is_abbrev(str, "other"))
	{
		type = OBJ_DATA::ITEM_OTHER;
	}
	else if (is_abbrev(str, "���������")
		|| is_abbrev(str, "container"))
	{
		type = OBJ_DATA::ITEM_CONTAINER;
	}
	else if (is_abbrev(str, "��������")
		|| is_abbrev(str, "material"))
	{
		type = OBJ_DATA::ITEM_MATERIAL;
	}
	else if (is_abbrev(str, "������������")
		|| is_abbrev(str, "enchant"))
	{
		type = OBJ_DATA::ITEM_ENCHANT;
	}
	else if (is_abbrev(str, "�������")
		|| is_abbrev(str, "tank"))
	{
		type = OBJ_DATA::ITEM_DRINKCON;
	}
	else if (is_abbrev(str, "�����")
		|| is_abbrev(str, "book"))
	{
		type = OBJ_DATA::ITEM_BOOK;
	}
	else if (is_abbrev(str, "����")
		|| is_abbrev(str, "rune"))
	{
		type = OBJ_DATA::ITEM_INGREDIENT;
	}
	else if (is_abbrev(str, "����������")
		|| is_abbrev(str, "ingradient"))
	{
		type = OBJ_DATA::ITEM_MING;
	}
	else if (is_abbrev(str, "������")
		|| is_abbrev(str, "������"))
	{
		type = OBJ_DATA::ITEM_ARMOR_LIGHT;
	}
	else if (is_abbrev(str, "�������")
		|| is_abbrev(str, "�������"))
	{
		type = OBJ_DATA::ITEM_ARMOR_MEDIAN;
	}
	else if (is_abbrev(str, "�������")
		|| is_abbrev(str, "�������"))
	{
		type = OBJ_DATA::ITEM_ARMOR_HEAVY;
	}
	else
	{
		return false;
	}

	return true;
}

bool ParseFilter::init_state(const char *str)
{
	if (is_abbrev(str, "������"))
		state = 0;
	else if (is_abbrev(str, "����� ����������"))
		state = 20;
	else if (is_abbrev(str, "���������"))
		state = 40;
	else if (is_abbrev(str, "������"))
		state = 60;
	else if (is_abbrev(str, "��������"))
		state = 80;
	else if (is_abbrev(str, "��������"))
		state = 1000;
	else return false;

	return true;
}

bool ParseFilter::init_wear(const char *str)
{
	if (is_abbrev(str, "�����"))
	{
		wear = EWearFlag::ITEM_WEAR_FINGER;
		wear_message = 1;
	}
	else if (is_abbrev(str, "���") || is_abbrev(str, "�����"))
	{
		wear = EWearFlag::ITEM_WEAR_NECK;
		wear_message = 2;
	}
	else if (is_abbrev(str, "����"))
	{
		wear = EWearFlag::ITEM_WEAR_BODY;
		wear_message = 3;
	}
	else if (is_abbrev(str, "������"))
	{
		wear = EWearFlag::ITEM_WEAR_HEAD;
		wear_message = 4;
	}
	else if (is_abbrev(str, "����"))
	{
		wear = EWearFlag::ITEM_WEAR_LEGS;
		wear_message = 5;
	}
	else if (is_abbrev(str, "������"))
	{
		wear = EWearFlag::ITEM_WEAR_FEET;
		wear_message = 6;
	}
	else if (is_abbrev(str, "�����"))
	{
		wear = EWearFlag::ITEM_WEAR_HANDS;
		wear_message = 7;
	}
	else if (is_abbrev(str, "����"))
	{
		wear = EWearFlag::ITEM_WEAR_ARMS;
		wear_message = 8;
	}
	else if (is_abbrev(str, "���"))
	{
		wear = EWearFlag::ITEM_WEAR_SHIELD;
		wear_message = 9;
	}
	else if (is_abbrev(str, "�����"))
	{
		wear = EWearFlag::ITEM_WEAR_ABOUT;
		wear_message = 10;
	}
	else if (is_abbrev(str, "����"))
	{
		wear = EWearFlag::ITEM_WEAR_WAIST;
		wear_message = 11;
	}
	else if (is_abbrev(str, "��������"))
	{
		wear = EWearFlag::ITEM_WEAR_WRIST;
		wear_message = 12;
	}
	else if (is_abbrev(str, "������"))
	{
		wear = EWearFlag::ITEM_WEAR_WIELD;
		wear_message = 13;
	}
	else if (is_abbrev(str, "�����"))
	{
		wear = EWearFlag::ITEM_WEAR_HOLD;
		wear_message = 14;
	}
	else if (is_abbrev(str, "���"))
	{
		wear = EWearFlag::ITEM_WEAR_BOTHS;
		wear_message = 15;
	}
	else
	{
		return false;
	}

	return true;
}

bool ParseFilter::init_cost(const char *str)
{
	if (sscanf(str, "%d%[-+]", &cost, &cost_sign) != 2)
	{
		return false;
	}
	if (cost_sign == '-')
	{
		cost = -cost;
	}

	return true;
}

bool ParseFilter::init_weap_class(const char *str)
{
	if (is_abbrev(str, "����"))
	{
		weap_class = SKILL_BOWS;
		weap_message = 0;
	}
	else if (is_abbrev(str, "��������"))
	{
		weap_class = SKILL_SHORTS;
		weap_message = 1;
	}
	else if (is_abbrev(str, "�������"))
	{
		weap_class = SKILL_LONGS;
		weap_message = 2;
	}
	else if (is_abbrev(str, "������"))
	{
		weap_class = SKILL_AXES;
		weap_message = 3;
	}
	else if (is_abbrev(str, "������"))
	{
		weap_class = SKILL_CLUBS;
		weap_message = 4;
	}
	else if (is_abbrev(str, "����"))
	{
		weap_class = SKILL_NONSTANDART;
		weap_message = 5;
	}
	else if (is_abbrev(str, "����������"))
	{
		weap_class = SKILL_BOTHHANDS;
		weap_message = 6;
	}
	else if (is_abbrev(str, "�����������"))
	{
		weap_class = SKILL_PICK;
		weap_message = 7;
	}
	else if (is_abbrev(str, "�����"))
	{
		weap_class = SKILL_SPADES;
		weap_message = 8;
	}
	else
	{
		return false;
	}

	type = OBJ_DATA::ITEM_WEAPON;

	return true;
}

bool ParseFilter::init_realtime(const char *str)
{
	tm trynewtimeup;
	tm trynewtimedown;
	int day, month, year;
	std::string tmp_string;

	if (strlen(str) != 11)
	{
		return false;
	}

	if ((str[2] != '.')
		|| (str[5] != '.')
		|| (str[10] != '<'
			&& str[10] != '>'
			&& str[10] != '='))
	{
		return false;
	}

	if (!isdigit(static_cast<unsigned int>(str[0])) 
		|| !isdigit(static_cast<unsigned int>(str[1]))
		|| !isdigit(static_cast<unsigned int>(str[3]))
		|| !isdigit(static_cast<unsigned int>(str[4]))
		|| !isdigit(static_cast<unsigned int>(str[6]))
		|| !isdigit(static_cast<unsigned int>(str[7]))
		|| !isdigit(static_cast<unsigned int>(str[8]))
		|| !isdigit(static_cast<unsigned int>(str[9])))
	{
		return false;
	}

	tmp_string = "";
	tmp_string.push_back(str[0]);
	tmp_string.push_back(str[1]);
	day = std::stoi(tmp_string);
	tmp_string = "";
	tmp_string.push_back(str[3]);
	tmp_string.push_back(str[4]);
	month = std::stoi(tmp_string);
	tmp_string = "";
	tmp_string.push_back(str[6]);
	tmp_string.push_back(str[7]);
	tmp_string.push_back(str[8]);
	tmp_string.push_back(str[9]);
	year = std::stoi(tmp_string);

	if (year <= 1900)
	{
		return false;
	}
	if (month > 12)
	{
		return false;
	}
	if (year % 4 == 0 && month == 2 && day > 29)
	{
		return false;
	}
	else if (month == 1 && day > 31)
	{
		return false;
	}
	else if (year % 4 != 0 && month == 2 && day > 28)
	{
		return false;
	}
	else if (month == 3 && day > 31)
	{
		return false;
	}
	else if (month == 4 && day > 30)
	{
		return false;
	}
	else if (month == 5 && day > 31)
	{
		return false;
	}
	else if (month == 6 && day > 30)
	{
		return false;
	}
	else if (month == 7 && day > 31)
	{
		return false;
	}
	else if (month == 8 && day > 31)
	{
		return false;
	}
	else if (month == 9 && day > 30)
	{
		return false;
	}
	else if (month == 10 && day > 31)
	{
		return false;
	}
	else if (month == 11 && day > 30)
	{
		return false;
	}
	else if (month == 12 && day > 31)
	{
		return false;
	}

	trynewtimedown.tm_sec = 0;
	trynewtimedown.tm_hour = 0;
	trynewtimedown.tm_min = 0;
	trynewtimedown.tm_mday = day;
	trynewtimedown.tm_mon = month-1;
	trynewtimedown.tm_year = year-1900;
	new_timedown = mktime(&trynewtimedown);

	trynewtimeup.tm_sec = 59;
	trynewtimeup.tm_hour = 23;
	trynewtimeup.tm_min = 59;
	trynewtimeup.tm_mday = day;
	trynewtimeup.tm_mon = month-1;
	trynewtimeup.tm_year = year - 1900;
	new_timeup= mktime(&trynewtimeup);

	new_timesign = str[10];

	return true;
}

size_t ParseFilter::affects_cnt() const
{
	return affect_weap.size() + affect_apply.size() + affect_extra.size();
}

bool ParseFilter::init_affect(char *str, size_t str_len)
{
	// ����!
	bool strong = false;
	if (str_len > 1 && str[str_len - 1] == '!')
	{
		strong = true;
		str[str_len - 1] = '\0';
	}
	// �1, �2, �3
	if (str_len == 1)
	{
		switch (*str)
		{
			case '1':
				sprintf(str, "����� �������� 1 ������");
			break;
			case '2':
				sprintf(str, "����� �������� 2 �����");
			break;
			case '3':
				sprintf(str, "����� �������� 3 �����");
			break;
		}
	}

	lower_convert(str);
	str_len = strlen(str);

	for (int num = 0; *apply_types[num] != '\n'; ++num)
	{
		if (strong && !strcmp(str, apply_types[num]))
		{
			affect_apply.push_back(num);
			return true;
		}
		else if (!strong && isname(str, apply_types[num]))
		{
			affect_apply.push_back(num);
			return true;
		}
	}

	int num = 0;
	for (int flag = 0; flag < 4; ++flag)
	{
		for (/* ��� ������ �� ���� */; *weapon_affects[num] != '\n'; ++num)
		{
			if (strong && !strcmp(str, weapon_affects[num]))
			{
				affect_weap.push_back(num);
				return true;
			}
			else if (!strong && isname(str, weapon_affects[num]))
			{
				affect_weap.push_back(num);
				return true;
			}
		}
		++num;
	}

	num = 0;
	for (int flag = 0; flag < 4; ++flag)
	{
		for (/* ��� ������ �� ���� */; *extra_bits[num] != '\n'; ++num)
		{
			if (strong && !strcmp(str, extra_bits[num]))
			{
				affect_extra.push_back(num);
				return true;
			}
			else if (!strong && isname(str, extra_bits[num]))
			{
				affect_extra.push_back(num);
				return true;
			}
		}
		num++;
	}

	return false;
}

/// ���, ����� ��� ����-������
bool ParseFilter::check_name(OBJ_DATA *obj, CHAR_DATA *ch) const
{
	bool result = false;
	if (name.empty()
		|| isname(name, GET_OBJ_PNAME(obj, 0)))
	{
		result = true;
	}
	else if ((GET_OBJ_TYPE(obj) == OBJ_DATA::ITEM_MING
			|| GET_OBJ_TYPE(obj) == OBJ_DATA::ITEM_INGREDIENT)
		&& GET_OBJ_RNUM(obj) >= 0
		&& isname(name, obj_proto[GET_OBJ_RNUM(obj)]->get_aliases().c_str()))
	{
		result = true;
	}
	else if (ch
		&& filter_type == CLAN
		&& CHECK_CUSTOM_LABEL(name, obj, ch))
	{
		result = true;
	}

	return result;
}

bool ParseFilter::check_type(OBJ_DATA *obj) const
{
	if (type < 0
		|| type == GET_OBJ_TYPE(obj))
	{
		return true;
	}

	return false;
}

bool ParseFilter::check_state(OBJ_DATA *obj) const
{
	bool result = false;
	if (state < 0)
	{
		result = true;
	}
	else if (GET_OBJ_RNUM(obj) >= 0)
	{
		int proto_tm = obj_proto.at(GET_OBJ_RNUM(obj))->get_timer();
		if (proto_tm <= 0)
		{
			char buf_[MAX_INPUT_LENGTH];
			snprintf(buf_, sizeof(buf_), "SYSERROR: wrong obj-proto timer %d, vnum=%d (%s %s:%d)",
				proto_tm, obj_proto.at(GET_OBJ_RNUM(obj))->get_rnum(), __func__, __FILE__, __LINE__);
			mudlog(buf_, CMP, LVL_IMMORT, SYSLOG, TRUE);
		}
		else
		{
			int tm_pct;
			if (check_unlimited_timer(obj))  // ���� ������ ��������, ��������� ����������� ����� ��������
			{
				tm_pct = 1000;
			}
			else
			{
				tm_pct = obj->get_timer() * 100 / proto_tm;
			}

			if (filter_type == CLAN
				&& tm_pct >= state
				&& tm_pct < state + 20)
			{
				result = true;
			}
			else if (filter_type == EXCHANGE && tm_pct >= state)
			{
				result = true;
			}
		}
	}
	return result;
}

bool ParseFilter::check_wear(OBJ_DATA *obj) const
{
	if (wear == EWearFlag::ITEM_WEAR_UNDEFINED
		|| CAN_WEAR(obj, wear))
	{
		return true;
	}
	return false;
}

bool ParseFilter::check_weap_class(OBJ_DATA *obj) const
{
	if (weap_class < 0 || weap_class == GET_OBJ_SKILL(obj))
	{
		return true;
	}
	return false;
}

bool ParseFilter::check_cost(int obj_price) const
{
	bool result = false;

	if (cost_sign == '\0')
	{
		result = true;
	}
	else if (cost >= 0 && obj_price >= cost)
	{
		result = true;
	}
	else if (cost < 0 && obj_price <= -cost)
	{
		result = true;
	}
	return result;
}

// ���������� ��� �����... ���������� num � ��� ���� � flags
bool CompareBits(const FLAG_DATA& flags, const char *names[], int affect)
{
	int i;
	for (i = 0; i < 4; i++)
	{
		int nr = 0;
		int fail = i;
		bitvector_t bitvector = flags.get_plane(i);

		while (fail)
		{
			if (*names[nr] == '\n')
				fail--;
			nr++;
		}

		for (; bitvector; bitvector >>= 1)
		{
			if (IS_SET(bitvector, 1))
				if (*names[nr] != '\n')
					if (nr == affect)
						return 1;
			if (*names[nr] != '\n')
				nr++;
		}
	}
	return 0;
}

bool ParseFilter::check_affect_weap(OBJ_DATA *obj) const
{
	if (!affect_weap.empty())
	{
		for (auto it = affect_weap.begin(); it != affect_weap.end(); ++it)
		{
			if (!CompareBits(obj->get_affect_flags(), weapon_affects, *it))
			{
				return false;
			}
		}
	}
	return true;
}

std::string ParseFilter::show_obj_aff(OBJ_DATA *obj)
{
	if (!affect_apply.empty())
	{
		for (auto it = affect_apply.begin(); it != affect_apply.end(); ++it)
		{
			for (int i = 0; i < MAX_OBJ_AFFECT; ++i)
			{
				if (obj->get_affected(i).location == *it)
				{
					int mod = obj->get_affected(i).modifier;
					char buf_[MAX_INPUT_LENGTH];
					sprinttype(obj->get_affected(i).location, apply_types, buf_);
					for (int j = 0; *apply_negative[j] != '\n'; j++)
					{
						if (!str_cmp(buf_, apply_negative[j]))
						{
							mod = -mod;
						}
					}
					std::string return_str(buf_);
					if (mod > 0)
						return_str = return_str + " +" + std::to_string(mod);
					else
						return_str = return_str + " " + std::to_string(mod);
					return "(" + return_str + ")";
				}

			}
		}
	}
	return " ";
}


bool ParseFilter::check_affect_apply(OBJ_DATA *obj) const
{
	bool result = true;
	if (!affect_apply.empty())
	{
		for (auto it = affect_apply.begin(); it != affect_apply.end() && result; ++it)
		{
			result = false;
			for (int i = 0; i < MAX_OBJ_AFFECT; ++i)
			{
				if (obj->get_affected(i).location == *it)
				{
					int mod = obj->get_affected(i).modifier;
					char buf_[MAX_INPUT_LENGTH];
					sprinttype(obj->get_affected(i).location, apply_types, buf_);
					for (int j = 0; *apply_negative[j] != '\n'; j++)
					{
						if (!str_cmp(buf_, apply_negative[j]))
						{
							mod = -mod;
							break;
						}
					}
					result = true;
					break;
				}
			}
		}
	}
	return result;
}

bool ParseFilter::check_affect_extra(OBJ_DATA *obj) const
{
	if (!affect_extra.empty())
	{
		for (auto it = affect_extra.begin(); it != affect_extra.end(); ++it)
		{
			if (!CompareBits(GET_OBJ_EXTRA(obj), extra_bits, *it))
			{
				return false;
			}
		}
	}
	return true;
}

bool ParseFilter::check_owner(exchange_item_data *exch_obj) const
{
	if (owner.empty()
		|| isname(owner, get_name_by_id(GET_EXCHANGE_ITEM_SELLERID(exch_obj))))
	{
		return true;
	}
	return false;
}

bool ParseFilter::check_realtime(exchange_item_data *exch_obj) const
{
	bool result = false;

	if (new_timesign == '\0')
		result = true;
	else if (new_timesign == '=' && (new_timedown <= GET_EXCHANGE_ITEM_TIME(exch_obj)) && (new_timeup >= GET_EXCHANGE_ITEM_TIME(exch_obj)))
		result = true;
	else if (new_timesign == '<' && (new_timeup >= GET_EXCHANGE_ITEM_TIME(exch_obj)))
		result = true;
	else if (new_timesign == '>' && (new_timedown <= GET_EXCHANGE_ITEM_TIME(exch_obj)))
		result = true;

	return result;
}

bool ParseFilter::check(OBJ_DATA *obj, CHAR_DATA *ch)
{
	if (check_name(obj, ch)
		&& check_type(obj)
		&& check_state(obj)
		&& check_wear(obj)
		&& check_weap_class(obj)
		&& check_cost(GET_OBJ_COST(obj))
		&& check_affect_apply(obj)
		&& check_affect_weap(obj)
		&& check_affect_extra(obj))
	{
		return true;
	}
	return false;
}

bool ParseFilter::check(exchange_item_data *exch_obj)
{
	OBJ_DATA *obj = GET_EXCHANGE_ITEM(exch_obj);
	if (check_name(obj)
		&& check_owner(exch_obj)
		//&& (owner_id == -1 || owner_id == GET_EXCHANGE_ITEM_SELLERID(exch_obj))
		&& check_type(obj)
		&& check_state(obj)
		&& check_wear(obj)
		&& check_weap_class(obj)
		&& check_cost(GET_EXCHANGE_ITEM_COST(exch_obj))
		&& check_affect_apply(obj)
		&& check_affect_weap(obj)
		&& check_affect_extra(obj)
		&& check_realtime(exch_obj))
	{
		return true;
	}
	return false;
}

const char *print_obj_state(int tm_pct)
{
	if (tm_pct < 20)
		return "������";
	else if (tm_pct < 40)
		return "����� ����������";
	else if (tm_pct < 60)
		return "���������";
	else if (tm_pct < 80)
		return "������";
	//else if (tm_pct <=100) // � ������ ��� ��������� ������ �������� 100% ������ ���, ������ <=
	//	return "��������";
	else if (tm_pct <1000) // �������� ������, �� ��������� ������ ������ ���������
		return "��������";
	else return "��������";
}

bool isname(const char *str, const char *namelist)
{
	bool once_ok = false;
	const char *curname, *curstr, *laststr;

	if (!namelist || !*namelist || !str)
	{
		return false;
	}

	for (curstr = str; !a_isalnum(*curstr); curstr++)
	{
		if (!*curstr)
		{
			return once_ok;
		}
	}

	laststr = curstr;
	curname = namelist;
	for (;;)
	{
		once_ok = false;
		for (;; curstr++, curname++)
		{
			if (!*curstr)
			{
				return once_ok;
			}

			if (*curstr == '!')
			{
				if (a_isalnum(*curname))
				{
					curstr = laststr;
					break;
				}
			}

			if (!a_isalnum(*curstr))
			{
				for (; !a_isalnum(*curstr); curstr++)
				{
					if (!*curstr)
					{
						return once_ok;
					}
				}
				laststr = curstr;
				break;
			}

			if (!*curname)
			{
				return false;
			}

			if (!a_isalnum(*curname))
			{
				curstr = laststr;
				break;
			}

			if (LOWER(*curstr) != LOWER(*curname))
			{
				curstr = laststr;
				break;
			}
			else
			{
				once_ok = true;
			}
		}

		// skip to next name
		for (; a_isalnum(*curname); curname++);
		for (; !a_isalnum(*curname); curname++)
		{
			if (!*curname)
			{
				return false;
			}
		}
	}
}

const char* one_word(const char* argument, char *first_arg)
{
	char *begin = first_arg;

	skip_spaces(&argument);
	first_arg = begin;

	if (*argument == '\"')
	{
		argument++;
		while (*argument && *argument != '\"')
		{
			*(first_arg++) = a_lcc(*argument);
			argument++;
		}
		argument++;
	}
	else
	{
		while (*argument && !a_isspace(*argument))
		{
			*(first_arg++) = a_lcc(*argument);
			argument++;
		}
	}
	*first_arg = '\0';

	return argument;
}

std::string ParseFilter::print() const
{
	std::string buffer = "�������: ";
	if (!name.empty())
	{
		buffer += name + " ";
	}
	/*
	if (owner_id >= 0)
	{
		const char *name = get_name_by_id(owner_id);
		if (name && name[0] != '\0')
		{
			buffer += name;
			buffer += " ";
		}
	}
	*/
	if (!owner.empty())
	{
		buffer += owner + " ";
	}
	if (type >= 0)
	{
		buffer += item_types[type];
		buffer += " ";
	}
	if (state >= 0)
	{
		buffer += print_obj_state(state);
		buffer += " ";
	}
	if (wear != EWearFlag::ITEM_WEAR_UNDEFINED)
	{
		buffer += wear_bits[wear_message];
		buffer += " ";
	}
	if (weap_class >= 0)
	{
		buffer += weapon_class[weap_message];
		buffer += " ";
	}
	if (cost >= 0)
	{
		sprintf(buf, "%d%c ", cost, cost_sign);
		buffer += buf;
	}
	if (!affect_weap.empty())
	{
		for (auto it = affect_weap.begin(); it != affect_weap.end(); ++it)
		{
			buffer += weapon_affects[*it];
			buffer += " ";
		}
	}
	if (!affect_apply.empty())
	{
		for (auto it = affect_apply.begin(); it != affect_apply.end(); ++it)
		{
			buffer += apply_types[*it];
			buffer += " ";
		}
	}
	if (!affect_extra.empty())
	{
		for (auto it = affect_extra.begin(); it != affect_extra.end(); ++it)
		{
			buffer += extra_bits[*it];
			buffer += " ";
		}
	}
	buffer += "\r\n";

	return buffer;
}

const char a_lcc_table[] = {
	'\x00', '\x01', '\x02', '\x03', '\x04', '\x05', '\x06', '\x07', '\x08', '\x09', '\x0a', '\x0b', '\x0c', '\x0d', '\x0e', '\x0f',
	'\x10', '\x11', '\x12', '\x13', '\x14', '\x15', '\x16', '\x17', '\x18', '\x19', '\x1a', '\x1b', '\x1c', '\x1d', '\x1e', '\x1f',
	'\x20', '\x21', '\x22', '\x23', '\x24', '\x25', '\x26', '\x27', '\x28', '\x29', '\x2a', '\x2b', '\x2c', '\x2d', '\x2e', '\x2f',
	'\x30', '\x31', '\x32', '\x33', '\x34', '\x35', '\x36', '\x37', '\x38', '\x39', '\x3a', '\x3b', '\x3c', '\x3d', '\x3e', '\x3f',
	'\x40', '\x61', '\x62', '\x63', '\x64', '\x65', '\x66', '\x67', '\x68', '\x69', '\x6a', '\x6b', '\x6c', '\x6d', '\x6e', '\x6f',
	'\x70', '\x71', '\x72', '\x73', '\x74', '\x75', '\x76', '\x77', '\x78', '\x79', '\x7a', '\x5b', '\x5c', '\x5d', '\x5e', '\x5f',
	'\x60', '\x61', '\x62', '\x63', '\x64', '\x65', '\x66', '\x67', '\x68', '\x69', '\x6a', '\x6b', '\x6c', '\x6d', '\x6e', '\x6f',
	'\x70', '\x71', '\x72', '\x73', '\x74', '\x75', '\x76', '\x77', '\x78', '\x79', '\x7a', '\x7b', '\x7c', '\x7d', '\x7e', '\x7f',
	'\x80', '\x81', '\x82', '\x83', '\x84', '\x85', '\x86', '\x87', '\x88', '\x89', '\x8a', '\x8b', '\x8c', '\x8d', '\x8e', '\x8f',
	'\x90', '\x91', '\x92', '\x93', '\x94', '\x95', '\x96', '\x97', '\x98', '\x99', '\x9a', '\x9b', '\x9c', '\x9d', '\x9e', '\x9f',
	'\xa0', '\xa1', '\xa2', '\xa3', '\xa4', '\xa5', '\xa6', '\xa7', '\xa8', '\xa9', '\xaa', '\xab', '\xac', '\xad', '\xae', '\xaf',
	'\xb0', '\xb1', '\xb2', '\xa3', '\xb4', '\xb5', '\xb6', '\xb7', '\xb8', '\xb9', '\xba', '\xbb', '\xbc', '\xbd', '\xbe', '\xbf',
	'\xc0', '\xc1', '\xc2', '\xc3', '\xc4', '\xc5', '\xc6', '\xc7', '\xc8', '\xc9', '\xca', '\xcb', '\xcc', '\xcd', '\xce', '\xcf',
	'\xd0', '\xd1', '\xd2', '\xd3', '\xd4', '\xd5', '\xd6', '\xd7', '\xd8', '\xd9', '\xda', '\xdb', '\xdc', '\xdd', '\xde', '\xdf',
	'\xc0', '\xc1', '\xc2', '\xc3', '\xc4', '\xc5', '\xc6', '\xc7', '\xc8', '\xc9', '\xca', '\xcb', '\xcc', '\xcd', '\xce', '\xcf',
	'\xd0', '\xd1', '\xd2', '\xd3', '\xd4', '\xd5', '\xd6', '\xd7', '\xd8', '\xd9', '\xda', '\xdb', '\xdc', '\xdd', '\xde', '\xdf'
};

const bool a_isalnum_table[] = {
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	 true,  true,  true,  true,  true,  true,  true,  true,  true,  true, false, false, false, false, false, false,
	false,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,
	 true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true, false, false, false, false, false,
	false,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,
	 true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false,  true, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false,  true, false, false, false, false, false, false, false, false, false, false, false, false,
	 true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,
	 true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,
	 true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,
	 true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true
};

const bool a_isdigit_table[256] = {
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	 true,  true,  true,  true,  true,  true,  true,  true,  true,  true, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false
};

#include <iostream>

void build_char_table(int(*func)(int c))
{
	for (int c = 0; c < 256; c++)
	{
		std::cout << (func(c) ? " true" : "false") << (255 > c ? ", " : "");
		if (0 < c && 0 == (1 + c) % 16)
		{
			std::cout << std::endl;
		}
	}
}

#ifdef WIN32
bool CCheckTable::test_values() const
{
	unsigned i = 0;
	do
	{
		if ((0 != m_original(i % 256)) != m_table(i % 256))
		{
			std::cout << static_cast<unsigned>(i % 256) << ": " << m_original(i % 256) << "; " << m_table(i % 256);
			return false;
		}
		++i;
	} while (0 < i);

	return true;
}

double CCheckTable::test_time() const
{
	class CMeasurer
	{
	public:
		CMeasurer(DWORD& duration) : m_start(GetTickCount()), m_output(duration) {}
		~CMeasurer() { m_output = GetTickCount() - m_start; }

	private:
		DWORD m_start;
		DWORD& m_output;
	};

	DWORD original_time = 0;
	{
		CMeasurer m(original_time);
		unsigned i = 0;
		do
		{
			bool result = 0 != m_original(i % 256);
			++i;
		} while (0 < i);
	}

	DWORD table_time = 0;
	{
		CMeasurer m(table_time);
		unsigned i = 0;
		do
		{
			m_table(i % 256);
			++i;
		} while (0 < i);
	}

	return static_cast<long double>(original_time) / table_time;
}

void CCheckTable::check() const
{
	std::cout << "Validity... " << std::endl;
	std::cout << (test_values() ? "passed" : "failed") << std::endl;
	std::cout << "Performance... " << std::endl;
	std::cout << std::setprecision(2) << std::fixed << test_time() * 100 << "%" << std::endl;
}
#endif	// WIN32

#ifdef HAVE_ICONV
void koi_to_utf8(char *str_i, char *str_o)
{
	iconv_t cd;
	size_t len_i, len_o = MAX_SOCK_BUF * 6;
	size_t i;

	if ((cd = iconv_open("UTF-8","KOI8-R")) == (iconv_t) - 1)
	{
		printf("koi_to_utf8: iconv_open error\n");
		return;
	}
	len_i = strlen(str_i);
	// const_cast at the next line is required for Linux, because there iconv has non-const input argument.
	if ((i = iconv(cd, &str_i, &len_i, &str_o, &len_o)) == (size_t) - 1)
	{
		printf("koi_to_utf8: iconv error\n");
		return;
	}
	*str_o = 0;
	if (iconv_close(cd) == -1)
	{
		printf("koi_to_utf8: iconv_close error\n");
		return;
	}
}

void utf8_to_koi(char *str_i, char *str_o)
{
	iconv_t cd;
	size_t len_i, len_o = MAX_SOCK_BUF * 6;
	size_t i;

	if ((cd = iconv_open("KOI8-R", "UTF-8")) == (iconv_t) - 1)
	{
		perror("utf8_to_koi: iconv_open error");
		return;
	}
	len_i = strlen(str_i);
	if ((i=iconv(cd, &str_i, &len_i, &str_o, &len_o)) == (size_t) - 1)
	{
		perror("utf8_to_koi: iconv error");
	}
	if (iconv_close(cd) == -1)
	{
		perror("utf8_to_koi: iconv_close error");
		return;
	}
}
#endif // HAVE_ICONV

// vim: ts=4 sw=4 tw=0 noet syntax=cpp :
