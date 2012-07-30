// $RCSfile$     $Date$     $Revision$
// Copyright (c) 2009 Krodo
// Part of Bylins http://www.mud.ru

#include <sstream>
#include <string>
#include <boost/format.hpp>
#include "conf.h"
#include "sysdep.h"
#include "structs.h"
#include "spells.h"
#include "utils.h"
#include "comm.h"
#include "screen.h"
#include "poison.hpp"
#include "char.hpp"
#include "db.h"
#include "room.hpp"

/*
������� ���������:
����� ���-�� ������� ��� ����� �� ������ - ����� � mag_alter_objs()
���� ���-�� ������� ��� ������ ������� - check_spell_remove()
���� ���� ���������� ������ - ������ ������ �� -1 � timed_spell.add()
���� ��������� ���� �� ���� �� ������ - timed_spell.check_spell(spell_num)

*/

////////////////////////////////////////////////////////////////////////////////
namespace
{

/**
 * �������� ���� �� ���-�� ������ �� ������� ��� ������ ����
 * ��� ������ ���������� �� ������.
 */
void check_spell_remove(OBJ_DATA *obj, int spell, bool send_message)
{
	if (!obj)
	{
		log("SYSERROR: NULL object %s:%d, spell = %d",
			__FILE__, __LINE__, spell);
		return;
	}

	// ���� ���-�� ���� ������� �� ������� ��� ������ �������
	switch (spell)
	{
	case SPELL_ACONITUM_POISON:
	case SPELL_SCOPOLIA_POISON:
	case SPELL_BELENA_POISON:
	case SPELL_DATURA_POISON:
		break;
	case SPELL_FLY:
	{
		const OBJ_DATA * const proto = read_object_mirror(GET_OBJ_VNUM(obj));
		if (!OBJ_FLAGGED(proto, ITEM_FLYING))
		{
			REMOVE_BIT(GET_OBJ_EXTRA(obj, ITEM_FLYING), ITEM_FLYING);
		}
		if (!OBJ_FLAGGED(proto, ITEM_SWIMMING))
		{
			REMOVE_BIT(GET_OBJ_EXTRA(obj, ITEM_SWIMMING), ITEM_SWIMMING);
		}
		break;
	}
	} // switch

	// ������ ����������� ����
	if (send_message && (obj->carried_by || obj->worn_by))
	{
		CHAR_DATA *ch = obj->carried_by ? obj->carried_by : obj->worn_by;
		switch (spell)
		{
		case SPELL_ACONITUM_POISON:
		case SPELL_SCOPOLIA_POISON:
		case SPELL_BELENA_POISON:
		case SPELL_DATURA_POISON:
			send_to_char(ch, "� %s ���������� ��������� �������� ���.\r\n",
					GET_OBJ_PNAME(obj, 1));
			break;
		case SPELL_FLY:
			send_to_char(ch, "���%s %s ��������%s ������ � �������.\r\n",
					GET_OBJ_VIS_SUF_7(obj, ch), GET_OBJ_PNAME(obj, 0),
					GET_OBJ_VIS_SUF_1(obj, ch));
			break;
		}
	}

}

/**
 * ���������� ������ � ����������� � �������� ��� ������� ������.
 */
std::string print_spell_str(CHAR_DATA *ch, int spell, int timer)
{
	if (spell < 0 || spell >= LAST_USED_SPELL)
	{
		log("SYSERROR: %s, spell = %d, time = %d", __func__, spell, timer);
		return "";
	}

	std::string out;
	switch (spell)
	{
	case SPELL_ACONITUM_POISON:
	case SPELL_SCOPOLIA_POISON:
	case SPELL_BELENA_POISON:
	case SPELL_DATURA_POISON:
		out = boost::str(boost::format(
			"%1%��������� %2% ��� %3% %4%.%5%\r\n")
			% CCGRN(ch, C_NRM) % get_poison_by_spell(spell) % timer
			% desc_count(timer, WHAT_MINu) % CCNRM(ch, C_NRM));
		break;
	default:
	{
		if (timer == -1)
		{
			out = boost::str(boost::format(
				"%1%�������� ���������� ���������� '%2%'.%3%\r\n")
				% CCCYN(ch, C_NRM)
				% (spell_info[spell].name ? spell_info[spell].name : "<null>")
				% CCNRM(ch, C_NRM));
		}
		else
		{
			out = boost::str(boost::format(
				"%1%�������� ���������� '%2%' (%3%).%4%\r\n")
				% CCCYN(ch, C_NRM)
				% (spell_info[spell].name ? spell_info[spell].name : "<null>")
				% time_format(timer, true)
				% CCNRM(ch, C_NRM));
		}
		break;
	}
	}
	return out;
}

} // namespace
////////////////////////////////////////////////////////////////////////////////

/**
 * �������� ���������� �� ������ � ��������� �� ��������/���������
 * ��� ������ �������.
 */
void TimedSpell::remove_spell(OBJ_DATA *obj, int spell, bool message)
{
	std::map<int, int>::iterator i = spell_list_.find(spell);
	if (i != spell_list_.end())
	{
		check_spell_remove(obj, spell, message);
		spell_list_.erase(i);
	}
}

/**
* ��� ���.����� � �������� �� ������.
* \param time = -1 ��� ����������� �������
*/
void TimedSpell::add(OBJ_DATA *obj, int spell, int time)
{
	if (!obj || spell < 0 || spell >= LAST_USED_SPELL)
	{
		log("SYSERROR: func: %s, obj = %s, spell = %d, time = %d",
			__func__, obj ? "true" : "false", spell, time);
		return;
	}

	// ��������� ���� ���� ������
	if (spell == SPELL_ACONITUM_POISON
		|| spell == SPELL_SCOPOLIA_POISON
		|| spell == SPELL_BELENA_POISON
		|| spell == SPELL_DATURA_POISON)
	{
		remove_spell(obj, SPELL_ACONITUM_POISON, false);
		remove_spell(obj, SPELL_SCOPOLIA_POISON, false);
		remove_spell(obj, SPELL_BELENA_POISON, false);
		remove_spell(obj, SPELL_DATURA_POISON, false);
	}

	spell_list_[spell] = time;
}

/**
* ����� ����������� ������� ��� �� ����� ��� �������.
*/
std::string TimedSpell::diag_to_char(CHAR_DATA *ch)
{
	if (spell_list_.empty())
	{
		return "";
	}

	std::string out;
	for(std::map<int, int>::iterator i = spell_list_.begin(),
		iend = spell_list_.end(); i != iend; ++i)
	{
		out += print_spell_str(ch, i->first, i->second);
	}
	return out;
}

/**
 * �������� �� ������ ������ ����� ����� ���.
 * \return -1 ���� ��� ���, spell_num ���� ����.
 */
int TimedSpell::is_spell_poisoned() const
{
	for(std::map<int, int>::const_iterator i = spell_list_.begin(),
		iend = spell_list_.end(); i != iend; ++i)
	{
		if (check_poison(i->first))
		{
			return i->first;
		}
	}
	return -1;
}

/**
* ��� ����� �������.
*/
bool TimedSpell::empty() const
{
	return spell_list_.empty();
}

/**
* ���������� ������ � ����.
*/
std::string TimedSpell::print() const
{
	std::stringstream out;

	out << "TSpl: ";
	for(std::map<int, int>::const_iterator i = spell_list_.begin(),
		iend = spell_list_.end(); i != iend; ++i)
	{
		out << i->first << " " << i->second << "\n";
	}
	out << "~\n";

	return out.str();
}

/**
 * ����� ���������� �� spell_num.
 */
bool TimedSpell::check_spell(int spell) const
{
	std::map<int, int>::const_iterator i = spell_list_.find(spell);
	if (i != spell_list_.end())
	{
		return true;
	}
	return false;
}

/**
* ��� ���.������� �� ������ (��� � ������).
* \param time �� ������� = 1.
*/
void TimedSpell::dec_timer(OBJ_DATA *obj, int time)
{
	for(std::map<int, int>::iterator i = spell_list_.begin();
		i != spell_list_.end(); /* empty */)
	{
		if (i->second != -1)
		{
			i->second -= time;
			if (i->second <= 0)
			{
				check_spell_remove(obj, i->first, true);
				spell_list_.erase(i++);
			}
			else
			{
				++i;
			}
		}
		else
		{
			++i;
		}
	}
}
